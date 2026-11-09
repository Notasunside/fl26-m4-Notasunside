#include "aiws/gui/workspace_widget.hpp"


//holy yap bro
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace aiws::gui {

WorkspaceWidget::WorkspaceWidget(QWidget* parent) : QWidget(parent) {
    // Constructed and laid out the Workspace and Selected Document areas,
    // connected local widget events, and establish initial presentation state.
    auto* workspace_box = new QGroupBox("My Workspace", this);
    auto* selected_box = new QGroupBox("Selected Document", this);

    document_list_ = new QListWidget(workspace_box);
    add_document_button_ = new QPushButton("Add Document", workspace_box);
    build_corpus_button_ = new QPushButton("Build Corpus", workspace_box);
    corpus_status_label_ = new QLabel(workspace_box);

    auto* button_layout = new QHBoxLayout;
    button_layout->addWidget(add_document_button_);
    button_layout->addWidget(build_corpus_button_);

    auto* workspace_layout = new QVBoxLayout(workspace_box);
    workspace_layout->addWidget(document_list_);
    workspace_layout->addLayout(button_layout);
    workspace_layout->addWidget(corpus_status_label_);

    document_title_label_ = new QLabel(selected_box);
    document_title_label_->setWordWrap(true);

    document_text_ = new QPlainTextEdit(selected_box);
    document_text_->setReadOnly(true);

    auto* selected_layout = new QVBoxLayout(selected_box);
    selected_layout->addWidget(document_title_label_);
    selected_layout->addWidget(document_text_);

    auto* main_layout = new QHBoxLayout(this);
    main_layout->addWidget(workspace_box, 1);
    main_layout->addWidget(selected_box, 1);

    connect(add_document_button_, &QPushButton::clicked, this, &WorkspaceWidget::add_document_requested); //ADDDOC1 1 announce request to MainWindow 
    connect(build_corpus_button_, &QPushButton::clicked, this, &WorkspaceWidget::build_corpus_requested);
    connect(document_list_, &QListWidget::currentRowChanged, this, &WorkspaceWidget::document_selected);

    reset(); 
}

void WorkspaceWidget::add_document(const QString& title) {
    document_list_->addItem(title);
    set_build_corpus_enabled(true); }

void WorkspaceWidget::select_document(int row) {
    document_list_->setCurrentRow(row); }

void WorkspaceWidget::show_selected_document(const QString& title,
                                              const QString& text) {
    document_title_label_->setText(title);
    document_text_->setPlainText(text); }

void WorkspaceWidget::clear_selected_document() {
    document_title_label_->clear();
    document_text_->clear(); }

void WorkspaceWidget::set_corpus_status(const QString& text) {
    corpus_status_label_->setText(text); }

void WorkspaceWidget::set_build_corpus_enabled(bool enabled) {
    build_corpus_button_->setEnabled(enabled); }

void WorkspaceWidget::reset() { 
    document_list_->clear();
    clear_selected_document();
    set_corpus_status("Corpus: Not built");
    set_build_corpus_enabled(false);
    add_document_button_->setEnabled(true); }

}  // namespace aiws::gui

//10/23: this builds successfully
