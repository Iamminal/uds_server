#include "mainwindow.h"

#include "canconnection.h"
#include "canframemodel.h"
#include "filterproxymodel.h"
#include "framedelegate.h"
#include "hexutil.h"
#include "settingsdialog.h"
#include "trafficstatistics.h"
#include "udsclient.h"
#include "udsdecoder.h"
#include "ui_mainwindow.h"

#include <QCanBusFrame>
#include <QDateTime>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui_(new Ui::MainWindow), connection_(new CanConnection(this)),
      model_(new CanFrameModel(this)), proxy_(new FilterProxyModel(this)),
      client_(new UdsClient(connection_, this)), statistics_(new TrafficStatistics(this)) {
    ui_->setupUi(this);
    proxy_->setSourceModel(model_);
    ui_->frameView->setModel(proxy_);
    ui_->frameView->setItemDelegate(new FrameDelegate(ui_->frameView));
    ui_->frameView->setSortingEnabled(true);
    ui_->frameView->horizontalHeader()->setStretchLastSection(true);

    connect(ui_->connectButton, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    connect(ui_->configureButton, &QPushButton::clicked, this, &MainWindow::showSettings);
    connect(ui_->sendButton, &QPushButton::clicked, this, &MainWindow::sendRequest);
    connect(ui_->clearButton, &QPushButton::clicked, model_, &CanFrameModel::clear);
    connect(ui_->clearButton, &QPushButton::clicked, statistics_, &TrafficStatistics::reset);
    connect(ui_->filterEdit, &QLineEdit::textChanged, proxy_, &FilterProxyModel::setSearchText);
    connect(ui_->extendedButton, &QPushButton::clicked, this, [this] { queueHex(QStringLiteral("10 03")); });
    connect(ui_->vinButton, &QPushButton::clicked, this, [this] { queueHex(QStringLiteral("22 F1 90")); });
    connect(ui_->dtcButton, &QPushButton::clicked, this, [this] { queueHex(QStringLiteral("19 02 FF")); });
    connect(connection_, &CanConnection::frameReceived, this, [this](const auto& frame) { observeFrame(frame, false); });
    connect(connection_, &CanConnection::frameTransmitted, this, [this](const auto& frame) { observeFrame(frame, true); });
    connect(connection_, &CanConnection::connectionChanged, this, [this](bool connected, const QString& detail) {
        ui_->connectButton->setText(connected ? QStringLiteral("Disconnect") : QStringLiteral("Connect"));
        ui_->connectionLabel->setText(connected ? QStringLiteral("Connected: %1").arg(detail) : detail);
    });
    connect(connection_, &CanConnection::errorOccurred, this, [this](const QString& error) {
        statusBar()->showMessage(error, 5000);
    });
    connect(client_, &UdsClient::responseReady, this, &MainWindow::displayResponse);
    connect(client_, &UdsClient::transportError, this, [this](const QString& error) {
        QMessageBox::warning(this, QStringLiteral("ISO-TP error"), error);
    });
    connect(statistics_, &TrafficStatistics::changed, ui_->statisticsLabel, &QLabel::setText);
    statistics_->reset();
}

MainWindow::~MainWindow() { delete ui_; }

quint32 MainWindow::requestId() const { return ui_->requestIdEdit->text().toUInt(nullptr, 0); }
quint32 MainWindow::responseId() const { return ui_->responseIdEdit->text().toUInt(nullptr, 0); }

void MainWindow::toggleConnection() {
    if (connection_->isConnected()) {
        connection_->disconnectDevice();
        return;
    }
    QString error;
    if (!connection_->connectDevice(ui_->interfaceEdit->text().trimmed(), &error)) {
        QMessageBox::critical(this, QStringLiteral("SocketCAN connection failed"), error);
        return;
    }
    client_->setIdentifiers(requestId(), responseId());
}

void MainWindow::sendRequest() {
    bool valid = false;
    const QByteArray payload = HexUtil::parse(ui_->requestEdit->text(), &valid);
    if (!valid) {
        QMessageBox::warning(this, QStringLiteral("Invalid request"), QStringLiteral("Enter complete hexadecimal bytes, for example: 22 F1 90"));
        return;
    }
    client_->setIdentifiers(requestId(), responseId());
    QString error;
    if (!client_->sendRequest(payload, &error)) QMessageBox::warning(this, QStringLiteral("Cannot send request"), error);
}

void MainWindow::queueHex(const QString& bytes) {
    ui_->requestEdit->setText(bytes);
    sendRequest();
}

void MainWindow::showSettings() {
    SettingsDialog dialog(this);
    dialog.setValues(ui_->interfaceEdit->text(), requestId(), responseId());
    if (dialog.exec() != QDialog::Accepted) return;
    ui_->interfaceEdit->setText(dialog.interfaceName());
    ui_->requestIdEdit->setText(QStringLiteral("0x%1").arg(dialog.requestId(), 0, 16).toUpper());
    ui_->responseIdEdit->setText(QStringLiteral("0x%1").arg(dialog.responseId(), 0, 16).toUpper());
    client_->setIdentifiers(dialog.requestId(), dialog.responseId());
}

void MainWindow::observeFrame(const QCanBusFrame& frame, bool transmitted) {
    model_->append(frame, transmitted, UdsDecoder::decodeCanFrame(frame.payload()));
    if (transmitted) statistics_->recordTx(frame.payload().size());
    else statistics_->recordRx(frame.payload().size());

    if (const auto message = monitorReassembler_.ingest(static_cast<quint32>(frame.frameId()), frame.payload())) {
        const QString direction = transmitted ? QStringLiteral("TX") : QStringLiteral("RX");
        ui_->messageLog->addItem(QStringLiteral("%1  %2  %3")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), direction,
                 UdsDecoder::decodeMessage(*message)));
        ui_->messageLog->scrollToBottom();
    }
    if (ui_->autoScrollCheck->isChecked()) ui_->frameView->scrollToBottom();
}

void MainWindow::displayResponse(const QByteArray& response) {
    statusBar()->showMessage(QStringLiteral("UDS response: %1 — %2")
        .arg(HexUtil::format(response), UdsDecoder::decodeMessage(response)), 8000);
}
