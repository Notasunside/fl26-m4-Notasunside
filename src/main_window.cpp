#include "aiws/gui/main_window.hpp"

#include "aiws/gui/context_widget.hpp"
#include "aiws/gui/resettable.hpp"
#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/term_inspector_widget.hpp"
#include "aiws/gui/workspace_widget.hpp"
#include "aiws/build_control.hpp"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QByteArray>
#include <QCloseEvent>

#include <QTimer>
#include <chrono>


#include <exception>
#include <utility>


namespace aiws::gui {


    //
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // DONE: Constructed the M3 application window, composde the GUI components,
    // established layouts/presentation, connected events, and established initial state.
    setWindowTitle("AI Workspace");
    resize(900, 900);

    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* layout = new QVBoxLayout(central);

    auto* heading = new QLabel("AI Workspace", central);

    auto* status_layout = new QHBoxLayout;

    auto* bottom_layout = new QHBoxLayout;
    
    heading->setStyleSheet(
        "background-color: #243b53;"
        "color: white;"
        "font-size: 20px;"
        "padding: 10px;");
    layout->addWidget(heading);

    workspace_widget_ =new WorkspaceWidget(central);
    search_widget_ = new SearchWidget(central);
    term_inspector_widget_ =new TermInspectorWidget(central);
    context_widget_= new ContextWidget(central);

    layout->addWidget(workspace_widget_, 2);
    layout->addWidget(search_widget_, 3);

    
    bottom_layout->addWidget(term_inspector_widget_, 1);
    bottom_layout->addWidget(context_widget_, 1);
    layout->addLayout(bottom_layout, 2);

    status_label_= new QLabel("Ready.", central);
    reset_button_= new QPushButton("Reset", central);
    exit_button_= new QPushButton("Exit", central);

    reset_button_->setStyleSheet( "background-color: #355e3b; color: white;");
    exit_button_->setStyleSheet( "background-color: #8b3030; color: white;");

    
    status_layout->addWidget(status_label_, 1);
    status_layout->addWidget(reset_button_);
    status_layout->addWidget(exit_button_);
    layout->addLayout(status_layout);

    connect(workspace_widget_, &WorkspaceWidget::add_document_requested, this,&MainWindow::add_document); //ADDDOC1 2 // Receive widget request -> run MainWindow::add_document()

    connect(workspace_widget_,
            &WorkspaceWidget::build_corpus_requested, this, &MainWindow::build_corpus);

    connect(workspace_widget_,
            &WorkspaceWidget::document_selected, this, &MainWindow::document_selected);

    connect(search_widget_, &SearchWidget::search_requested, this, &MainWindow::search_requested); //SEARCH1 2

    connect(search_widget_,&SearchWidget::query_changed, this, &MainWindow::query_changed);

    connect(search_widget_, &SearchWidget::source_document_requested,this, &MainWindow::source_document_requested);
    connect(search_widget_, &SearchWidget::selected_chunk_changed, term_inspector_widget_, &TermInspectorWidget::set_selected_chunk);  //SELECTROW1 5 // sends chunk id to TermINspectorWidget
    
    connect(term_inspector_widget_, &TermInspectorWidget::term_inspection_requested, this, &MainWindow::term_inspection_requested);
    connect(context_widget_,&ContextWidget::build_context_requested,this, &MainWindow::build_context_requested);
    
    connect(reset_button_, &QPushButton::clicked,this, &MainWindow::reset_application);
    connect(exit_button_, &QPushButton::clicked, this, &MainWindow::exit_application);

    build_timer_ = new QTimer(this);
    build_timer_->setInterval(25);
    connect(build_timer_, &QTimer::timeout, this, &MainWindow::finish_corpus_build);

    update_action_states();}


    //
void MainWindow::set_exit_callback(ExitCallback callback) noexcept { exit_callback_ = callback; }


    //
void MainWindow::add_document() {
    const QString path = QFileDialog::getOpenFileName( this, "Add Document", QString(),
        "Text files (*.txt);;All files (*)");

    if (path.isEmpty()) 
        { return;}

    if (!add_document_file(path)) {
        QMessageBox::warning(
            this, "Add Document Error",
            "Could not add the selected document."); }}

