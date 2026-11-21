// Your M4 tests ( specification, Section 6).
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

//fixing what M4 missed
#include "aiws/ipc_server.hpp"
#include <QDir>
#include <QListWidget>

namespace {

QString make_document( const QTemporaryDir& directory, const QString& name) {
    const QString path=directory.filePath( name );
    QFile file( path );
    if ( !file.open( QIODevice::WriteOnly)) {return {};}

    const QByteArray text( "AI workspaces organize and retrieve documents.\n" );
    if ( file.write( text) != text.size( ) || !file.flush( )) { return {};}
    return path;}

QPushButton* find_button( QWidget& window, const QString& text) {
    for ( auto* button : window.findChildren<QPushButton*>( )) {if ( button->text( )== text) {return button;}} 
    return nullptr;}

bool has_label_starting_with( const QWidget& window, const QString& prefix) {
    for ( const auto* label : window.findChildren<QLabel*>( )) {if ( label->text( ).startsWith( prefix)) 
        { return true;} } return false; } 
}  // namespace

class StudentTests : public QObject {
    Q_OBJECT
    //11/19: fixed MOC warning




private slots:
    void exits_after_worker_becomes_idle( ) {
        if ( qEnvironmentVariableIsSet( "AIWS_IDLE_EXIT_CHILD")) {
            QTemporaryDir directory;
            QVERIFY( directory.isValid( ) );

            const QString path= make_document( directory, "idle-exit.txt" );
            QVERIFY( !path.isEmpty( ) );

            aiws::gui::MainWindow window;
            auto* build= find_button( window, "Build Corpus" );
            QVERIFY( build );
            QVERIFY( window.add_document_file( path) );

            build->click(  );
            QTRY_VERIFY_WITH_TIMEOUT( has_label_starting_with( window, "Corpus: 1 chunks"),3000 );

            QVERIFY( window.close( ) );
            return;}

        QProcess child;
        QProcessEnvironment environment= QProcessEnvironment::systemEnvironment(   );
        environment.insert( "AIWS_IDLE_EXIT_CHILD", "1" );
        environment.insert( "QT_QPA_PLATFORM", "offscreen" );
        child.setProcessEnvironment( environment );

        child.start( QCoreApplication::applicationFilePath( ),QStringList{"exits_after_worker_becomes_idle"} );
        QVERIFY( child.waitForStarted( 3000) );

        const bool finished= child.waitForFinished( 8000 );
        if ( !finished) {
            child.kill(  );
            child.waitForFinished( 3000 );}



        const QByteArray output=child.readAllStandardOutput( ) + child.readAllStandardError(  );

        if ( !finished) {

            QTest::qFail( "Exit hung after the build worker became idle.", __FILE__,__LINE__ );
            std::exit( EXIT_FAILURE ); }



        QVERIFY2( child.exitStatus( )== QProcess::NormalExit, output.constData( ) );


        QVERIFY2( child.exitCode( )== 0, output.constData( ) ); }

    //



    //
    void reuses_worker_across_builds( ) {
        QTemporaryDir directory;
        QVERIFY( directory.isValid( ) );

        aiws::gui::MainWindow window;
        const QString path= make_document( directory, "reuse.txt" );

        QVERIFY( !path.isEmpty( ) );

        QVERIFY( window.add_document_file( path) );

        auto* build= find_button( window, "Build Corpus" );

        QVERIFY( build );

        QVERIFY( window.build_worker_id( )== std::thread::id{} );

        QSignalSpy started( &window, &aiws::gui::MainWindow::build_started );

        QVERIFY( started.isValid( ) );

        build->click(  );

        QTRY_VERIFY_WITH_TIMEOUT( started.count( )== 1, 2000 );

        QTRY_VERIFY_WITH_TIMEOUT( has_label_starting_with( window, "Corpus: 1 chunks"), 3000 );

        const auto first_worker= window.build_worker_id(  );

        QVERIFY( first_worker != std::thread::id{} );

        QVERIFY( build->isEnabled( ) );

        build->click(  );

        QTRY_VERIFY_WITH_TIMEOUT( started.count( )== 2, 2000 );

        QTRY_VERIFY_WITH_TIMEOUT( build->isEnabled( ), 3000 );

        QVERIFY( window.build_worker_id( )== first_worker );

        QVERIFY( has_label_starting_with( window, "Corpus: 1 chunks") ); }


