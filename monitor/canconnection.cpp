#include "canconnection.h"

#include <QCanBus>

CanConnection::CanConnection(QObject* parent) : QObject(parent) {}
CanConnection::~CanConnection() { disconnectDevice(); }

bool CanConnection::connectDevice(const QString& interfaceName, QString* error) {
    disconnectDevice();
    QString creationError;
    device_.reset(QCanBus::instance()->createDevice(QStringLiteral("socketcan"), interfaceName, &creationError));
    if (!device_) {
        if (error) *error = creationError;
        emit connectionChanged(false, creationError);
        return false;
    }
    connect(device_.get(), &QCanBusDevice::framesReceived, this, &CanConnection::drainFrames);
    connect(device_.get(), &QCanBusDevice::errorOccurred, this, &CanConnection::deviceError);
    if (!device_->connectDevice()) {
        const QString detail = device_->errorString();
        if (error) *error = detail;
        device_.reset();
        emit connectionChanged(false, detail);
        return false;
    }
    emit connectionChanged(true, interfaceName);
    return true;
}

void CanConnection::disconnectDevice() {
    if (device_) device_->disconnectDevice();
    device_.reset();
    emit connectionChanged(false, QStringLiteral("disconnected"));
}

bool CanConnection::sendFrame(quint32 id, const QByteArray& payload, QString* error) {
    if (!device_ || device_->state() != QCanBusDevice::ConnectedState) {
        if (error) *error = QStringLiteral("CAN device is not connected");
        return false;
    }
    QCanBusFrame frame(id, payload.left(8));
    frame.setExtendedFrameFormat(id > 0x7FF);
    if (!device_->writeFrame(frame)) {
        if (error) *error = device_->errorString();
        return false;
    }
    emit frameTransmitted(frame);
    return true;
}

bool CanConnection::isConnected() const {
    return device_ && device_->state() == QCanBusDevice::ConnectedState;
}

void CanConnection::drainFrames() {
    while (device_ && device_->framesAvailable() > 0) emit frameReceived(device_->readFrame());
}

void CanConnection::deviceError(QCanBusDevice::CanBusError error) {
    if (error != QCanBusDevice::NoError && device_) emit errorOccurred(device_->errorString());
}