bool MainWindow::add_document_file(const QString& path) {
    const QFileInfo info(path);
    if (!info.isFile())
     {return false;}

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) 
     {return false;}

    const QByteArray contents = file.readAll();
    if (file.error() != QFileDevice::NoError) 
     {return false;}

    const QString title = info.fileName();
    try {
        workspace_.add_document(Document(
            info.absoluteFilePath().toStdString(),
            title.toStdString(),
            QString::fromUtf8(contents).toStdString())); } 
    catch (const std::exception&) 
     {    return false; }

    ++workspace_version_;
    workspace_widget_->add_document(title);
    invalidate_corpus();
    status_label_->setText("Document added. To proceed, build the corpus.");
    return true; }

    //
void MainWindow::build_corpus() {
    if (close_pending_ || workspace_.documents().empty() ||  build_thread_.joinable()) { return; }
    try {
        Workspace snapshot = workspace_;
        std::promise<std::unique_ptr<ProcessingCore>> promise;
        build_result_ =promise.get_future();
        build_version_ =workspace_version_;
        build_cancel_ = std::make_shared<std::atomic<bool>>(false);

        build_thread_ = std::thread( [snapshot = std::move(snapshot), promise = std::move(promise), cancel = build_cancel_, delay_ms = build_delay_ms_]() mutable {
                detail::build_delay_ms = delay_ms;
                detail::build_cancel_flag = cancel.get();

                try {
                    auto core =std::make_unique<ProcessingCore>();
                    core->rebuild(snapshot);
                    promise.set_value_at_thread_exit(std::move(core));
                } catch (const detail::BuildCancelled&) {  promise.set_value_at_thread_exit(nullptr );
                } catch (...){ promise.set_exception_at_thread_exit(std::current_exception());}
            });
    } catch (const std::exception& error) {
        build_result_ = {};
        build_cancel_.reset();
        status_label_->setText( QString("Couldnt start build: %1").arg(QString::fromUtf8( error.what() )));
        return; }

    workspace_widget_->set_corpus_status("Corpus: Building...");
    status_label_->setText("Building...");
    update_action_states();
    build_timer_->start(); }


    //

void MainWindow::finish_corpus_build() {
    if (!build_result_.valid() || build_result_.wait_for(std::chrono::milliseconds{0}) !=std::future_status::ready)
        {  return; }

    build_timer_->stop();
    build_thread_.join();
    build_cancel_.reset();

    if (close_pending_) {
        build_result_ = {};
        close();
        return;}

    try {
        auto core =build_result_.get();
        bool matches_workspace =build_version_ == workspace_version_;

        // Simulate accepting an outdated build.
        #ifdef AIWS_FAULT_ACCEPT_STALE
            matches_workspace = true;
        #endif

        if (core && matches_workspace) {
            processing_core_ =std::move(*core);
            corpus_current_ =true;
            clear_processing_outputs();

            workspace_widget_->set_corpus_status( QString("Corpus: %1 chunks").arg(  static_cast<qulonglong>(processing_core_.chunk_count())));
            status_label_->setText("Corpus built."); } } catch (const std::exception& error) {
        #ifdef AIWS_FAULT_LOSE_FAILURE
            return;
        #endif

        if (build_version_ == workspace_version_) {
            invalidate_corpus();
            status_label_->setText( QString("Corpus build failed: %1").arg(QString::fromUtf8(error.what())));  }  } catch (...) {
        #ifdef AIWS_FAULT_LOSE_FAILURE
            return;
        #endif

        if (build_version_ == workspace_version_) { invalidate_corpus();
            status_label_->setText("Corpus build failed.");}}

    update_action_states();}

MainWindow::~MainWindow() {
    build_timer_->stop();
    request_build_cancellation();
    if (build_thread_.joinable()) { build_thread_.join(); } }

    //
void MainWindow::document_selected(int row) {
    const auto& documents = workspace_.documents();

    if (row < 0 || static_cast<std::size_t>(row) >= documents.size()) {
        workspace_widget_->clear_selected_document();
        return; }

    const auto& document =documents[static_cast<std::size_t>(row)];

    workspace_widget_->show_selected_document(
        QString::fromStdString(document.title()),
        QString::fromStdString(document.text())); }


    //
