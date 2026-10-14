#include "settingsdialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("SocketCAN settings"));
    interface_ = new QLineEdit(QStringLiteral("vcan0"), this);
    requestId_ = new QLineEdit(QStringLiteral("0x7E0"), this);
    responseId_ = new QLineEdit(QStringLiteral("0x7E8"), this);
    auto* validator = new QRegularExpressionValidator(QRegularExpression(QStringLiteral("(?:0[xX])?[0-9A-Fa-f]{1,8}")), this);
    requestId_->setValidator(validator);
    responseId_->setValidator(validator);
    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Interface"), interface_);
    form->addRow(QStringLiteral("Request ID"), requestId_);
    form->addRow(QStringLiteral("Response ID"), responseId_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

QString SettingsDialog::interfaceName() const { return interface_->text().trimmed(); }
quint32 SettingsDialog::requestId() const { return requestId_->text().toUInt(nullptr, 0); }
quint32 SettingsDialog::responseId() const { return responseId_->text().toUInt(nullptr, 0); }

void SettingsDialog::setValues(const QString& interfaceName, quint32 requestId, quint32 responseId) {
    interface_->setText(interfaceName);
    requestId_->setText(QStringLiteral("0x%1").arg(requestId, 0, 16).toUpper());
    responseId_->setText(QStringLiteral("0x%1").arg(responseId, 0, 16).toUpper());
}
