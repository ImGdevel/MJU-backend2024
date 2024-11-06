#ifndef WORKERPOOL
#define WORKERPOOL

#include <condition_variable>
#include <thread>
#include <atomic>
#include <thread>
#include <chrono>
#include <queue>
#include <vector>

class Worker;

class WorkerPool{
public:
    WorkerPool(int thread);
    ~WorkerPool();

    void init();
    void run();
    void quitThreadAll();

    void task();

    void doWork(int task);

private:

    std::condition_variable queFilled;
    std::atomic<bool> quit;
    std::queue<int> taskQueue;
    std::vector<std::thread> threadPool;

};

#endif