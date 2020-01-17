#pragma once

#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>

struct background_worker {
    std::mutex mreq;
    std::condition_variable var_req;
    std::mutex mres;
    std::queue<std::pair<int, std::vector<char> > > qreq;
    std::queue<std::pair<int, std::vector<char> > > qres;
    int notify_fd;
    bool is_working = true;
    std::thread worker_thread;

    int create_fd();
    std::pair<int, std::vector<char> > get_work();
    background_worker();
    ~background_worker();
};