    //
        void exits_during_running_build( ) {
        if ( qEnvironmentVariableIsSet( "AIWS_ACTIVE_EXIT_CHILD")) {
            QTemporaryDir directory;

            QVERIFY( directory.isValid( ) );

            aiws::gui::MainWindow window;

            window.set_build_delay_ms( 500 );

            for ( int index= 0; index < 12; ++index) {
                const QString path= make_document( directory, QString( "exit-%1.txt").arg( index) );

                QVERIFY( !path.isEmpty( ) );

                QVERIFY( window.add_document_file( path) ); }

            auto* build= find_button( window, "Build Corpus" );

            QVERIFY( build );

            QSignalSpy started( &window, &aiws::gui::MainWindow::build_started );

            QVERIFY( started.isValid( ) );

            build->click(  );

            QTRY_VERIFY_WITH_TIMEOUT( started.count( )== 1, 2000 );

            QVERIFY( window.close( ) );
            return;
        }

        QProcess child;
        QProcessEnvironment environment=QProcessEnvironment::systemEnvironment(  );
        environment.insert( "AIWS_ACTIVE_EXIT_CHILD", "1" );
        environment.insert( "QT_QPA_PLATFORM", "offscreen" );
        child.setProcessEnvironment( environment );

        child.start( QCoreApplication::applicationFilePath( ),QStringList{"exits_during_running_build"} );

        QVERIFY( child.waitForStarted( 3000) );

        const bool finished= child.waitForFinished( 4000 );

        if ( !finished) {
            child.kill( ); //kill the child >:( NOW!!
            child.waitForFinished( 3000 ); }

        const QByteArray output=child.readAllStandardOutput( ) + child.readAllStandardError(  );

        QVERIFY2( finished, "Exit failed to stop the running build promptly." );

        QVERIFY2( child.exitStatus( )== QProcess::NormalExit,output.constData( ) );

        QVERIFY2( child.exitCode( )== 0, output.constData( ) );}

        //




        //
    void reset_cancels_running_build( ) {
        QTemporaryDir directory;
        QVERIFY( directory.isValid( ) );

        aiws::gui::MainWindow window;

        window.set_build_delay_ms( 500 );

        auto* build= find_button( window, "Build Corpus" );

        auto* reset= find_button( window, "Reset" );
        
        QVERIFY( build );
        QVERIFY( reset );

        for ( int index= 0; index < 12; ++index) {
            const QString path= make_document( directory, QString( "document-%1.txt").arg( index) );
            QVERIFY( !path.isEmpty( ) );
            QVERIFY( window.add_document_file( path) ); }


        QSignalSpy started( &window, &aiws::gui::MainWindow::build_started );

        QVERIFY( started.isValid( ) );

        build->click(  );

        QTRY_VERIFY_WITH_TIMEOUT( started.count( )== 1, 2000 );

        reset->click(  );

        const QString replacement= make_document( directory, "replacement.txt" );

        QVERIFY( !replacement.isEmpty( ) );

        QVERIFY( window.add_document_file( replacement) );

        // cancellation should release the worker almost immediately
        //w/in 60 sec? 
        QTRY_VERIFY_WITH_TIMEOUT( build->isEnabled( ), 1000 );

        QVERIFY( has_label_starting_with( window, "Corpus: Not built") );

        window.set_build_delay_ms( 0 );

        build->click(  );

        QTRY_VERIFY_WITH_TIMEOUT( has_label_starting_with( window, "Corpus: 1 chunks"),2000 );}

        //



        //
    void discards_build_after_document_added( ) {
        QTemporaryDir directory;

        QVERIFY( directory.isValid( ) );

        const QString first= make_document( directory, "first.txt" );

        const QString second= make_document( directory, "second.txt" );

        QVERIFY( !first.isEmpty( ) );

        QVERIFY( !second.isEmpty( ) );

        aiws::gui::MainWindow window;

        auto* build=find_button( window, "Build Corpus" );

        QVERIFY( build );

        QVERIFY( window.add_document_file(first) );

        QVERIFY( build->isEnabled( ) );

        build->click(  );

        QVERIFY( !build->isEnabled( ) );

        QVERIFY( window.add_document_file( second) );

        QTRY_VERIFY_WITH_TIMEOUT( build->isEnabled( ), 3000 );

        QVERIFY( has_label_starting_with( window, "Corpus: Not built") );}

        //

