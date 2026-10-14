#pragma once

#include <QDialog>

class QLineEdit;
class QSpinBox;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    QString interfaceName() const;
    quint32 requestId() const;
    quint32 responseId() const;
    void setValues(const QString& interfaceName, quint32 requestId, quint32 responseId);

private:
    QLineEdit* interface_;
    QLineEdit* requestId_;
    QLineEdit* responseId_;
};
