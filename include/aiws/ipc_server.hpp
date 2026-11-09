#pragma once

#include "ipc_message.hpp"
#include "aiws/gui/main_window.hpp"

#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QString>

#include <thread>

namespace aiws::ipc {

class Server {
public:
    ~Server() { stop_.store(true);
        if (worker_.joinable()) {  worker_.join(); } }


            //
    bool start(const std::string& path, gui::MainWindow& window) {
        sockaddr_un address{};
        if (!make_address(path, address)) { return false;}

        const int listener =::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (listener < 0) { return false; }

        if (::bind(listener,reinterpret_cast<const sockaddr*>(&address),sizeof(address)) != 0) {
            ::close(listener);
            return false;}

        if (::listen(listener, 8) != 0) 
        {   ::close(listener);
            ::unlink(path.c_str());
            return false;}

        try { worker_ = std::thread(&Server::run, this, listener, path, &window);} catch (...) {
            ::close(listener);
            ::unlink(path.c_str());
            return false;}
        return true;}

private:

    //
    static void handle_client(int client, gui::MainWindow* window) {
        const auto deadline = Clock::now() + std::chrono::seconds{5};
        std::string request;
        if (!receive_frame(client, request, deadline)) {return;}

        if (request.empty() || request.find('\0') != std::string::npos) {
            send_frame(client, "1Invalid document path", deadline);
            return; }

        const QString path = QFile::decodeName(QByteArray(request.data(), static_cast<qsizetype>(request.size())));
        QFile file(path);

        if (!QFileInfo(path).isFile() || !file.open(QIODevice::ReadOnly)) {
            send_frame(client, "1Couldnt read document", deadline);
            return;}

        const bool queued = QMetaObject::invokeMethod(window, [window, path] { window->add_document_file(path); },  Qt::QueuedConnection);
        send_frame(client, queued ? "0" : "Couldnt queue document", deadline);
    }

        //
    void run(int listener, std::string path, gui::MainWindow* window) {
        socket_stop_flag = &stop_;

        while (!stop_.load()) {
            const auto deadline = Clock::now() + std::chrono::milliseconds{50};
            if (!wait_for_socket(listener, POLLIN, deadline)) {continue;}

            const int client = ::accept(listener, nullptr, nullptr);
            if (client < 0) {continue;}

            handle_client(client, window);
            ::close(client);
        }

        ::close(listener);
        ::unlink(path.c_str());
    }

    std::atomic<bool> stop_{false};
    std::thread worker_; };

}  // namespace aiws::ipc