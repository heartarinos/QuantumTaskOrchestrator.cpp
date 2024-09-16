#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <atomic>
#include <future>
#include <mutex>
#include <condition_variable>
#include <chrono>

class TaskScheduler {
public:
    TaskScheduler(size_t num_threads) : stop(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this] {
                            return this->stop || !this->tasks.empty();
                        });
                        if (this->stop && this->tasks.empty()) {
                            return;
                        }
                        task = std::move(this->tasks.top().second);
                        this->tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    template<class F>
    auto enqueue(F&& f, int priority = 0) -> std::future<decltype(f())> {
        using return_type = decltype(f());
        auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (stop) {
                throw std::runtime_error("enqueue on stopped TaskScheduler");
            }
            tasks.emplace(priority, [task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }

    ~TaskScheduler() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread &worker : workers) {
            worker.join();
        }
    }

private:
    std::vector<std::thread> workers;
    std::priority_queue<std::pair<int, std::function<void()>>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
};

class NetworkOperation {
public:
    void perform() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
};

class DataProcessing {
public:
    void process() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
};

class ReportGeneration {
public:
    void generate() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
};

int main() {
    TaskScheduler scheduler(4);

    auto future1 = scheduler.enqueue([] {
        NetworkOperation op;
        op.perform();
        return "Network Operation Complete";
    }, 1);

    auto future2 = scheduler.enqueue([] {
        DataProcessing dp;
        dp.process();
        return "Data Processing Complete";
    }, 2);

    auto future3 = scheduler.enqueue([] {
        ReportGeneration rg;
        rg.generate();
        return "Report Generation Complete";
    }, 3);

    std::cout << future1.get() << std::endl;
    std::cout << future2.get() << std::endl;
    std::cout << future3.get() << std::endl;

    return 0;
}
