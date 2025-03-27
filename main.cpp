#include <QCoreApplication>
#include <QSettings>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include "portlistener.h"
#include "snmpconverter.h"

void configurateSettings(portSettings &port, QSettings &settings) {
    port.name = settings.value("SerialPort/portName", "COM1").toString();
    qDebug() << "Port name:" << port.name;
    port.baudRate = static_cast<QSerialPort::BaudRate>(settings.value("SerialPort/baudRate", 19200).toInt());
    port.dataBits = static_cast<QSerialPort::DataBits>(settings.value("SerialPort/dataBits", 8).toInt());
    QString parityStr = settings.value("SerialPort/parity", "None").toString();
    if (parityStr == "None") port.parityMode = QSerialPort::NoParity;
    else if (parityStr == "Even") port.parityMode = QSerialPort::EvenParity;
    else if (parityStr == "Odd") port.parityMode = QSerialPort::OddParity;
    port.stopBits = static_cast<QSerialPort::StopBits>(settings.value("SerialPort/stopBits", 1).toInt());
    QString flowControlStr = settings.value("SerialPort/flowControl", "None").toString();
    if (flowControlStr == "None") port.flowControlMode = QSerialPort::NoFlowControl;
    else if (flowControlStr == "Hardware") port.flowControlMode = QSerialPort::HardwareControl;
    else if (flowControlStr == "Software") port.flowControlMode = QSerialPort::SoftwareControl;
}

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    // Формируем путь к config.ini
    QString projectDir = QString(PROJECT_DIR);
    QString configPath = projectDir + "/config.ini";
    QSettings settings(configPath, QSettings::IniFormat);

    // Проверяем, существует ли файл
    QFileInfo fileInfo(configPath);
    if (!fileInfo.exists()) {
        qWarning() << "Config file not found at" << configPath;
    } else {
        qDebug() << "Config file found at" << configPath;
    }

    // Проверяем ключи в секции RS485
    settings.beginGroup("RS485");
    QStringList keys = settings.allKeys();
    qDebug() << "Keys in RS485 section:" << keys;
    settings.endGroup();

    // Читаем настройки порта
    portSettings port;
    configurateSettings(port, settings);

    // Читаем настройки SNMP
    QHostAddress snmpIp;
    if (!snmpIp.setAddress(settings.value("SNMP/ipAddress", "127.0.0.1").toString())) {
        qWarning() << "Invalid SNMP IP address, using default: 127.0.0.1";
        snmpIp = QHostAddress("127.0.0.1");
    }
    quint16 snmpPort = settings.value("SNMP/port", 161).toUInt();

    // Читаем маску подсети
    QHostAddress subnetMask;
    if (!subnetMask.setAddress(settings.value("SNMP/subnetMask", "255.255.255.0").toString())) {
        qWarning() << "Invalid SNMP subnet mask, using default: 255.255.255.0";
        subnetMask = QHostAddress("255.255.255.0");
    }
    qDebug() << "Subnet mask set to:" << subnetMask.toString();

    // Читаем шлюз
    QHostAddress gateway;
    if (!gateway.setAddress(settings.value("SNMP/gateway", "0.0.0.0").toString())) {
        qWarning() << "Invalid gateway address, using default: 0.0.0.0 (no gateway)";
        gateway = QHostAddress("0.0.0.0");
    }
    qDebug() << "Gateway set to:" << gateway.toString();

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

    // Создаём объекты
    PortListener *m_port = new PortListener(port);
    SnmpConverter *m_snmp = new SnmpConverter(snmpIp, snmpPort, subnetMask, gateway, listenAddress);

    // Соединяем сигналы и слоты
    QObject::connect(m_port, &PortListener::readedInfo, m_snmp, &SnmpConverter::processSciDataSlot);
    QObject::connect(m_port, &PortListener::errorOccurred, [](const QString &err) {
        qWarning() << "Port Error:" << err;
    });
    QObject::connect(m_snmp, &SnmpConverter::errorOccurred, [](const QString &err) {
        qWarning() << "SNMP Error:" << err;
    });

    // Открываем порт
    m_port->connectPort();

    return a.exec();
}
