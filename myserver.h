#pragma once

#include <map>
#include <set>
#include <vector>
#include <sys/epoll.h>
#include <functional>

#include "background_worker.h"

typedef std::function<void(epoll_event&)> epoll_callback;

typedef std::function<bool(int, epoll_callback*, bool, bool)> func_add_fd;
typedef std::function<bool(int, const epoll_callback&, bool, bool)> my_func_add_fd;
typedef std::function<void(int)> func_del_fd;

struct myserver {
    int server_fd;
    my_func_add_fd epoll_add;
    func_del_fd epoll_del;
    epoll_callback new_connect_callback;
    epoll_callback work_done_callback;

    std::map<int, std::vector<char> > connections_new;
    std::map<int, std::vector<char> > connections_send;
    std::map<int, int> timers;
    std::map<int, epoll_callback> callbacks;
    std::set<int> wait_worker;
    background_worker worker;

    myserver(uint16_t, const func_add_fd&, const func_del_fd&);
    ~myserver();

    void close_fd(int);
    void new_connect(int);
    void myread(int);
    void mywrite(int);
    void remove_connection(int);
    void update();
};

int create_server(uint16_t);
