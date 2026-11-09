#include "aiws/gui/context_widget.hpp"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QString>
#include <QVBoxLayout>

namespace aiws::gui {


    //
ContextWidget::ContextWidget(QWidget* parent) : QWidget(parent) {
    // DONE: Constructed and laid out the Context area, configured the token-budget
    // control, connected local events, and established initial state.

    auto* box = new QGroupBox("Context", this);
    auto* layout = new QVBoxLayout(box);
    auto* main_layout = new QVBoxLayout(this);
    auto* budget_layout = new QHBoxLayout;


    // range: 50-1000
    //step size : 50
    //starting value: 300
    context_budget_slider_ = new QSlider(Qt::Horizontal, box);
    context_budget_slider_->setRange(50, 1000); //spec requires
    context_budget_slider_->setSingleStep(50);
    context_budget_slider_->setPageStep(50);

    budget_value_label_ = new QLabel(box);

    //--budget
    budget_layout->addWidget(new QLabel("Token budget:", box));
    budget_layout->addWidget(context_budget_slider_, 1);
    budget_layout->addWidget(budget_value_label_);

    build_context_button_ = new QPushButton("Build Context", box);

    context_text_ = new QPlainTextEdit(box);
    context_text_->setReadOnly(true);

    //--layout
    layout->addLayout(budget_layout);
    layout->addWidget(build_context_button_);
    layout->addWidget(context_text_);

   
    main_layout->addWidget(box);

    connect(context_budget_slider_, &QSlider::valueChanged, this, &ContextWidget::budget_changed);
    connect(build_context_button_, &QPushButton::clicked,this, &ContextWidget::request_context);

    reset(); }

    //
int ContextWidget::token_budget() const { return context_budget_slider_->value(); }


    //
void ContextWidget::set_build_context_enabled(bool enabled) { build_context_button_->setEnabled(enabled); }


    //
void ContextWidget::set_context(const std::vector<ContextItem>& items) {
    QString output;

    for (const auto& item : items) {
        if (!output.isEmpty()) { output += "\n\n";}

        output += "Document: " + QString::fromStdString(item.document_id);
        output += "\nChunk: " + QString::number(static_cast<qulonglong>(item.chunk_sequence));
        output += "\nTokens: " + QString::number(static_cast<qulonglong>(item.token_count));
        output += "\nScore: " + QString::number(item.score);
        output += "\nTruncated: ";
        output += item.truncated ? "Yes" : "No";
        output += "\nText:\n" + QString::fromStdString(item.text);
    }

    context_text_->setPlainText(output); }


    //
void ContextWidget::clear_context() {context_text_->clear(); }


    //
void ContextWidget::reset() {
    context_budget_slider_->setValue(300);
    budget_changed(300);
    clear_context();
    set_build_context_enabled(false); }

    //
void ContextWidget::request_context() {
    if (build_context_button_->isEnabled()) 
     { emit build_context_requested(token_budget()); } }

     //
void ContextWidget::budget_changed(int value) { budget_value_label_->setText(QString::number(value)); }


}  // namespace aiws::gui
