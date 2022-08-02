#include "bfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstddef>
#include <omp.h>

#include "../common/CycleTimer.h"
#include "../common/graph.h"

#define ROOT_NODE_ID 0
#define NOT_VISITED_MARKER -1

// #define VERBOSE true

void vertex_set_clear(vertex_set* list) {
    list->count = 0;
}

void vertex_set_init(vertex_set* list, int count) {
    list->max_vertices = count;
    list->vertices = (int*)malloc(sizeof(int) * list->max_vertices);
    vertex_set_clear(list);
}

// Take one step of "top-down" BFS.  For each vertex on the frontier,
// follow all outgoing edges, and add all neighboring vertices to the
// new_frontier.
// Complexity: O(frontier->count x outgoing) or O(sum(outgoing) over frontier)
void top_down_step(
    Graph g,
    vertex_set* frontier,
    vertex_set* new_frontier,
    int* distances)
{
    #pragma omp parallel
    {
        int *local_nodes = (int*)malloc(sizeof(int) * g->num_nodes);
        int local_count = 0;
        #pragma omp for
        for (int i=0; i<frontier->count; i++) {

            int node = frontier->vertices[i];

            int start_edge = g->outgoing_starts[node];
            int end_edge = (node == g->num_nodes - 1)
                            ? g->num_edges
                            : g->outgoing_starts[node + 1];

            // attempt to add all neighbors to the new frontier
            
            for (int neighbor=start_edge; neighbor<end_edge; neighbor++) {
                int outgoing = g->outgoing_edges[neighbor];

                if (distances[outgoing] == NOT_VISITED_MARKER && 
                    __sync_bool_compare_and_swap(&distances[outgoing], NOT_VISITED_MARKER, distances[node] + 1))
                {
                    local_nodes[local_count++] = outgoing;
                }
            }
        }
        int index;
        while (true)
        {
            index = new_frontier->count;
            if (__sync_bool_compare_and_swap(&new_frontier->count, index, index + local_count))
            {
                break;
            }
        }
        for (int i = 0; i < local_count; i++)
        {
            new_frontier->vertices[index + i] = local_nodes[i];
        }
        free(local_nodes);
    }
}

// Implements top-down BFS.
//
// Result of execution is that, for each node in the graph, the
// distance to the root is stored in sol.distances.
void bfs_top_down(Graph graph, solution* sol) {

    vertex_set list1;
    vertex_set list2;
    vertex_set_init(&list1, graph->num_nodes);
    vertex_set_init(&list2, graph->num_nodes);

    vertex_set* frontier = &list1;
    vertex_set* new_frontier = &list2;

    // initialize all nodes to NOT_VISITED
    #pragma omp parallel for schedule(dynamic, 200)
    for (int i=0; i<graph->num_nodes; i++)
        sol->distances[i] = NOT_VISITED_MARKER;

    // setup frontier with the root node
    frontier->vertices[frontier->count++] = ROOT_NODE_ID;
    sol->distances[ROOT_NODE_ID] = 0;

    while (frontier->count != 0) {

#ifdef VERBOSE
        double start_time = CycleTimer::currentSeconds();
#endif

        vertex_set_clear(new_frontier);

        top_down_step(graph, frontier, new_frontier, sol->distances);

#ifdef VERBOSE
    double end_time = CycleTimer::currentSeconds();
    printf("frontier=%-10d %.4f sec\n", frontier->count, end_time - start_time);
#endif

        // swap pointers
        vertex_set* tmp = frontier;
        frontier = new_frontier;
        new_frontier = tmp;
    }
}

