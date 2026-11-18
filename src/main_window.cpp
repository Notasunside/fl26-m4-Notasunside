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
#include <QMetaObject>



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

    // build_timer_ = new QTimer(this);
    // build_timer_->setInterval(25);
    // connect(build_timer_, &QTimer::timeout, this, &MainWindow::finish_corpus_build);

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
    if (close_pending_ || workspace_.documents().empty() || build_in_progress_) {return;}

    try {
        auto cancel= std::make_shared<std::atomic<bool>>(false);
        BuildRequest request{ workspace_, workspace_version_, build_delay_ms_, cancel};

        if (!build_thread_.joinable()) { build_thread_ = std::thread(&MainWindow::build_worker_loop, this); }
        {
            std::lock_guard<std::mutex> lock(build_mutex_);
            pending_build_.emplace(std::move(request));
            build_cancel_ = std::move(cancel);
            build_in_progress_ = true;
        }
        build_ready_.notify_one(); } catch (const std::exception& error) {
        status_label_->setText( QString("Couldn't start build: %1").arg(QString::fromUtf8(error.what())));
        return; }

    workspace_widget_->set_corpus_status("Corpus: Building...");
    status_label_->setText("Building...");
    update_action_states(); }

    //m5

    void MainWindow::build_worker_loop() {
    for (;;) { std::unique_lock<std::mutex> lock( build_mutex_ );
        build_ready_.wait(lock , [this] { return build_worker_stopping_ ||pending_build_.has_value(); });
        if (build_worker_stopping_) { return;}

        BuildRequest request= std::move(*pending_build_ );
        pending_build_.reset( );
        lock.unlock();
        auto result= std::make_shared<std::unique_ptr<ProcessingCore>>();
        std::exception_ptr error;
        detail::build_delay_ms= request.delay_ms;
        detail::build_cancel_flag= request.cancel.get( );

        try { if ( !request.cancel->load() ) 
            {
               auto core= std::make_unique<ProcessingCore>();
                core->rebuild(request.snapshot);
                *result= std::move(core); }}
                catch (const detail::BuildCancelled&) {
            //cancelledbuild returs empty result
                } catch ( ... ) { error= std::current_exception(); }
        detail::build_cancel_flag= nullptr;
        QMetaObject::invokeMethod( this, [this,version= request.version,result, error] { handle_build_result( version, std::move( *result ), error );}, Qt::QueuedConnection); }}


    //


    void MainWindow::handle_build_result(std::uint64_t version, std::unique_ptr<ProcessingCore> core, std::exception_ptr error) {
    build_in_progress_ = false;
    build_cancel_.reset();

    if (close_pending_) { return;}
    if (version != workspace_version_) {
        update_action_states();
        return;
    }

    if (error) {
        invalidate_corpus();
        try { std::rethrow_exception(error); } 
        catch (const std::exception& failure) {
            status_label_->setText( QString("Corpus build failed: %1").arg(QString::fromUtf8(failure.what()))); } 
            catch (...) { status_label_->setText("Corpus build failed."); } } 
            else if (core) {
            processing_core_= std::move(*core);
            corpus_current_ =true;
        clear_processing_outputs();
        workspace_widget_->set_corpus_status( QString("Corpus: %1 chunks").arg(static_cast<qulonglong>(processing_core_.chunk_count())));
        status_label_->setText("Corpus built."); }
    update_action_states(); }

MainWindow::~MainWindow() { stop_build_worker(); }

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
        workspace_widget_->set_build_corpus_enabled( !close_pending_ && !workspace_.documents().empty() && !build_in_progress_ );

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

    //m5
void MainWindow::stop_build_worker() {
    request_build_cancellation();
    {
        std::lock_guard<std::mutex> lock(build_mutex_);
        build_worker_stopping_= true;
        pending_build_.reset();
    }

    build_ready_.notify_one();
    if (build_thread_.joinable()) { build_thread_.join(); } }

void MainWindow::closeEvent(QCloseEvent* event) {
    close_pending_ = true;
    stop_build_worker();
    QMainWindow::closeEvent(event); }


}  // namespace aiws::gui
