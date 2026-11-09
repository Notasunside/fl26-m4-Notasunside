    
#include "aiws/gui/main_window.hpp"

#include "aiws/ipc_server.hpp"

#include <QByteArray>
#include <QFile>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QMessageBox>

#include <iostream>

namespace {

void confirm_exit(QWidget* parent) {
    auto answer = QMessageBox::question( parent, "Exit","Exit the application?",QMessageBox::Yes | QMessageBox::No,  QMessageBox::No);
        if (answer == QMessageBox::Yes) { parent->close(); } }

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();

    const QCommandLineOption delay_option(
        QStringLiteral("build-delay-ms"), QStringLiteral("Xtra build time per document in ms."), QStringLiteral("N"), QStringLiteral("0"));

        //
    const QCommandLineOption socket_option(
        QStringLiteral("ipcsocket"),
        QStringLiteral("Unix socket path for remote document additions"),
        QStringLiteral("PATH"));

        parser.addOption(delay_option);
    parser.addOption(socket_option);
    parser.process(app);

    bool valid_delay =false;
    const int delay_ms =parser.value(delay_option).toInt(&valid_delay);
    if (!valid_delay || delay_ms < 0) {
        std::cerr << "--build-delay-ms requires a nonnegative integer\n";
        return 1;  }

    aiws::gui::MainWindow window;
    window.set_build_delay_ms(delay_ms);
        window.set_exit_callback(confirm_exit);

    //
    const std::string socket_path = parser.isSet(socket_option) ? QFile::encodeName(parser.value(socket_option)).toStdString() : aiws::ipc::default_socket_path();

    aiws::ipc::Server server;
    if (!server.start(socket_path, window)) {
        std::cerr << "Could not start IPC server at " << socket_path << '\n';
        return 1; }
    //


    window.show();

    return app.exec();
}