// Complexity: O(incoming x num_nodes)
int bottom_up_step(Graph graph, bool* isInFrantier, bool* newFrantier, int* distances)
{
    int frantierCount = 0;
    #pragma omp parallel reduction(+ : frantierCount)
    {
        int localCount = 0;
        #pragma omp for schedule(dynamic, 200)
        for (int i = 0; i < graph->num_nodes; i++)
        {
            newFrantier[i] = false;
            if (distances[i] == NOT_VISITED_MARKER)
            {
                int start_edge = graph->incoming_starts[i];
                int end_edge = (i == graph->num_nodes - 1) ? graph->num_edges : graph->incoming_starts[i + 1];
                for (int j = start_edge; j < end_edge; j++)
                {
                    int incoming = graph->incoming_edges[j];
                    if (isInFrantier[incoming])
                    {
                        distances[i] = distances[incoming] + 1;
                        localCount++;
                        newFrantier[i] = true;
                        break;
                    }
                }
            }
        }
        frantierCount += localCount;
    }
    return frantierCount;
}

void bfs_bottom_up(Graph graph, solution* sol)
{
    // CS149 students:
    //
    // You will need to implement the "bottom up" BFS here as
    // described in the handout.
    //
    // As a result of your code's execution, sol.distances should be
    // correctly populated for all nodes in the graph.
    //
    // As was done in the top-down case, you may wish to organize your
    // code by creating subroutine bottom_up_step() that is called in
    // each step of the BFS process.
    bool *isInFrantier = (bool*)malloc(sizeof(bool) * graph->num_nodes);
    bool *newFrantier = (bool*)malloc(sizeof(bool) * graph->num_nodes);

    #pragma omp parallel for schedule(dynamic, 200)
    for (int i = 0; i < graph->num_nodes; i++)
    {
        sol->distances[i] = NOT_VISITED_MARKER;
        isInFrantier[i] = false;
    }

    isInFrantier[ROOT_NODE_ID] = true;
    sol->distances[ROOT_NODE_ID] = 0;
    int frantierCount = 1;
    
    while (frantierCount != 0)
    {
        frantierCount = bottom_up_step(graph, isInFrantier, newFrantier, sol->distances);
        
        bool* tmp = newFrantier;
        newFrantier = isInFrantier;
        isInFrantier = tmp;
    }
}

struct edgeCounter
{
    int in, out;

    edgeCounter(int iin, int iout)
    {
        in = iin;
        out = iout;
    }
};

edgeCounter top_down_step_hybrid(Graph g, vertex_set* frontier, vertex_set* new_frontier,
                          bool* newFrantier_bool, int* distances)
{
    int in = 0, out = 0;
    #pragma omp parallel reduction(+ : in, out)
    {
        int *local_nodes = (int*)malloc(sizeof(int) * g->num_nodes);
        int local_count = 0;
        int localIn = 0, localOut = 0;
        #pragma omp for schedule(dynamic, 200)
        for (int i=0; i<frontier->count; i++) {

            int node = frontier->vertices[i];

            int start_edge = g->outgoing_starts[node];
            int end_edge = (node == g->num_nodes - 1)
                            ? g->num_edges
                            : g->outgoing_starts[node + 1];
            
            for (int neighbor=start_edge; neighbor<end_edge; neighbor++) {
                int outgoing = g->outgoing_edges[neighbor];

                if (distances[outgoing] == NOT_VISITED_MARKER && 
                     __sync_bool_compare_and_swap(&distances[outgoing], NOT_VISITED_MARKER, distances[node] + 1))
                {
                    local_nodes[local_count++] = outgoing;
                    newFrantier_bool[outgoing] = true;
                    int end_in = (outgoing == g->num_nodes - 1) ? g->num_edges : g->incoming_starts[outgoing+1];
                    localIn += end_in - g->incoming_starts[outgoing];
                    localOut += end_edge - start_edge;
                }
            }
        }
        in += localIn;
        out += localOut;
        int index = __sync_fetch_and_add(&new_frontier->count, local_count);
        memcpy(new_frontier->vertices + index, local_nodes, sizeof(int) * local_count);
        free(local_nodes);
    }
    return edgeCounter(in, out);
}

