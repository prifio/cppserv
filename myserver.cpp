#include <unistd.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <iostream>

#include "myserver.h"
#include "all_consts.h"

myserver::myserver(uint16_t port, const func_add_fd& fn_add, const func_del_fd& fn_del) :
    server_fd(create_server(port)),
    epoll_add([this, fn_add](int fd, const epoll_callback& callback, bool is_in, bool is_out) {
        bool res = fn_add(fd, &callbacks[fd], is_in, is_out);
        if (res) {
            callbacks[fd] = callback;
        }
        return res;
    }),
    epoll_del([this, fn_del](int fd){
        callbacks.erase(fd);
        fn_del(fd);
    }),
    new_connect_callback([this](epoll_event& ev){
        if ((ev.events & EPOLLIN) > 0) {
            int conn_fd = accept(server_fd, nullptr, nullptr);
            new_connect(conn_fd);
        }
    }),
    work_done_callback([this](epoll_event& ev) {
        std::cout << "mada" << std::endl;
        update();
        char buf[1];
        read(worker.notify_fd, buf, 1);
    })
{
    fn_add(server_fd, &new_connect_callback, true, false);
    fn_add(worker.notify_fd, &work_done_callback, true, false);
}

myserver::~myserver() {
    worker.is_working = false;
    worker.var_req.notify_all();
    for (auto& i : callbacks) {
        close(i.first);
    }
    for (auto& i : wait_worker) {
        close(i);
    }
    close(server_fd);
}

void myserver::close_fd(int fd) {
    epoll_del(fd);
    close(fd);
}

void myserver::new_connect(int conn_fd) {
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    itimerspec timer_spec;
    timer_spec.it_interval.tv_nsec = 0;
    timer_spec.it_interval.tv_sec = 0;
    timer_spec.it_value.tv_nsec = 0;
    timer_spec.it_value.tv_sec = CONNECTION_TIMEOUT;
    timerfd_settime(timer_fd, 0, &timer_spec, nullptr);

    if (!epoll_add(conn_fd, [this, conn_fd](epoll_event& ev) {
        if (!connections_new.count(conn_fd)) {
            return;
        }
        if ((ev.events & EPOLLIN) > 0) {
            myread(conn_fd);
        } else {
            remove_connection(conn_fd);
        }
    }, true, false)) {
        close(conn_fd);
        return;
    }
    if (!epoll_add(timer_fd, [this, conn_fd](epoll_event& ev) {
        remove_connection(conn_fd);
    }, true, false)) {
        close_fd(conn_fd);
        close(timer_fd);
    }

    connections_new[conn_fd] = std::vector<char>();
    timers[conn_fd] = timer_fd;
}

void myserver::myread(int conn_fd) {
    std::vector<char>& req = connections_new[conn_fd];
    char buf[BUF_SIZE];
    ssize_t r = read(conn_fd, buf, sizeof buf);
    bool is_done = false;
    if (r == 0) {
        is_done = true;
    } else if (req.size() + r > LIMIT) {
        req.clear();
        is_done = true;
    } else {
        int j;
        for(j = 0; j < r && buf[j] != '\n' && buf[j] != '\r'; ++j) {
            req.push_back(buf[j]);
        }
        is_done = j < r;
    }
    if (is_done) {
        epoll_del(conn_fd);
        int timer_fd = timers[conn_fd];
        close_fd(timer_fd);
        if (req.size() > 0) {
            wait_worker.insert(conn_fd);
            std::unique_lock<std::mutex> ul(worker.mreq);
            worker.qreq.push(std::make_pair(conn_fd, req));
            worker.var_req.notify_one();
        } else {
            close(conn_fd);
        }
        timers.erase(conn_fd);
        connections_new.erase(conn_fd);
    }
}

void myserver::mywrite(int conn_fd) {
    write(conn_fd, connections_send[conn_fd].data(), connections_send[conn_fd].size());
    close_fd(conn_fd);
    connections_send.erase(conn_fd);
}

void myserver::remove_connection(int conn_fd) {
    if (!connections_new.count(conn_fd)) {
        return;
    }
    int timer_fd = timers[conn_fd];
    close_fd(timer_fd);
    close_fd(conn_fd);

    timers.erase(conn_fd);
    connections_new.erase(conn_fd);
}

void myserver::update() {
    std::unique_lock<std::mutex> ul(worker.mres);
    while (worker.qres.size() > 0) {
        auto res = worker.qres.front();
        worker.qres.pop();
        int conn_fd = res.first;
        if (epoll_add(res.first, [this, conn_fd](epoll_event& ev){
            if ((ev.events & EPOLLOUT) > 0) {
                mywrite(conn_fd);
            } else {
                close_fd(conn_fd);
                connections_send.erase(conn_fd);
            }
        }, false, true)) {
            connections_send[conn_fd] = res.second;
        } else {
            close(conn_fd);
        }
        wait_worker.erase(conn_fd);
    }
}

int create_server(uint16_t port) {
    int s = socket(PF_INET, SOCK_STREAM, 0);
    sockaddr_in local_addr;
    local_addr.sin_family = PF_INET;
    local_addr.sin_addr.s_addr = 0;
    local_addr.sin_port = htons(port);
    while (bind(s, reinterpret_cast<sockaddr const*>(&local_addr), sizeof local_addr) != 0) {
        std::cerr << "cant start. retry" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    while (listen(s, SOMAXCONN) != 0) {
        std::cerr << "cant start. retry" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    };
    std::cout << "strart success" << std::endl;
    return s;
}
