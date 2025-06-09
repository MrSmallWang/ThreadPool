#pragma once

#include <iostream>
#include <atomic>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <type_traits>
#include <vector>

namespace ThreadPoolManual {

enum class PoolMode
{
    MODE_CACHED,
    MODE_FIXED
};

class Result;

class Task
{};

class Thread
{};

//线程池类->应该不是模板类吧
class ThreadPool
{
public:
    ThreadPool() = default;
    ~ThreadPool() = default;

    void setMode(PoolMode mode);
    void setMaxThreadNum(int number);
    Result submitTask(std::shared_ptr<Task> task);
    void start(int initThreadSize = 4);

    // 禁用拷贝构造函数 禁用拷贝赋值
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator = (const ThreadPool&) = delete;

private:
    void threadFunc();
    bool checkRunningState();

private:
    std::vector<std::unique_ptr<Thread>> threads_;
    int initThreadSize_;
    std::atomic_int curThreadSize_;
    int idleThreadSize_;

    std::queue<std::shared_ptr<Task>> taskQueue_;
    std::atomic_int taskQueSize_;
    int taskQueMaxThreshold_;
    
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    std::mutex taskQueueMtx_;
    // TODO
    std::atomic_bool isPoolRunning_;
    PoolMode poolMode_;
};

}