#pragma once

#include "isotpreassembler.h"

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QCanBusFrame;
QT_END_NAMESPACE

class CanConnection;
class CanFrameModel;
class FilterProxyModel;
class TrafficStatistics;
class UdsClient;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void toggleConnection();
    void sendRequest();
    void showSettings();
    void displayResponse(const QByteArray& response);

private:
    Ui::MainWindow* ui_;
    CanConnection* connection_;
    CanFrameModel* model_;
    FilterProxyModel* proxy_;
    UdsClient* client_;
    TrafficStatistics* statistics_;
    IsoTpReassembler monitorReassembler_;

    void observeFrame(const QCanBusFrame& frame, bool transmitted);
    quint32 requestId() const;
    quint32 responseId() const;
    void queueHex(const QString& bytes);
};