    void reports_build_failure_and_recovers( ) {
        QTemporaryDir directory;

        QVERIFY( directory.isValid( ) );

        const QString path=make_document( directory, "duplicate.txt" );

        QVERIFY( !path.isEmpty( ) );

        aiws::gui::MainWindow window;

        auto* build=find_button( window, "Build Corpus" );

        auto* reset=find_button( window, "Reset" );

        QVERIFY( build );
        QVERIFY( reset );

        QVERIFY( window.add_document_file( path) );
        QVERIFY( window.add_document_file( path) );

        build->click(  );

        QVERIFY( !build->isEnabled( ) );

        QTRY_VERIFY_WITH_TIMEOUT(  has_label_starting_with( window, "Corpus build failed"),3000 );

        QVERIFY( build->isEnabled( ) );

        reset->click(  );

        QVERIFY( window.add_document_file( path) );

        build->click(  );

        QTRY_VERIFY_WITH_TIMEOUT( has_label_starting_with( window, "Corpus built."),3000 );

        QVERIFY( build->isEnabled( ) );}

//

//
        void remote_add_invalidates_current_corpus( ) {
        QTemporaryDir directory;

        QVERIFY( directory.isValid( ) );

        aiws::gui::MainWindow window;
        aiws::ipc::Server server;

        const QString socket= directory.filePath( "remote.sock" );

        QVERIFY( server.start( QFile::encodeName( socket).toStdString( ), window) );

        const QString first= make_document( directory, "first.txt" );

        QVERIFY( !first.isEmpty( ) );

        QVERIFY( window.add_document_file( first) );

        auto* build= find_button( window, "Build Corpus" );
        auto* documents= window.findChild<QListWidget*>(  );


        QVERIFY( build );
        QVERIFY( documents );

        build->click(  );

        QTRY_VERIFY_WITH_TIMEOUT(  has_label_starting_with( window, "Corpus: 1 chunks"), 3000 );

        // Exercise spaces, UTF8, and a newline in one filename
        const QString filename= QString::fromUtf8( "remote café\nwith spaces.txt" );

        const QString remote= make_document( directory, filename );

        QVERIFY( !remote.isEmpty( ) );

        QProcess client;
        client.start( QDir( QCoreApplication::applicationDirPath( )).filePath( "aiws_send"),  QStringList{"--socket", socket, "add", remote} );

        QVERIFY( client.waitForStarted( 3000) );

        QTRY_VERIFY_WITH_TIMEOUT( client.state( )== QProcess::NotRunning, 7000 );

        QCOMPARE( client.exitStatus( ), QProcess::NormalExit );

        QCOMPARE( client.exitCode( ), 0 );

        QTRY_COMPARE_WITH_TIMEOUT( documents->count( ), 2, 3000 );

        QCOMPARE( documents->item( 1)->text( ), filename );

        QVERIFY( has_label_starting_with( window, "Corpus: Not built") );

        QVERIFY( build->isEnabled( ) );

        build->click(  );

        QTRY_VERIFY_WITH_TIMEOUT(  has_label_starting_with( window, "Corpus: 2 chunks"),3000 );  }


//
    void remote_client_reports_errors( ) {
        QTemporaryDir directory;

        QVERIFY( directory.isValid( ) );

        const QString executable=QDir( QCoreApplication::applicationDirPath( )).filePath( "aiws_send" );

        aiws::gui::MainWindow window;
        aiws::ipc::Server server;

        const QString socket= directory.filePath( "errors.sock" );

        QVERIFY( server.start( QFile::encodeName( socket).toStdString( ), window) );

        QProcess unreadable;

        unreadable.start(  executable, QStringList{ "--socket",socket,"add", directory.filePath( "missing.txt")} );

        QVERIFY( unreadable.waitForStarted( 3000) );

        QTRY_VERIFY_WITH_TIMEOUT(  unreadable.state( )== QProcess::NotRunning, 7000 );

        QCOMPARE( unreadable.exitStatus( ), QProcess::NormalExit );

        QCOMPARE( unreadable.exitCode( ), 1 );

        QProcess unavailable;

        unavailable.start(  executable, QStringList{"--socket", directory.filePath( "no-server.sock"),"add",  directory.filePath( "missing.txt") } );

        QVERIFY( unavailable.waitForStarted( 3000) );
        //-- wait for the process to finish with a timeout of 7 seconds this time 
        QTRY_VERIFY_WITH_TIMEOUT(  unavailable.state( )== QProcess::NotRunning, 7000 );

        QCOMPARE( unavailable.exitStatus( ), QProcess::NormalExit );

        QCOMPARE( unavailable.exitCode( ), 2 ); }


        //more m5



