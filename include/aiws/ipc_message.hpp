#pragma once

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>  // std::min
#include <atomic>     // std::atomic

namespace aiws::ipc {

using Clock = std::chrono::steady_clock;
using Deadline = Clock::time_point;
//addserver
inline thread_local const std::atomic<bool>* socket_stop_flag = nullptr;

constexpr std::uint32_t max_payload_size = 64 * 1024;


    //
inline std::string default_socket_path() {
    const char* user = std::getenv("USER");
    const std::string name =user && *user ? user : std::to_string(::getuid());
    return "/tmp/aiws-" + name + ".sock"; }


    //
inline bool make_address(const std::string& path, sockaddr_un& address) {
    address = {};
    if (path.empty() || path.size() >= sizeof(address.sun_path)) { return false; }

    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    return true;}


    //
inline bool wait_for_socket(int fd, short events, Deadline deadline) {
    while (Clock::now() < deadline) {
        if (socket_stop_flag && socket_stop_flag->load()) { return false; }
        const auto remaining = std::chrono::ceil<std::chrono::milliseconds>( deadline - Clock::now());
        const auto timeout = socket_stop_flag ? std::min(remaining, std::chrono::milliseconds{50}) : remaining;

        pollfd descriptor{fd, events, 0};
        const int result = ::poll(&descriptor, 1, static_cast<int>(timeout.count()));

        if (result > 0) { return (descriptor.revents & events) != 0; }
        if (result < 0 && errno != EINTR) { return false; }   }
    return false; }


    //
inline bool write_all(int fd, const void* data, std::size_t size, Deadline deadline) {
    const auto* bytes = static_cast<const char*>(data);
    while (size > 0) {
        if (!wait_for_socket(fd, POLLOUT, deadline)) {return false;}

        const auto sent = ::send(fd, bytes, size, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (sent < 0 &&  (errno == EINTR || errno == EAGAIN|| errno ==EWOULDBLOCK)) 
        {continue;}
        if (sent <= 0) {return false;}

        bytes += sent;
        size -= static_cast<std::size_t>(sent);}
    return true; }

    //
inline bool read_all(int fd, void* data, std::size_t size,  Deadline deadline) {
    auto* bytes = static_cast<char*>(data);

    while (size > 0) {
        if (!wait_for_socket(fd, POLLIN, deadline)) {   return false;    }

        const auto received = ::recv(fd, bytes, size, MSG_DONTWAIT);
        if (received < 0 &&(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) 
        { continue; }
        
        if (received <= 0) {return false;}
        bytes += received;
        size -= static_cast<std::size_t>(received);
    }
    return true;
}

//

inline bool send_frame(int fd, const std::string& payload,Deadline deadline) {
    if (payload.size() > max_payload_size) {        return false;}
    const std::uint32_t length = htonl(static_cast<std::uint32_t>(payload.size()));
    return write_all(fd, &length, sizeof(length), deadline) && write_all(fd, payload.data(), payload.size(), deadline); }


    //
inline bool receive_frame(int fd, std::string& payload, Deadline deadline) {
    std::uint32_t length = 0;
    if (!read_all(fd, &length, sizeof(length), deadline)) {return false; }
    length = ntohl(length);
    if (length > max_payload_size) { return false; }
    payload.resize(length);
    return read_all(fd, payload.data(), payload.size(), deadline); }

}  // namespace aiws::ipc