#include "page_rank.h"

#include <stdlib.h>
#include <cmath>
#include <omp.h>
#include <utility>

#include "../common/CycleTimer.h"
#include "../common/graph.h"


// pageRank --
//
// g:           graph to process (see common/graph.h)
// solution:    array of per-vertex vertex scores (length of array is num_nodes(g))
// damping:     page-rank algorithm's damping parameter
// convergence: page-rank algorithm's convergence threshold
//
void pageRank(Graph g, double* solution, double damping, double convergence)
{


  // initialize vertex weights to uniform probability. Double
  // precision scores are used to avoid underflow for large graphs

  int numNodes = num_nodes(g);
  double rest = (1.0 - damping) / numNodes;

  double equal_prob = 1.0 / numNodes;
  for (int i = 0; i < numNodes; ++i) {
    solution[i] = equal_prob;
  }
  double* score_new = new double[numNodes];
  int* non_outgoing_nodes = new int[numNodes];
  int non_outgoing_num = 0;
  for (int i = 0; i < numNodes; i++)
  {
    if (outgoing_size(g, i) == 0)
    {
      non_outgoing_nodes[non_outgoing_num++] = i;
    }
  }

  bool converged = false;
  while (!converged)
  {
    double non_outgoing = 0.0;
    #pragma omp parallel for reduction(+:non_outgoing) schedule(dynamic, 200)
    for (int i = 0; i < non_outgoing_num; i++)
    {
      non_outgoing += solution[non_outgoing_nodes[i]];
    }
    non_outgoing = non_outgoing * damping / numNodes;

    double global_diff = 0;
    #pragma omp parallel for schedule(dynamic, 200)
    for (int i = 0; i < numNodes; i++)
    {
      score_new[i] = 0;
      const Vertex* start = incoming_begin(g,  i);
      const Vertex* end = incoming_end(g, i);
      for (const Vertex* v= start; v != end; v++)
      {
        score_new[i] += solution[*v] / outgoing_size(g, *v);
      }
      score_new[i] = (damping * score_new[i]) + rest + non_outgoing;
    }

    #pragma omp parallel for reduction(+:global_diff) schedule(dynamic, 200)
    for (int i = 0; i < numNodes; i++)
    {
      global_diff += abs(solution[i] - score_new[i]);
      solution[i] = score_new[i];
    }
    converged = global_diff < convergence;
  }
  delete score_new;
  delete non_outgoing_nodes;
  
  
  /*
     CS149 students: Implement the page rank algorithm here.  You
     are expected to parallelize the algorithm using openMP.  Your
     solution may need to allocate (and free) temporary arrays.

     Basic page rank pseudocode is provided below to get you started:

     // initialization: see example code above
     score_old[vi] = 1/numNodes;

     while (!converged) {

       // compute score_new[vi] for all nodes vi:
       score_new[vi] = sum over all nodes vj reachable from incoming edges
                          { score_old[vj] / number of edges leaving vj  }
       score_new[vi] = (damping * score_new[vi]) + (1.0-damping) / numNodes;

       score_new[vi] += sum over all nodes v in graph with no outgoing edges
                          { damping * score_old[v] / numNodes }

       // compute how much per-node scores have changed
       // quit once algorithm has converged

       global_diff = sum over all nodes vi { abs(score_new[vi] - score_old[vi]) };
       converged = (global_diff < convergence)
     }

   */
}
