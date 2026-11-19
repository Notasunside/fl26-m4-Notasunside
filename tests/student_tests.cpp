// Your M4 tests (specification, Section 6).
#include "aiws/gui/main_window.hpp"

#include <QFile>
#include <QIODevice>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest/QtTest>

//m5
#include <QCoreApplication>
#include <QProcess>
#include <QProcessEnvironment>

#include <cstdlib>

namespace {

QString make_document(const QTemporaryDir& directory, const QString& name) {
    const QString path =directory.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {return {};}

    const QByteArray text("AI workspaces organize and retrieve documents.\n");
    if (file.write(text) != text.size() || !file.flush()) { return {};}
    return path;}

QPushButton* find_button(QWidget& window, const QString& text) {
    for (auto* button : window.findChildren<QPushButton*>()) {if (button->text() == text) {return button;}} 
    return nullptr;}

bool has_label_starting_with(const QWidget& window, const QString& prefix) {
    for (const auto* label : window.findChildren<QLabel*>()) {if (label->text().startsWith(prefix)) 
        { return true;} } return false; } 
}  // namespace

class StudentTests : public QObject {
    Q_OBJECT
    //11/19: fixed MOC warning

private slots:
    void exits_after_worker_becomes_idle() {
        if (qEnvironmentVariableIsSet("AIWS_IDLE_EXIT_CHILD")) {
            QTemporaryDir directory;
            QVERIFY(directory.isValid());

            const QString path = make_document(directory, "idle-exit.txt");
            QVERIFY(!path.isEmpty());

            aiws::gui::MainWindow window;
            auto* build = find_button(window, "Build Corpus");
            QVERIFY(build);
            QVERIFY(window.add_document_file(path));

            build->click();
            QTRY_VERIFY_WITH_TIMEOUT(
                has_label_starting_with(window, "Corpus: 1 chunks"),
                3000);

            QVERIFY(window.close());
            return;
        }

        QProcess child;
        QProcessEnvironment environment =
            QProcessEnvironment::systemEnvironment();
        environment.insert("AIWS_IDLE_EXIT_CHILD", "1");
        environment.insert("QT_QPA_PLATFORM", "offscreen");
        child.setProcessEnvironment(environment);

        child.start(
            QCoreApplication::applicationFilePath(),
            QStringList{"exits_after_worker_becomes_idle"});
        QVERIFY(child.waitForStarted(3000));

        const bool finished = child.waitForFinished(8000);
        if (!finished) {
            child.kill();
            child.waitForFinished(3000);
        }

        const QByteArray output =
            child.readAllStandardOutput() + child.readAllStandardError();

        if (!finished) {
            QTest::qFail(
                "Exit hung after the build worker became idle.",
                __FILE__,
                __LINE__);
            std::exit(EXIT_FAILURE);
        }

        QVERIFY2(
            child.exitStatus() == QProcess::NormalExit,
            output.constData());
        QVERIFY2(child.exitCode() == 0, output.constData());
    }

    void reset_cancels_running_build() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        aiws::gui::MainWindow window;
        window.set_build_delay_ms(500);

        auto* build = find_button(window, "Build Corpus");
        auto* reset = find_button(window, "Reset");
        QVERIFY(build);
        QVERIFY(reset);

        for (int index = 0; index < 12; ++index) {
            const QString path = make_document(
                directory, QString("document-%1.txt").arg(index));
            QVERIFY(!path.isEmpty());
            QVERIFY(window.add_document_file(path));
        }

        QSignalSpy started(
            &window, &aiws::gui::MainWindow::build_started);
        QVERIFY(started.isValid());

        build->click();
        QTRY_VERIFY_WITH_TIMEOUT(started.count() == 1, 2000);

        reset->click();

        const QString replacement =
            make_document(directory, "replacement.txt");
        QVERIFY(!replacement.isEmpty());
        QVERIFY(window.add_document_file(replacement));

        // Cancellation should release the worker promptly.
        QTRY_VERIFY_WITH_TIMEOUT(build->isEnabled(), 1000);
        QVERIFY(has_label_starting_with(window, "Corpus: Not built"));

        window.set_build_delay_ms(0);
        build->click();

        QTRY_VERIFY_WITH_TIMEOUT(
            has_label_starting_with(window, "Corpus: 1 chunks"),
            2000);
    }

    void discards_build_after_document_added() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString first = make_document(directory, "first.txt");
        const QString second = make_document(directory, "second.txt");
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());

        aiws::gui::MainWindow window;
        auto* build = find_button(window, "Build Corpus");
        QVERIFY(build);
        QVERIFY(window.add_document_file(first));
        QVERIFY(build->isEnabled());

        build->click();
        QVERIFY(!build->isEnabled());
        QVERIFY(window.add_document_file(second));

        QTRY_VERIFY_WITH_TIMEOUT(build->isEnabled(), 3000);
        QVERIFY(has_label_starting_with(window, "Corpus: Not built"));
    }

    void reports_build_failure_and_recovers() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = make_document(directory, "duplicate.txt");
        QVERIFY(!path.isEmpty());

        aiws::gui::MainWindow window;
        auto* build = find_button(window, "Build Corpus");
        auto* reset = find_button(window, "Reset");
        QVERIFY(build);
        QVERIFY(reset);

        QVERIFY(window.add_document_file(path));
        QVERIFY(window.add_document_file(path));

        build->click();
        QVERIFY(!build->isEnabled());

        QTRY_VERIFY_WITH_TIMEOUT(
            has_label_starting_with(window, "Corpus build failed"),
            3000);
        QVERIFY(build->isEnabled());

        reset->click();
        QVERIFY(window.add_document_file(path));
        build->click();

        QTRY_VERIFY_WITH_TIMEOUT(
            has_label_starting_with(window, "Corpus built."),
            3000);
        QVERIFY(build->isEnabled());
    }
};



QTEST_MAIN(StudentTests)
#include "student_tests.moc"