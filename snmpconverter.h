#ifndef SNMPCONVERTER_H
#define SNMPCONVERTER_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include "tools.h"

class SnmpConverter : public QObject {
    Q_OBJECT
public:
    explicit SnmpConverter(const QHostAddress &udpAddress, quint16 udpPort, QObject *parent = nullptr);
    ~SnmpConverter();

private:
    QUdpSocket *m_udpSocket;
    QHostAddress m_udpAddress;
    quint16 m_udpPort;
    QByteArray snmpPacket;
    QString community = "public";  // SNMP community string
    uint32_t requestId = 1;        // SNMP request ID, starts at 1

    // Reading SCI packet
    SCIPacket readSCI(const QByteArray &sciData);
    /*
     * Calculate CRC byte
     * Return: calculated CRC
    */
    uint8_t calculateCRC(const QByteArray &data);
    /*
    *Receive readed data from port
    *
    */
    void processSciData(const QByteArray &sciData);
    void buildSnmpPacket(const QByteArray &oid, int32_t value, int valueType, bool isSigned = false);
    void sendSnmpPacket();
    void encodeLength(QByteArray &buffer, int length);
    void encodeOID(QByteArray &buffer, const QByteArray &oid);
    void encodeInteger(QByteArray &buffer, int32_t value, bool isSigned);

public slots:
    void processSciDataSlot(const QByteArray &sciData);

signals:
    void snmpPacketSent(const QByteArray &packet);
    void errorOccurred(const QString &err);
};

#endif // SNMPCONVERTER_H
