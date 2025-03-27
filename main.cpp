#include <QSettings>
#include <QCoreApplication>
#include <QDebug>
#include "portlistener.h"
#include "snmpconverter.h"


void configurateSettings(portSettings &port,  QSettings &settings){


    port.name = settings.value("SerialPort/portName", "/dev/ttyUSB0").toString();
    qDebug() << "Port name:" << port.name;
    port.baudRate = static_cast<QSerialPort::BaudRate>(settings.value("SerialPort/baudRate", 19200).toInt());
    port.dataBits = static_cast<QSerialPort::DataBits>(settings.value("SerialPort/dataBits", 8).toInt());
    QString parityStr = settings.value("SerialPort/parity", "None").toString();
    if (parityStr == "None") port.parityMode = QSerialPort::NoParity;
    else if (parityStr == "Even") port.parityMode = QSerialPort::EvenParity;
    else if (parityStr == "Odd") port.parityMode = QSerialPort::OddParity;
    // Add more parity options as needed
    port.stopBits = static_cast<QSerialPort::StopBits>(settings.value("SerialPort/stopBits", 1).toInt());
    QString flowControlStr = settings.value("SerialPort/flowControl", "None").toString();
    if (flowControlStr == "None") port.flowControlMode = QSerialPort::NoFlowControl;
    else if (flowControlStr == "Hardware") port.flowControlMode = QSerialPort::HardwareControl;
    else if (flowControlStr == "Software") port.flowControlMode = QSerialPort::SoftwareControl;


    settings.beginGroup("SerialPort");
    QStringList keys = settings.allKeys();
    qDebug() << "Keys in SerialPort:" << keys;
    settings.endGroup();
}

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    QString projectDir = QString(PROJECT_DIR);
    QString configPath = projectDir + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);

    portSettings port;
    configurateSettings(port, settings);


    PortListener *m_port = new PortListener(port);
    QHostAddress snmpIp = QHostAddress(settings.value("SNMP/ipAddress", "127.0.0.1").toString());
    quint16 snmpPort = settings.value("SNMP/port", 161).toUInt();

    // Читаем listenAddress
    QString listenAddressStr = settings.value("RS485/listenAddress", "all").toString();
    qDebug() << "Raw listenAddress from config:" << listenAddressStr;
    int listenAddress = -1;
    if (listenAddressStr != "all") {
        bool ok;
        QString cleanedAddress = listenAddressStr.startsWith("0x") ? listenAddressStr.mid(2) : listenAddressStr;
        qDebug() << "Cleaned address:" << cleanedAddress;
        listenAddress = cleanedAddress.toUInt(&ok, 16);
        if (!ok) {
            qWarning() << "Invalid RS485 listen address:" << listenAddressStr << ", using 'all'";
            listenAddress = -1;
        }
    }
    qDebug() << "Listen address set to:" << listenAddress;

    SnmpConverter *m_snmp = new SnmpConverter(snmpIp, snmpPort, listenAddress);

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
