#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <unistd.h>

#include "background_worker.h"

std::pair<int, std::vector<char> > background_worker::get_work() {
    std::unique_lock<std::mutex> ul(mreq);
    while (qreq.size() == 0 && is_working) {
        var_req.wait(ul);
    }
    if (qreq.size() == 0) {
        return std::make_pair(-1, std::vector<char>());
    }
    auto task = qreq.front();
    qreq.pop();
    return task;
}

int background_worker::create_fd() {
    int fd = memfd_create("worker", 0);
    ftruncate(fd, 1);
    return fd;
}

background_worker::background_worker() :
    notify_fd(create_fd()),
    worker_thread([this](){
    while (is_working) {
        auto task = get_work();
        if (is_working) {
            addrinfo* info_result;
            task.second.push_back('\0');
            getaddrinfo(task.second.data(), nullptr, nullptr, &info_result);
            std::vector<char> res;
            for (addrinfo* r = info_result; r != nullptr; r = r->ai_next) {
                char* addr = inet_ntoa(reinterpret_cast<sockaddr_in*>(r->ai_addr)->sin_addr);
                for (int j = 0; addr[j] != '\0'; ++j) {
                    res.push_back(addr[j]);
                }
                res.push_back('\n');
            }
            freeaddrinfo(info_result);
            {
                std::unique_lock<std::mutex> ul(mres);
                qres.emplace(task.first, res);
            }
            char buf[1];
            buf[0] = '0';
        }
    }
})
{}

background_worker::~background_worker() {
    worker_thread.join();
    close(notify_fd);
}
