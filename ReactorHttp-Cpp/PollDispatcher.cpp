#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include "Dispatcher.h"
#include "PollDispatcher.h"


PollDispatcher::PollDispatcher(EventLoop* evloop) : Dispatcher(evloop) {
    m_maxfd = 0;
    m_fds = new struct pollfd[m_maxNode];
    for (int i = 0; i < m_maxNode; i++) {
        m_fds[i].fd = -1;
        m_fds[i].events = -1;
        m_fds[i].revents = 0;
    }
    m_name = "Poll";
}


PollDispatcher::~PollDispatcher() {
    delete []m_fds;
}


int PollDispatcher::add() {

    int events = 0;
    if (m_channel->getEvent() & (int)FDEvent::ReadEvent) {
        events |= POLLIN;
    }
    if (m_channel->getEvent() & (int)FDEvent::WriteEvent) {
        events |= POLLOUT;
    }
    int i;
    for (i = 0; i < m_maxNode; i++) {
        if (m_fds[i].fd == -1) {
            m_fds[i].events = events;
            m_fds[i].fd = m_channel->getSocket();
            m_maxfd = i > m_maxfd ? i : m_maxfd;
            break;
        }
    }
    if (i >= m_maxNode) {
        return -1;
    }
    return 0;
}


int PollDispatcher::remove() {   
    int i;
    for (i = 0; i < m_maxNode; i++) {
        if (m_fds[i].fd == m_channel->getSocket()) {
            m_fds[i].events = 0;
            m_fds[i].revents = 0;
            m_fds[i].fd = -1;
            break;
        }
    }
    m_channel->destroyCallback(const_cast<void*>(m_channel->getArg()));
    if (i >= m_maxNode) {
        return -1;
    }
    return 0;
}


int PollDispatcher::modify() {
    int events = 0;
    if (m_channel->getEvent() & (int)FDEvent::ReadEvent) {
        events |= POLLIN;
    }
    if (m_channel->getEvent() & (int)FDEvent::WriteEvent) {
        events |= POLLOUT;
    }
    int i;
    for (i = 0; i < m_maxNode; i++) {
        if (m_fds[i].fd == m_channel->getSocket()) {
            m_fds[i].events = events;
            break;
        }
    }
    if (i >= m_maxNode) {
        return -1;
    }
    return 0;
}

int PollDispatcher::dispatch(int timeout) {
  
    int count = poll(m_fds, m_maxfd + 1, timeout * 1000);
    if (count == -1) {
        perror("poll");
        exit(0);
    }
    for (int i = 0; i <= m_maxfd; i++) {
        if (m_fds[i].fd == -1) {
            continue;
        }
        if (m_fds[i].revents & POLLIN) {
            m_evLoop->eventActivate(m_fds[i].fd, (int)FDEvent::ReadEvent);
        }   

        if (m_fds[i].revents & POLLOUT) {
            m_evLoop->eventActivate(m_fds[i].fd, (int)FDEvent::WriteEvent);
        }
    }
    return 0;
}
