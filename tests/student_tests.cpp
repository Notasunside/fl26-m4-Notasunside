// Your M4 tests (specification, Section 6).
#include "aiws/gui/main_window.hpp"

#include <QFile>
#include <QIODevice>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest/QtTest>

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

class StudentTests : public QObject { Q_OBJECT

private slots:
    void discards_build_after_document_added() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString first = make_document(directory, "first.txt");
        const QString second = make_document(directory, "second.txt");
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());

        aiws::gui::MainWindow window;
        auto* build= find_button(window, "Build Corpus");
        QVERIFY(build);
        QVERIFY(window.add_document_file(first));
        QVERIFY(build->isEnabled());

        build->click();
        QVERIFY(!build->isEnabled());
        QVERIFY(window.add_document_file(second));

        QTRY_VERIFY_WITH_TIMEOUT(build->isEnabled(), 3000);
        QVERIFY(has_label_starting_with(window, "Corpus: Not built"));}

    void reports_build_failure_and_recovers() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = make_document(directory, "duplicate.txt");
        QVERIFY(!path.isEmpty());

        aiws::gui::MainWindow window;
        auto* build= find_button(window, "Build Corpus");
        auto* reset= find_button(window, "Reset");
        QVERIFY(build);
        QVERIFY(reset);

        QVERIFY(window.add_document_file(path));
        QVERIFY(window.add_document_file(path));
        build->click();
        QVERIFY(!build->isEnabled());

        QTRY_VERIFY_WITH_TIMEOUT(has_label_starting_with(window, "Corpus build failed"), 3000);
        QVERIFY(build->isEnabled());

        reset->click();
        QVERIFY(window.add_document_file(path));
        build->click();

        QTRY_VERIFY_WITH_TIMEOUT( has_label_starting_with(window, "Corpus built."), 3000);
        QVERIFY(build->isEnabled()); } };

QTEST_MAIN(StudentTests)
#include "student_tests.moc"