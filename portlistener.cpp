#include "portlistener.h"
#include <QDebug>

PortListener::PortListener(const portSettings &config, QObject *parent) : QObject(parent) {
    m_serialPort = new QSerialPort(this); // Allocating memory
    writeSettingsPort(config); // Configurate serial port
    connect(m_serialPort, &QSerialPort::readyRead, this, &PortListener::readSerialData);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, [](QSerialPort::SerialPortError error) {
        if (error != QSerialPort::NoError) {
            qWarning() << "Serial error:" << error;
        }
    });
    qDebug() << "PortListener created";
}

PortListener::~PortListener() {
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }
    qDebug() << "PortListener destroyed";
}

void PortListener::writeSettingsPort(const portSettings &s) {
    m_serialPort->setPortName(s.name);
    if (!m_serialPort->setBaudRate(s.baudRate)) {
        throw std::invalid_argument("Invalid baud rate");
    }
    if (!m_serialPort->setDataBits(s.dataBits)) {
        throw std::invalid_argument("Invalid data bits");
    }
    if (!m_serialPort->setParity(s.parityMode)) {
        throw std::invalid_argument("Invalid parity");
    }
    if (!m_serialPort->setStopBits(s.stopBits)) {
        throw std::invalid_argument("Invalid stop bits");
    }
    if (!m_serialPort->setFlowControl(s.flowControlMode)) {
        throw std::invalid_argument("Invalid flow control");
    }
    qDebug() << "Port configured: " << s.name << "at" << s.baudRate << "bps";
}

void PortListener::connectPort() {
    if (m_serialPort->open(QIODevice::ReadOnly)) {
        qDebug() << "Port opened successfully";
    } else {
        QString err = "Failed to open port: " + m_serialPort->errorString();
        qWarning() << err;
        emit errorOccurred(err);
    }
}

void PortListener::readSerialData() {
    QByteArray sciData = m_serialPort->readAll();
    if (!sciData.isEmpty()) {
        qDebug() << "Received SCI data:" << sciData.toHex(' ');
        emit readedInfo(sciData);
    }
}
