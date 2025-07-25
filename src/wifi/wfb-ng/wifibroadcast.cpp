// Copyright (C) 2017 - 2024 Vasily Evseenko <svpcom@p2ptech.org>

/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 3.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef __linux__

#include "wifibroadcast.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <memory>
#include <stdexcept>
#include <string>

using namespace std;

string string_format(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    // Extra space for '\0'
    size_t size = vsnprintf(nullptr, 0, format, args) + 1; // NOLINT(clang-analyzer-valist.Uninitialized)
    va_end(args);

    unique_ptr<char[]> buf(new char[size]);

    va_start(args, format);
    vsnprintf(buf.get(), size, format, args);
    va_end(args);

    // We don't want the '\0' inside
    return string(buf.get(), buf.get() + size - 1);
}

uint64_t get_time_ms(void) // in milliseconds
{
    struct timespec ts;
    int rc = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (rc < 0) throw runtime_error(string_format("Error getting time: %s", strerror(errno)));
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

uint64_t get_time_us(void) // in microseconds
{
    struct timespec ts;
    int rc = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (rc < 0) throw runtime_error(string_format("Error getting time: %s", strerror(errno)));
    return ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

int open_udp_socket_for_rx(int port, int rcv_buf_size, uint32_t bind_addr, int socket_type, int socket_protocol)
{
    struct sockaddr_in saddr;
    int fd = socket(AF_INET, socket_type, socket_protocol);
    if (fd < 0) throw runtime_error(string_format("Error opening socket: %s", strerror(errno)));

    const int optval = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(optval)) !=0)
    {
        close(fd);
        throw runtime_error(string_format("Unable to set SO_REUSEADDR: %s", strerror(errno)));
    }

    if(setsockopt(fd, SOL_SOCKET, SO_RXQ_OVFL, (const void *)&optval , sizeof(optval)) != 0)
    {
        close(fd);
        throw runtime_error(string_format("Unable to set SO_RXQ_OVFL: %s", strerror(errno)));
    }

    if (rcv_buf_size > 0)
    {
        if(setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const void *)&rcv_buf_size , sizeof(rcv_buf_size)) !=0)
        {
            close(fd);
            throw runtime_error(string_format("Unable to set SO_RCVBUF: %s", strerror(errno)));
        }
    }

    memset(&saddr, '\0', sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(bind_addr);
    saddr.sin_port = htons((unsigned short)port);

    if (::bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) < 0)
    {
        close(fd);
        throw runtime_error(string_format("Unable to bind to %s:%d : %s", inet_ntoa(saddr.sin_addr), port, strerror(errno)));
    }
    return fd;
}


int open_unix_socket_for_rx(const char *socket_path, int rcv_buf_size, int socket_type, int socket_protocol)
{
    struct sockaddr_un saddr;

    int fd = socket(AF_UNIX, socket_type, socket_protocol);
    if (fd < 0) throw runtime_error(string_format("Error opening socket: %s", strerror(errno)));

    const int optval = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(optval)) !=0)
    {
        close(fd);
        throw runtime_error(string_format("Unable to set SO_REUSEADDR: %s", strerror(errno)));
    }

    if(setsockopt(fd, SOL_SOCKET, SO_RXQ_OVFL, (const void *)&optval , sizeof(optval)) != 0)
    {
        close(fd);
        throw runtime_error(string_format("Unable to set SO_RXQ_OVFL: %s", strerror(errno)));
    }

    if (rcv_buf_size > 0)
    {
        if(setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const void *)&rcv_buf_size , sizeof(rcv_buf_size)) !=0)
        {
            close(fd);
            throw runtime_error(string_format("Unable to set SO_RCVBUF: %s", strerror(errno)));
        }
    }

    memset(&saddr, '\0', sizeof(saddr));
    saddr.sun_family = AF_UNIX;
    strncpy(saddr.sun_path + 1, socket_path, sizeof(saddr.sun_path) - 2);

    if (::bind(fd, (struct sockaddr *) &saddr, sizeof(sa_family_t) + strlen(saddr.sun_path + 1) + 1) < 0)
    {
        close(fd);
        throw runtime_error(string_format("Unable to bind to @%s : %s", saddr.sun_path + 1, strerror(errno)));
    }
    return fd;
}

#endif