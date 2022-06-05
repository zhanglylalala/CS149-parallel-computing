#include "tasksys.h"


IRunnable::~IRunnable() {}

ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}

/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */

const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemSerial::sync() {
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {}

void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }

    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // NOTE: CS149 students are not expected to implement TaskSystemParallelSpawn in Part B.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    alive = true;
    current_work = 0;
    while (!ready.empty())
    {
        ready.pop();
    }
    waiting.clear();
    work = new std::mutex();
    mut = new std::mutex();
    done = new std::mutex();
    hasWork = new std::condition_variable();
    hasDone = new std::condition_variable();
    this -> num_threads = num_threads;
    
    thread_pool = new std::thread[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        thread_pool[i] = std::thread(&TaskSystemParallelThreadPoolSleeping::thread_run, this);
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    alive = false;
    for (int i = 0; i < num_threads; i++)
    {
        hasWork -> notify_all();
        thread_pool[i].join();
    }
    delete work;
    delete mut;
    delete[] thread_pool;
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    std::vector<TaskID> nodeps;
    runAsyncWithDeps(runnable, num_total_tasks, nodeps);
    sync();
}

TaskState::TaskState(IRunnable *runnable, int num_total_tasks, const std::vector<TaskID> &deps, TaskID id)
{
    this -> runnable = runnable;
    this -> num_total_task = num_total_tasks;
    this -> deps = deps;
    num_waiting = deps.size();
    this -> id = id;
    current_task = task_finished = 0;
    mut = new std::mutex();
}

TaskState::~TaskState()
{
    delete mut;
}

void TaskSystemParallelThreadPoolSleeping::thread_run()
{
    while (alive)
    {
        mut -> lock();
        
        if (!ready.empty())
        {
            TaskState *task = ready.front();
            mut -> unlock();

            task -> mut -> lock();
            int taskID = task -> current_task;
            task -> current_task += 1;
            task -> mut -> unlock();

            if (taskID < task -> num_total_task)
            {
                task -> runnable -> runTask(taskID, task -> num_total_task);

                task -> mut -> lock();
                task -> task_finished += 1;
                if (task -> task_finished >= task -> num_total_task)
                {
                    task -> mut -> unlock();
                    
                    mut -> lock();
                    flag[task -> id] = true;
                    current_work--;
                    if (!current_work)
                    {
                        mut -> unlock();
                        hasDone -> notify_all();
                    }
                    else
                    {
                        mut -> unlock();
                    }
                    for (int i = 0; i < deps_structure[task -> id].size(); i++)
                    {
                        TaskState *tmp = waiting[deps_structure[task -> id][i]];
                        tmp -> mut -> lock();
                        tmp -> num_waiting--;
                        if (tmp -> num_waiting == 0)
                        {
                            tmp -> mut -> unlock();
                            mut -> lock();
                            ready.push(tmp);
                            mut -> unlock();
                        }
                        else
                        {
                            tmp -> mut -> unlock();
                        }
                    }
                }
                else
                {
                    task -> mut -> unlock();
                }
            }
            else
            {
                mut -> lock();
                if (!ready.empty() && ready.front() != nullptr && ready.front() -> id == task -> id)
                {
                    ready.pop();
                }
                mut -> unlock();
            }

        }
        else
        {
            mut -> unlock();
            std::unique_lock<std::mutex> lk(*work);
            hasWork -> wait(lk);
            lk.unlock();
        }
    }

}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //
    TaskID id = flag.size();
    TaskState *task = new TaskState(runnable, num_total_tasks, deps, id);

    flag.push_back(false);
    waiting.push_back(task);
    deps_structure.push_back(std::vector<TaskID>());
    current_work++;
    for (int i = 0; i < deps.size(); i++)
    {
        if (deps[i] < id && flag[deps[i]])
        {
            task -> num_waiting--;
        }
        else
        {
            mut -> lock();
            deps_structure[deps[i]].push_back(id);
            mut -> unlock();
        }
    }
    if (task -> num_waiting == 0)
    {
        mut -> lock();
        ready.push(task);
        mut -> unlock();
        hasWork -> notify_all();
    }

    return id;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //
    // while (current_work)
    {
        std::unique_lock<std::mutex> lk(*done);
        hasDone -> wait(lk);
        lk.unlock();
    }

    return;
}