        void remote_add_cancels_running_build( ) {
            QTemporaryDir directory;
            QVERIFY( directory.isValid( ) );

            aiws::gui::MainWindow window;

            window.set_build_delay_ms( 500 );

            aiws::ipc::Server server;

            const QString socket= directory.filePath( "mid-build.sock" );

            QVERIFY( server.start( QFile::encodeName( socket).toStdString( ), window) );

            auto* build= find_button( window, "Build Corpus" );

            auto* documents= window.findChild<QListWidget*>(  );
            QVERIFY( build );
            QVERIFY( documents );

            for ( int index= 0; index < 12; ++index) {
                const QString path= make_document( directory, QString( "original-%1.txt").arg( index) );

                QVERIFY( !path.isEmpty( ) );

                QVERIFY( window.add_document_file( path) ); }

            QSignalSpy started(  &window, &aiws::gui::MainWindow::build_started );

            QVERIFY( started.isValid( ) );

            build->click(  );

            QTRY_VERIFY_WITH_TIMEOUT( started.count( )== 1, 2000 );

            const QString remote=make_document( directory, "remote-during-build.txt" );

            QVERIFY( !remote.isEmpty( ) );

            QProcess client;

            client.start(  QDir( QCoreApplication::applicationDirPath( )).filePath( "aiws_send"), QStringList{"--socket", socket, "add", remote} );

            QVERIFY( client.waitForStarted( 3000) );

            QTRY_VERIFY_WITH_TIMEOUT( client.state( )== QProcess::NotRunning, 7000 );

            QCOMPARE( client.exitStatus( ), QProcess::NormalExit );

            QCOMPARE( client.exitCode( ), 0 );

            QTRY_COMPARE_WITH_TIMEOUT( documents->count( ), 13, 3000 );

            QVERIFY( has_label_starting_with( window, "Corpus: Not built") );

            // an obsolete build should stop quick
            QTRY_VERIFY_WITH_TIMEOUT( build->isEnabled( ), 1000 );

            window.set_build_delay_ms( 0 );

            build->click(  );

            QTRY_VERIFY_WITH_TIMEOUT(  has_label_starting_with( window, "Corpus: 13 chunks"),3000 );}


//
        void rejects_malformed_remote_requests( ) {
            QTemporaryDir directory;

            QVERIFY( directory.isValid( ) );

            aiws::gui::MainWindow window;
            aiws::ipc::Server server;
            
            const QString socket= directory.filePath( "malformed.sock" );

            QVERIFY( server.start(  QFile::encodeName( socket).toStdString( ), window) );

            sockaddr_un address{};

            QVERIFY( aiws::ipc::make_address(  QFile::encodeName( socket).toStdString( ), address) );

            const std::string requests[]= {std::string{}, std::string( "invalid\0path", 12) };

            //--//
            for ( const auto& request : requests) {
                const int client= ::socket( AF_UNIX, SOCK_STREAM, 0 );
                
                QVERIFY( client >= 0 );

                const int connected= ::connect( client, reinterpret_cast<const sockaddr*>( &address),sizeof( address) );

                //--//
                if ( connected != 0) {
                    ::close( client );
                    QFAIL( "Could not connect to the test server." );}
                //--//

                const auto deadline=aiws::ipc::Clock::now( ) + std::chrono::seconds{2};

                std::string reply;

                const bool sent=aiws::ipc::send_frame( client, request, deadline );
                
                const bool received=sent && aiws::ipc::receive_frame( client, reply, deadline );

                ::close( client );

                QVERIFY( received );
                QVERIFY( !reply.empty( ) );
                QCOMPARE( reply.front( ), '1' );}
                //--//
            auto* documents= window.findChild<QListWidget*>(  );

            QVERIFY( documents );

            QCOMPARE( documents->count( ), 0 );

            const QString valid=make_document( directory, "valid-after-malformed.txt" );

            QVERIFY( !valid.isEmpty( ) );

            QProcess client;

            client.start( QDir( QCoreApplication::applicationDirPath( )).filePath( "aiws_send"), QStringList{"--socket", socket, "add", valid} );

            QVERIFY( client.waitForStarted( 3000) );

            QTRY_VERIFY_WITH_TIMEOUT(  client.state( )== QProcess::NotRunning, 7000 );

            QCOMPARE( client.exitStatus( ), QProcess::NormalExit );

            QCOMPARE( client.exitCode( ), 0 );

            QTRY_COMPARE_WITH_TIMEOUT( documents->count( ), 1, 3000 ); } };


        
    //exits_after_worker_becomes_idle
    //reuses_worker_across_builds
    //exits_during_running_build
    //reset_cancels_running_build
    //discards_build_after_document_added
    //reports_build_failure_and_recovers
    // remote_add_invalidates_current_corpus
    // remote_client_reports_errors
    //more m5


QTEST_MAIN( StudentTests)
#include "student_tests.moc"