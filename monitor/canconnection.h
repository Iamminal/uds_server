#pragma once

#include <QCanBusDevice>
#include <QCanBusFrame>
#include <QObject>
#include <QString>

#include <memory>

class CanConnection : public QObject {
    Q_OBJECT
public:
    explicit CanConnection(QObject* parent = nullptr);
    ~CanConnection() override;

    bool connectDevice(const QString& interfaceName, QString* error = nullptr);
    void disconnectDevice();
    bool sendFrame(quint32 id, const QByteArray& payload, QString* error = nullptr);
    bool isConnected() const;

signals:
    void frameReceived(const QCanBusFrame& frame);
    void frameTransmitted(const QCanBusFrame& frame);
    void connectionChanged(bool connected, const QString& detail);
    void errorOccurred(const QString& error);

private slots:
    void drainFrames();
    void deviceError(QCanBusDevice::CanBusError error);

private:
    std::unique_ptr<QCanBusDevice> device_;
};
