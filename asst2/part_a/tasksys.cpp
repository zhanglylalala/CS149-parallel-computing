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
    //
    // TODO: DONE
    // CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //

    this -> num_threads = num_threads;
    workers = new std::thread[num_threads];
    mut = new std::mutex();
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() {
    delete[] workers;
    delete mut;
}

void TaskSystemParallelSpawn::thread_run(IRunnable *runnable, int num_total_tasks)
{
    while (current_task < num_total_tasks)
    {
        mut -> lock();
        int taskID = current_task;
        current_task += 1;
        mut -> unlock();
        runnable -> runTask(taskID, num_total_tasks);
    }
}
int cnt_spawn = 0;
void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: DONE
    // CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    int num_threads_spawn = std::min(num_total_tasks, num_threads);
    current_task = 0;
    for (int i = 1; i < num_threads_spawn; i++)
    {
        workers[i] = std::thread(&TaskSystemParallelSpawn::thread_run, this, runnable, num_total_tasks);
    }
    thread_run(runnable, num_total_tasks);
    for (int i = 1; i < num_threads_spawn; i++)
    {
        workers[i].join();
    }
}

TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */

SpinningState::SpinningState()
{
    mut = new std::mutex();
    fin = new std::mutex();
    hasDone = new std::condition_variable();
    current_task = num_total_tasks = task_finished = 0;
    runnable = nullptr;
}

SpinningState::~SpinningState()
{
    delete mut;
    delete fin;
    delete hasDone;
}

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: DONE
    // CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    alive = true;
    state = new SpinningState();
    this -> num_threads = num_threads;
    thread_pool = new std::thread[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        thread_pool[i] = std::thread(&TaskSystemParallelThreadPoolSpinning::thread_run, this);
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    alive = false;
    for (int i = 0; i < num_threads; i++)
    {
        thread_pool[i].join();
    }
    delete[] thread_pool;
    delete state;
}

void TaskSystemParallelThreadPoolSpinning::thread_run()
{
    // FILE *ptr = fopen("./log.txt", "a");
    // fprintf(ptr, "Launching %d ...\n", std::this_thread::get_id());
    // fclose(ptr);

    while (alive)
    {
        state -> mut -> lock();
        int taskID = state -> current_task;
        state -> current_task += 1;
        state -> mut -> unlock();
        
        if (taskID < state -> num_total_tasks)
        {
            state -> runnable -> runTask(taskID, state -> num_total_tasks);

            state -> mut -> lock();
            state -> task_finished += 1;
            if (state -> task_finished >= state -> num_total_tasks)
            {
                state -> mut -> unlock();
                state -> hasDone -> notify_all();
            }
            else
            {
                state -> mut -> unlock();
            }
        }
    }
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: DONE
    // CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    std::unique_lock<std::mutex> lk(*state -> fin);
    state -> mut -> lock();

    state -> runnable = runnable;
    state -> current_task = state -> task_finished = 0;
    state -> num_total_tasks = num_total_tasks;

    state -> mut -> unlock();

    state -> hasDone -> wait(lk);
    lk.unlock();
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

SleepingState::SleepingState()
{
    current_task = num_total_tasks = task_finished = 0;
    runnable = nullptr;
    hasWork = new std::condition_variable();
    hasDone = new std::condition_variable();
    mut = new std::mutex();
    work = new std::mutex();
    fin = new std::mutex();
}

SleepingState::~SleepingState()
{
    delete hasWork;
    delete hasDone;
    delete mut;
    delete work;
    delete fin;

}

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: DONE
    // CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    this -> num_threads = num_threads;
    state = new SleepingState();
    alive = true;
    thread_pool = new std::thread[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        thread_pool[i] = std::thread(&TaskSystemParallelThreadPoolSleeping::thread_run, this);
    }

}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: DONE
    // CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    alive = false;
    for (int i = 0; i < num_threads; i++)
    {
        state -> hasWork -> notify_all();
    }
    for (int i = 0; i < num_threads; i++)
    {
        thread_pool[i].join();
    }
    delete[] thread_pool;
    delete state;
}

void TaskSystemParallelThreadPoolSleeping::thread_run()
{
    while (alive)
    {
        state -> mut -> lock();
        int taskID = state -> current_task;
        state -> current_task += 1;
        state -> mut -> unlock();

        if (taskID < state -> num_total_tasks)
        {
            state -> runnable -> runTask(taskID, state -> num_total_tasks);

            state -> mut -> lock();
            state -> task_finished += 1;
            if (state -> task_finished >= state -> num_total_tasks)
            {
                state -> mut -> unlock();
                state -> hasDone -> notify_all();
            }
            else
            {
                state -> mut -> unlock();
            }
        }
        else
        {
            std::unique_lock<std::mutex> lk(*state -> work);
            state -> hasWork -> wait(lk);
            lk.unlock();
        }

    }
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: DONE
    // CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    state -> mut -> lock();

    state -> runnable = runnable;
    state -> num_total_tasks = num_total_tasks;
    state -> current_task = state -> task_finished = 0;
    
    state -> mut -> unlock();

    for (int i = 0; i < num_threads; i++)
        state -> hasWork -> notify_all();

    std::unique_lock<std::mutex> lk(*state -> fin);
    state -> hasDone -> wait(lk);
    state -> num_total_tasks = state -> current_task = state -> task_finished = 0;
    state -> runnable = nullptr;
    lk.unlock();
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}