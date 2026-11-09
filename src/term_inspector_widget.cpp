#include "aiws/gui/term_inspector_widget.hpp"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace aiws::gui {


    //
TermInspectorWidget::TermInspectorWidget(QWidget* parent) : QWidget(parent) {
    auto* box = new QGroupBox("Term Inspector", this);
    auto* layout = new QVBoxLayout(box);

    term_edit_ = new QLineEdit(box);
    inspect_button_ = new QPushButton("Inspect", box);

    auto* controls = new QHBoxLayout;
    controls->addWidget(new QLabel("Term:", box));
    controls->addWidget(term_edit_);
    controls->addWidget(inspect_button_);

    document_frequency_value_ = new QLabel(box);
    chunk_frequency_value_ = new QLabel(box);

    auto* df_layout = new QHBoxLayout;
    df_layout->addWidget(new QLabel("Document frequency:", box));
    df_layout->addWidget(document_frequency_value_);

    auto* tf_layout = new QHBoxLayout;
    tf_layout->addWidget(new QLabel("Selected-chunk TF:", box));
    tf_layout->addWidget(chunk_frequency_value_);

    layout->addLayout(controls);
    layout->addLayout(df_layout);
    layout->addLayout(tf_layout);
    layout->addStretch();

    auto* main_layout = new QVBoxLayout(this);
    main_layout->addWidget(box);

    connect(inspect_button_, &QPushButton::clicked,
            this, &TermInspectorWidget::request_inspection);

    reset(); }


    //
void TermInspectorWidget::set_inspection_enabled(bool enabled) { inspect_button_->setEnabled(enabled);}


    //
void TermInspectorWidget::set_selected_chunk(const QString& chunk_id) {  //SELECTROW1 6 // receives chunk id from SearchWidget and stores it for later use
    selected_chunk_id_ = chunk_id;
     //  previous TF belongs to the previous selection.
    chunk_frequency_value_->clear(); }



    //
void TermInspectorWidget::set_statistics(std::size_t document_frequency,
                                         bool has_chunk_frequency,
                                         std::size_t chunk_frequency) {

    document_frequency_value_->setText(
    QString::number(static_cast<qulonglong>(document_frequency)));

    if (has_chunk_frequency) 
     { chunk_frequency_value_->setText(QString::number(static_cast<qulonglong>(chunk_frequency)));} 
    else 
     { chunk_frequency_value_->clear();} }


     //
void TermInspectorWidget::clear_statistics() {
    document_frequency_value_->clear();
    chunk_frequency_value_->clear(); }


    //
void TermInspectorWidget::reset() {
    term_edit_->clear();
    selected_chunk_id_.clear();
    clear_statistics();
    set_inspection_enabled(false); }


    //
void TermInspectorWidget::request_inspection() {
     if (inspect_button_->isEnabled() && !term_edit_->text().trimmed().isEmpty()) 
        { emit term_inspection_requested(term_edit_->text(), selected_chunk_id_); } }

}  // namespace aiws::gui
