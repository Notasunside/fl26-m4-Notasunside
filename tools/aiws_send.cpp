#include "aiws/ipc_message.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

namespace {

        //

bool connect_to_server(int fd, const sockaddr_un& address) {
    const auto* socket_address =  reinterpret_cast<const sockaddr*>(    &address    );
    if (::connect(fd, socket_address, sizeof(address)) == 0) { return true;}
    if (errno != EINPROGRESS) { return false;}
    const auto deadline =aiws::ipc::Clock::now() + std::chrono::seconds{2};
    if (!aiws::ipc::wait_for_socket(fd, POLLOUT, deadline)) { return false;}

    int socket_error =0;
    socklen_t size =sizeof(socket_error);
    return ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &size) ==0 && socket_error == 0; }

}  // namespace


    //
int main(int argc, char* argv[]) {
    std::string socket_path = aiws::ipc::default_socket_path();
    int arg = 1;

    if (argc >= 3 && std::string(argv[arg]) == "--socket") {
        socket_path =argv[arg + 1];
        arg += 2; }

    if (argc - arg != 2 || std::string(argv[arg]) != "add") 
    {  std::cerr << "Usage: aiws_send [--socket PATH] add FILE\n";
        return 2; }

    std::error_code path_error;
    const auto file_path = std::filesystem::absolute(argv[arg + 1], path_error);
    if (path_error) {
        std::cerr << "Couldnt resolve file path: "<< path_error.message() << '\n';
        return 2; }

    sockaddr_un address{};
    if (!aiws::ipc::make_address(socket_path, address)) { std::cerr << "Invalid socket path\n";
        return 2; }

    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        std::cerr << "Coudlnt create socket\n";
        return 2; }

    if (!connect_to_server(fd, address)) {
        ::close(fd);
        std::cerr << "Couldnt reach AI Workspace at " << socket_path << '\n';
        return 2; }

    const auto deadline= aiws::ipc::Clock::now() + std::chrono::seconds{5};
    std::string reply;

    const bool exchanged =aiws::ipc::send_frame(fd, file_path.string(), deadline) && aiws::ipc::receive_frame(fd, reply, deadline);
    ::close(fd);

    if (!exchanged || reply.empty()) {
        std::cerr << "Bad comms with AI workspace\n";
        return 2; }

    if (reply == "0") {  return 0;}

    if (reply.front() == '1') { std::cerr << reply.substr(1) << '\n';
        return 1; }

    std::cerr << "Inv reply from AI workspace\n";
    return 2; }