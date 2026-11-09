#include "aiws/gui/search_widget.hpp"


#include "aiws/gui/search_result_model.hpp"

#include <QAbstractItemView>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QVBoxLayout>

#include <utility>


namespace aiws::gui {


    //
SearchWidget::SearchWidget(QWidget* parent) : QWidget(parent) {
    // Constructed and laid out the Query, Search Results, and Selected Result
    // areas; configured the table/model; connected local events; established initial state.
    auto* query_box = new QGroupBox("Query", this);

        auto* layout = new QVBoxLayout(query_box);

        //class member variable
    query_edit_ = new QLineEdit(query_box);

    result_count_spin_ = new QSpinBox(query_box);
    result_count_spin_->setRange(1, 100);

    search_button_ = new QPushButton("Search", query_box);

    auto* controls = new QHBoxLayout;
    controls->addWidget(new QLabel("Query:", query_box));
    controls->addWidget(query_edit_, 1);
    controls->addWidget(new QLabel("Results:", query_box));
    controls->addWidget(result_count_spin_);
    controls->addWidget(search_button_);

    result_model_ = new SearchResultModel(this);
    results_view_ = new QTableView(query_box);
    results_view_->setModel(result_model_);

    //new addition
    //fixed: cannot scroll through search results
    results_view_->setMinimumHeight(120);
    results_view_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    results_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    results_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    //--<QHeaderView> added after building
    results_view_->setWordWrap(false); 
    results_view_->horizontalHeader()->setStretchLastSection(true);

    selected_result_text_ = new QPlainTextEdit(query_box);
    selected_result_text_->setReadOnly(true);

    //fix: cannot scroll through selected result text
    selected_result_text_->setMinimumHeight(100);

    view_source_button_ = new QPushButton("View Source", query_box);

    layout->addLayout(controls);
    layout->addWidget(new QLabel("Search Results", query_box));
    layout->addWidget(results_view_, 2);
    layout->addWidget(new QLabel("Selected Result", query_box));
    layout->addWidget(selected_result_text_, 1);
    
    //fix: moved view source button to the bottom of the search widget
    layout->addSpacing(8);

    layout->addWidget(view_source_button_);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->addWidget(query_box);

    connect(search_button_, &QPushButton::clicked, this, &SearchWidget::request_search);
    connect(query_edit_, &QLineEdit::textChanged,this, &SearchWidget::query_changed);
    connect(results_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SearchWidget::show_selected_result);  //SELECTROW1 2 // connects the section changed signal to the show_selected_result slot

    connect(view_source_button_, &QPushButton::clicked,this, &SearchWidget::request_source_document);

    reset();
}

QString SearchWidget::query() const { return query_edit_->text();}


    //
int SearchWidget::result_count() const { return result_count_spin_->value(); }


    //
bool SearchWidget::has_query() const { return !query().trimmed().isEmpty(); }


    //
void SearchWidget::set_search_enabled(bool enabled) { search_button_->setEnabled(enabled); }


    //
void SearchWidget::set_results(std::vector<SearchResult> results) { //SEARCH1 4 //	Updates the model and clears the old selection/details; disables View Source.
     result_model_->set_results(std::move(results));
    results_view_->selectionModel()->clear(); //passes to SearchResultModel for storage
    selected_result_text_->clear();
    view_source_button_->setEnabled(false);
    emit selected_chunk_changed(QString()); }



    //
void SearchWidget::clear_results() { set_results({}); }


    //
void SearchWidget::reset() {
    query_edit_->clear();
    result_count_spin_->setValue(3);
    clear_results();
    set_search_enabled(false); }


    //SEARCH1 
void SearchWidget::request_search() {
     if (search_button_->isEnabled() && has_query()) 
        { emit search_requested(query(), result_count()); }} //SEARCH1 1


    // 
void SearchWidget::show_selected_result() { 
    const auto rows = results_view_->selectionModel()->selectedRows(); //SELECTROW1 1 

    const SearchResult* result = nullptr;
    if (!rows.isEmpty()) 
     { result = result_model_->result_at(rows.first().row());} //SELECTROW1 3 // get the first selected row and retrieve the corresponding SearchResult from the model

    if (result == nullptr) {
        selected_result_text_->clear();
        view_source_button_->setEnabled(false);
        emit selected_chunk_changed(QString()); // //SELECTROW1 4 // emit signal with empty chunk_id when no result is selected
        return; }

    QString details;
    details += "Document: " + QString::fromStdString(result->document_id);
    details += "\nChunk ID: " + QString::fromStdString(result->chunk_id);
    details += "\nChunk: " + QString::number(static_cast<qulonglong>(result->chunk_sequence));
    details += "\nScore: " + QString::number(result->score);
    details += "\nMatched terms: " + QString::number(static_cast<qulonglong>(result->matched_terms));
    details += "\nText:\n" + QString::fromStdString(result->text);

    selected_result_text_->setPlainText(details);
    view_source_button_->setEnabled(true);

    emit selected_chunk_changed(QString::fromStdString(result->chunk_id)); }


    //
void SearchWidget::request_source_document() {
    const auto rows = results_view_->selectionModel()->selectedRows();
    if (rows.isEmpty()) 
     { return; }

    const SearchResult* result =result_model_->result_at(rows.first().row()); 
    if (result != nullptr)
     {emit source_document_requested(QString::fromStdString(result->document_id));} }

}  // namespace aiws::gui