void MainWindow::search_requested(const QString& query, int k) { 
    if (!corpus_current_ || query.trimmed().isEmpty()) { return; } //enforces that a search is only performed when the corpus is current and the query is not empty  //independent of button state
    

    try {
        auto results = processing_core_.search(query.toStdString(), k);
        search_widget_->set_results(std::move(results));
        term_inspector_widget_->clear_statistics();
        context_widget_->clear_context();

        status_label_->setText("Search complete."); } 
        catch (const std::exception& error) {
        QMessageBox::warning( this, "Search Error",QString::fromUtf8(error.what())); } }




    //
void MainWindow::query_changed(const QString&) { update_action_states(); }



    //
void MainWindow::build_context_requested(int token_budget) {
    if (!corpus_current_ || !search_widget_->has_query()) { return;  }

    try {
        auto items = processing_core_.build_context(
            search_widget_->query().toStdString(),
            search_widget_->result_count(),
            static_cast<std::size_t>(token_budget));

        context_widget_->set_context(items);
        status_label_->setText("Context built.");
    } catch (const std::exception& error) {
        QMessageBox::warning(
            this, "Context Error",
            QString::fromUtf8(error.what())); } }


    //
void MainWindow::source_document_requested( const QString& document_id) {

    const auto& documents = workspace_.documents();
    for (std::size_t row = 0; row < documents.size(); ++row) {
        if (documents[row].id() == document_id.toStdString()) 
            { workspace_widget_->select_document( static_cast<int>(row));
            document_selected(static_cast<int>(row));
            return; } } }



    //
void MainWindow::term_inspection_requested(
    const QString& term, const QString& chunk_id) {

    if (!corpus_current_ || term.trimmed().isEmpty()) { return; }

    try {
        std::size_t df = processing_core_.document_frequency( term.toStdString());
        bool has_chunk = !chunk_id.isEmpty();
        std::size_t tf = 0;

        if (has_chunk) { tf = processing_core_.term_frequency(term.toStdString(), chunk_id.toStdString());}
        term_inspector_widget_->set_statistics( df, has_chunk, tf);

        status_label_->setText("Term inspected.");
        } catch (const std::exception& error) {
        term_inspector_widget_->clear_statistics();

        QMessageBox::warning(this, "Inspection Error",QString::fromUtf8(error.what())); }}

    //
void MainWindow::reset_application() {
    ++workspace_version_;
    request_build_cancellation();
    corpus_current_= false;
    workspace_.clear();
    processing_core_= ProcessingCore();


    Resettable* components[] = {
        workspace_widget_,
        search_widget_,
        term_inspector_widget_,
        context_widget_
    };

    for (Resettable* component : components) {component->reset();} //RESET1 1  // Virtual call through my Resettable*
 
    update_action_states();
    status_label_->setText("Application reset."); }


    //
void MainWindow::exit_application() { if (exit_callback_ != nullptr) { exit_callback_(this); } } //EXIT1 2 // check of stored callback pointer


    //
void MainWindow::update_action_states() { //ADDDOC1 10 ... Enable/disable GUI actions based on current state of workspace and corpus
    workspace_widget_->set_build_corpus_enabled(!workspace_.documents().empty() && !build_thread_.joinable());

    bool can_process =corpus_current_ && search_widget_->has_query();

    search_widget_->set_search_enabled(can_process);
    context_widget_->set_build_context_enabled(can_process);
    term_inspector_widget_->set_inspection_enabled(corpus_current_); }


    //
void MainWindow::invalidate_corpus() {
    corpus_current_ = false;
    clear_processing_outputs(); 

    workspace_widget_->set_corpus_status("Corpus: Not built");
    update_action_states(); }


    //
void MainWindow::clear_processing_outputs() { // ADDDOC1 7 ... Clear dependent GUI state; this does not rebuild the backend
    search_widget_->clear_results();
    term_inspector_widget_->set_selected_chunk(QString());
    term_inspector_widget_->clear_statistics();
    context_widget_->clear_context(); }


    // m4

void MainWindow::set_build_delay_ms(int ms_per_document) 
{    build_delay_ms_ = ms_per_document < 0 ? 0 : ms_per_document; }

void MainWindow::request_build_cancellation() noexcept {
    if (build_cancel_) {   build_cancel_->store(true); } }

void MainWindow::closeEvent(QCloseEvent* event) {
    if (!build_thread_.joinable()) {
        QMainWindow::closeEvent(event);
        return; }

    event->ignore();
    close_pending_ = true;
    request_build_cancellation();
    setEnabled(false);
    status_label_->setText("Closing..."); }


}  // namespace aiws::gui
