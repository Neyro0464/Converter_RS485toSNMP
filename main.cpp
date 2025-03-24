#include <QCoreApplication>
#include <QDebug>
#include "portlistener.h"
#include "snmpconverter.h"

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    portSettings port;
    port.name = "COM1"; // "/dev/ttyUSB0" - UNIX
    port.baudRate = QSerialPort::Baud19200;
    port.dataBits = QSerialPort::Data8;
    port.parityMode = QSerialPort::NoParity;
    port.stopBits = QSerialPort::OneStop;
    port.flowControlMode = QSerialPort::NoFlowControl;

    PortListener *m_port = new PortListener(port);
    SnmpConverter *m_snmp = new SnmpConverter(QHostAddress("127.0.0.1"), 161); // Adjust IP and port as needed

    QObject::connect(m_port, &PortListener::readedInfo, m_snmp, &SnmpConverter::processSciDataSlot);
    QObject::connect(m_port, &PortListener::errorOccurred, [](const QString &err) {
        qWarning() << "Port Error:" << err;
    });
    QObject::connect(m_snmp, &SnmpConverter::errorOccurred, [](const QString &err) {
        qWarning() << "SNMP Error:" << err;
    });

    m_port->connectPort();

    return a.exec();
}
