#include <unistd.h>
#include <sys/epoll.h>

#include <csignal>
#include <set>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <list>

#include "all_consts.h"
#include "myserver.h"

bool epoll_add(int fd, int epoll_fd, epoll_callback* callback, bool is_in, bool is_out) {
    epoll_event ev;
    ev.data.ptr = reinterpret_cast<void*>(callback);
    ev.events = 0;
    if (is_in) {
        ev.events |= EPOLLIN;
    }
    if (is_out) {
        ev.events |= EPOLLOUT;
    }
    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == 0;
}

void epoll_del(int fd, int epoll_fd) {
    epoll_event trash;
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &trash);
}

namespace
{
    volatile std::sig_atomic_t is_working;
}

void signal_handler(int signal)
{
    is_working = signal;
}

int main()
{
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::vector<uint16_t> ports = {2100, 2101, 2102};
    int epoll_fd = epoll_create(EPOLL_SIZE);
    {
        std::list<myserver> servers;
        for (auto& port : ports) {
            servers.emplace_back(port, [epoll_fd](int conn_fd, epoll_callback* callback,
                                 bool is_in, bool is_out) {
                return epoll_add(conn_fd, epoll_fd, callback, is_in, is_out);
            }, [epoll_fd](int conn_fd) {
                epoll_del(conn_fd, epoll_fd);
            });
        }
        while (is_working == 0) {
            epoll_event events[EVENTS_SIZE];
            int events_now = epoll_wait(epoll_fd, events, static_cast<int>(EVENTS_SIZE), static_cast<int>(EPOLL_TIMEOUT));
            if (events_now != -1) {
                for (size_t i = 0; i < events_now; ++i) {
                    (*reinterpret_cast<epoll_callback*>(events[i].data.ptr))(events[i]);
                }
            }
            for (auto& server : servers) {
                server.update();
            }
        }
    }
    close(epoll_fd);
    return 0;
}