// Complexity: O(incoming x num_nodes)
edgeCounter bottom_up_step_hybrid(Graph graph, bool* isInFrantier, bool* newFrantier,
                           vertex_set* newFrantier_vertex, int* distances)
{
    int in = 0, out = 0;
    #pragma omp parallel reduction(+ : in, out)
    {
        int* local_nodes = (int*)malloc(sizeof(int) * graph->num_nodes);
        int localCount = 0;
        int localIn = 0, localOut = 0;
        #pragma omp for schedule(dynamic, 200)
        for (int i = 0; i < graph->num_nodes; i++)
        {
            if (distances[i] == NOT_VISITED_MARKER)
            {
                int start_edge = graph->incoming_starts[i];
                int end_edge = (i == graph->num_nodes - 1) ? graph->num_edges : graph->incoming_starts[i + 1];
                for (int j = start_edge; j < end_edge; j++)
                {
                    int incoming = graph->incoming_edges[j];
                    if (isInFrantier[incoming])
                    {
                        distances[i] = distances[incoming] + 1;
                        local_nodes[localCount++] = i;
                        newFrantier[i] = true;
                        localIn += end_edge - start_edge;
                        int endOut = (incoming == graph->num_nodes-1) ? graph->num_edges : graph->outgoing_starts[incoming+1];
                        localOut += endOut - graph->outgoing_starts[incoming];
                        break;
                    }
                }
            }
        }
        in += localIn;
        out += localOut;
        int index = __sync_fetch_and_add(&newFrantier_vertex->count, localCount);
        memcpy(newFrantier_vertex->vertices + index, local_nodes, sizeof(int) * localCount);
        free(local_nodes);
    }
    return edgeCounter(in, out);
}

void bfs_hybrid(Graph graph, solution* sol)
{
    // CS149 students:
    //
    // You will need to implement the "hybrid" BFS here as
    // described in the handout.
    vertex_set list1;
    vertex_set list2;
    vertex_set_init(&list1, graph->num_nodes);
    vertex_set_init(&list2, graph->num_nodes);
    vertex_set* frantier_vertex = &list1;
    vertex_set* newFrantier_vertex = &list2;

    bool* frantier_bool = (bool*)malloc(sizeof(bool) * graph->num_nodes);
    bool* newFrantier_bool = (bool*)malloc(sizeof(bool) * graph->num_nodes);

    #pragma omp parallel for schedule(dynamic, 200)
    for (int i = 0; i < graph->num_nodes; i++)
    {
        sol->distances[i] = NOT_VISITED_MARKER;
        frantier_bool[i] = false;
    }

    sol->distances[ROOT_NODE_ID] = 0;
    frantier_bool[ROOT_NODE_ID] = true;
    frantier_vertex->vertices[frantier_vertex->count++] = ROOT_NODE_ID;

    int rootIn = ((ROOT_NODE_ID == graph->num_nodes - 1) ? graph->num_edges : graph->incoming_starts[ROOT_NODE_ID+1]) - graph->incoming_starts[ROOT_NODE_ID];
    int bu_num = graph->num_edges - rootIn;
    int td_num = ((ROOT_NODE_ID == graph->num_nodes-1) ? graph->num_edges : graph->outgoing_starts[ROOT_NODE_ID+1]) - graph->outgoing_starts[ROOT_NODE_ID];

    while (frantier_vertex -> count != 0)
    {
        vertex_set_clear(newFrantier_vertex);
        memset(newFrantier_bool, false, sizeof(newFrantier_bool));
        edgeCounter e = edgeCounter(0, 0);
        if (bu_num <= 1.5 * td_num)
        {
            // bottom-up
            e = bottom_up_step_hybrid(graph, frantier_bool, newFrantier_bool,
                                  newFrantier_vertex, sol->distances);
        }
        else
        {
            // top-down
            e = top_down_step_hybrid(graph, frantier_vertex, newFrantier_vertex, 
                                 newFrantier_bool, sol->distances);
        }
        bu_num -= e.in;
        td_num = e.out;
        
        vertex_set* tmp_vertex = newFrantier_vertex;
        newFrantier_vertex = frantier_vertex;
        frantier_vertex = tmp_vertex;

        bool* tmp_bool = newFrantier_bool;
        newFrantier_bool = frantier_bool;
        frantier_bool = tmp_bool;
    }
}
