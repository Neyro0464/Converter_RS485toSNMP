#include "snmpconverter.h"
#include <QDebug>

SnmpConverter::SnmpConverter(const QHostAddress &udpAddress, quint16 udpPort, QObject *parent)
    : QObject(parent), m_udpSocket(new QUdpSocket(this)), m_udpAddress(udpAddress), m_udpPort(udpPort) {
    qDebug() << "SnmpConverter created for" << udpAddress.toString() << ":" << udpPort;
}

SnmpConverter::~SnmpConverter() {
    qDebug() << "SnmpConverter destroyed";
}


uint8_t SnmpConverter::calculateCRC(const QByteArray &data) {
    uint8_t crc = 0;
    for (int i = 1; i < data.size() - 2; ++i) { // Skip STX and ETX
        crc ^= static_cast<uint8_t>(data[i]);
    }
    return ~crc;
}


SCIPacket SnmpConverter::readSCI(const QByteArray &sciData) {
    if (sciData.size() < 5) {
        throw std::runtime_error("SCI packet too short");
    }
    if (sciData[0] != STX || sciData[sciData.size() - 1] != ETX) {
        throw std::runtime_error("Invalid STX/ETX");
    }

    SCIPacket pack;
    pack.destSrc = sciData[1]; // high nibble is detination; low nibble is source
    pack.cmd = (sciData[2] >> 4) & 0x0F; // high nibble of the byte - command
    pack.len = sciData[2] & 0x0F; // low nibble of the byte - length of data
    pack.crc = sciData[sciData.size() - 2];

    if (sciData.size() != pack.len + 5) {
        throw std::runtime_error("Invalid data length");
    }

    pack.data.clear();
    for (int i = 3; i < 3 + pack.len; ++i) { // skip STX, destSrc and CmdLen bytes
        pack.data.push_back(static_cast<uint8_t>(sciData[i]));
    }

    uint8_t calcCrc = calculateCRC(sciData); // calculate CRC
    if (calcCrc != pack.crc) {
        throw std::runtime_error("CRC mismatch: expected " + std::to_string(pack.crc) + ", got " + std::to_string(calcCrc));
    }

    return pack;
}


void SnmpConverter::processSciData(const QByteArray &sciData) {
    try {
        SCIPacket pack = readSCI(sciData);
        qDebug() << "SCI Packet - Src:" << (pack.destSrc & 0x0F)
                 << "Cmd:" << pack.cmd
                 << "Data:" << QByteArray::fromRawData(reinterpret_cast<const char*>(pack.data.data()), pack.data.size()).toHex(' ');

        // Extracting the source address (unit)
        uint8_t src = pack.destSrc & 0x0F;
        QString unitPrefix = "2B06010401E2F704"; // Fixed OID prefix: 1.3.6.1.4.1.58039.4
        QString muteSuffix, summaryAlarmSuffix, tempAlarmSuffix, tempSuffix, gainSuffix, powerSuffix;

        // Defining OID suffixes depending on the unit
        if (src == 0xA) { // PA A
            muteSuffix = "08";          // unitquery.8
            summaryAlarmSuffix = "0A";  // unitquery.10
            tempAlarmSuffix = "0C";     // unitquery.12
            tempSuffix = "05";          // unitquery.5
            gainSuffix = "07";          // unitquery.7
            powerSuffix = "03";         // unitquery.3
        } else if (src == 0xB) { // PA B
            muteSuffix = "19";          // unitquery.25
            summaryAlarmSuffix = "1B";  // unitquery.27
            tempAlarmSuffix = "1D";     // unitquery.29
            tempSuffix = "16";          // unitquery.22
            gainSuffix = "18";          // unitquery.24
            powerSuffix = "14";         // unitquery.20
        } else if (src == 0xC) { // PA C
            muteSuffix = "2E";          // unitquery.46
            summaryAlarmSuffix = "30";  // unitquery.48
            tempAlarmSuffix = "32";     // unitquery.50
            tempSuffix = "2B";          // unitquery.43
            gainSuffix = "2D";          // unitquery.45
            powerSuffix = "29";         // unitquery.41
        } else {
            qWarning() << "Unknown source address:" << src;
            return;
        }

        // Checking the UPD (0x8) command and the minimum data length
        if (pack.cmd == 0x8 && pack.data.size() >= 2 && pack.data[0] == 0xFF && pack.data[1] == 0x09) {
            if (pack.data.size() < 11) {
                qWarning() << "UPD packet too short";
                return;
            }

            // Forming an OID for all parameters
            QByteArray muteOid = QByteArray::fromHex((unitPrefix + muteSuffix).toLatin1());
            QByteArray summaryAlarmOid = QByteArray::fromHex((unitPrefix + summaryAlarmSuffix).toLatin1());
            QByteArray tempAlarmOid = QByteArray::fromHex((unitPrefix + tempAlarmSuffix).toLatin1());
            QByteArray tempOid = QByteArray::fromHex((unitPrefix + tempSuffix).toLatin1());
            QByteArray gainOid = QByteArray::fromHex((unitPrefix + gainSuffix).toLatin1());
            QByteArray powerOid = QByteArray::fromHex((unitPrefix + powerSuffix).toLatin1());

            // Mute Status
            int32_t mute = static_cast<int32_t>(pack.data[2]);
            buildSnmpPacket(muteOid, mute, 0x02, false);
            sendSnmpPacket();

            // Summary Alarm
            int32_t summaryAlarm = (pack.data[3] & 0x80) ? 1 : 0;
            buildSnmpPacket(summaryAlarmOid, summaryAlarm, 0x02, false);
            sendSnmpPacket();

            // Temperature Alarm
            int32_t tempAlarm = (pack.data[4] & 0x04) ? 1 : 0;
            buildSnmpPacket(tempAlarmOid, tempAlarm, 0x02, false);
            sendSnmpPacket();

            // Temperature
            int16_t tempRaw = static_cast<int16_t>((pack.data[5] << 8) | pack.data[6]);
            int32_t temp = static_cast<int32_t>(tempRaw);
            buildSnmpPacket(tempOid, temp, 0x02, true);
            sendSnmpPacket();

            // Gain
            uint16_t gainRaw = (static_cast<uint8_t>(pack.data[7]) << 8) | static_cast<uint8_t>(pack.data[8]);
            int32_t gain = static_cast<int32_t>(gainRaw);
            buildSnmpPacket(gainOid, gain, 0x02, false);
            sendSnmpPacket();

            // Output Power
            uint16_t powerRaw = (static_cast<uint8_t>(pack.data[9]) << 8) | static_cast<uint8_t>(pack.data[10]);
            int32_t power = static_cast<int32_t>(powerRaw);
            buildSnmpPacket(powerOid, power, 0x02, false);

            sendSnmpPacket();
        } else {
            qWarning() << "Unsupported SCI command:" << pack.cmd;
        }
    } catch (const std::exception &e) {
        QString err = "SCI processing error: " + QString(e.what());
        qWarning() << err;
        emit errorOccurred(err);
    }
}


void SnmpConverter::processSciDataSlot(const QByteArray &sciData) {
    processSciData(sciData);
}

void SnmpConverter::encodeLength(QByteArray &buffer, int length) {
    if (length < 128) {
        buffer.append(static_cast<char>(length));
    } else {
        QByteArray lenBytes;
        while (length > 0) {
            lenBytes.prepend(static_cast<char>(length & 0xFF));
            length >>= 8;
        }
        buffer.append(static_cast<char>(0x80 | lenBytes.size()));
        buffer.append(lenBytes);
    }
}

void SnmpConverter::encodeOID(QByteArray &buffer, const QByteArray &oid) {
    buffer.append(0x06); // OID tag
    encodeLength(buffer, oid.size());
    buffer.append(oid);
}

void SnmpConverter::encodeInteger(QByteArray &buffer, int32_t value, bool isSigned) {
    buffer.append(0x02); // INTEGER tag
    QByteArray intBytes;

    if (value == 0) {
        intBytes.append(static_cast<char>(0x00));
    } else {
        if (isSigned) {
            //Signed value
            bool isNegative = value < 0;
            uint32_t absValue = isNegative ? -value : value;
            while (absValue > 0) {
                intBytes.prepend(static_cast<char>(absValue & 0xFF));
                absValue >>= 8;
            }
            if (isNegative && (intBytes[0] & 0x80)) {
                intBytes.prepend(0xFF); // Adding a leading 0xFF for a negative number
            } else if (!isNegative && (intBytes[0] & 0x80)) {
                intBytes.prepend(static_cast<char>(0x00)); // Adding a leading 0x00 for a positive number
            }
        } else {
            // Unsigned value
            uint32_t absValue = static_cast<uint32_t>(value);
            while (absValue > 0) {
                intBytes.prepend(static_cast<char>(absValue & 0xFF));
                absValue >>= 8;
            }
            if (intBytes[0] & 0x80) {
                intBytes.prepend(static_cast<char>(0x00)); // Add a leading 0x00 to avoid interpretation as negative
            }
        }
    }

    encodeLength(buffer, intBytes.size());
    buffer.append(intBytes);
}
/*
 * Building SNMP packet
 * QByteArray &oid - oid prefix
 * int32_t value
 * int valueType - if INTEGER -> encode
 * bool isSigned - to cast in rigth type
 */
void SnmpConverter::buildSnmpPacket(const QByteArray &oid, int32_t value, int valueType, bool isSigned){
    snmpPacket.clear();
    snmpPacket.append(0x30); // Sequence
    int totalLenPos = snmpPacket.size();
    snmpPacket.append(static_cast<char>(0x00)); // Placeholder for total length

    // Version (SNMPv2c)
    snmpPacket.append(0x02); // INTEGER
    snmpPacket.append(0x01); // Length
    snmpPacket.append(static_cast<char>(0x00)); // Version 1

    // Community
    snmpPacket.append(0x04); // OCTET STRING
    snmpPacket.append(static_cast<char>(community.length()));
    snmpPacket.append(community.toLatin1());

    // PDU (GetResponse)
    snmpPacket.append(0xA2); // GetResponse-PDU
    int pduLenPos = snmpPacket.size();
    snmpPacket.append(static_cast<char>(0x00)); // Placeholder for PDU length

    // Request ID
    snmpPacket.append(0x02); // INTEGER
    snmpPacket.append(0x04); // Length 4
    snmpPacket.append((requestId >> 24) & 0xFF);
    snmpPacket.append((requestId >> 16) & 0xFF);
    snmpPacket.append((requestId >> 8) & 0xFF);
    snmpPacket.append(requestId & 0xFF);
    requestId++; // Increment for the next packet

    // Error Status
    snmpPacket.append(0x02); // INTEGER
    snmpPacket.append(0x01); // Length
    snmpPacket.append(static_cast<char>(0x00)); // No error

    // Error Index
    snmpPacket.append(0x02); // INTEGER
    snmpPacket.append(0x01); // Length
    snmpPacket.append(static_cast<char>(0x00)); // Error index 0

    // VarBindList
    snmpPacket.append(0x30); // Sequence
    int varBindListLenPos = snmpPacket.size();
    snmpPacket.append(static_cast<char>(0x00)); // Placeholder for VarBindList length

    // VarBind
    snmpPacket.append(0x30); // Sequence
    int varBindLenPos = snmpPacket.size();
    snmpPacket.append(static_cast<char>(0x00)); // Placeholder for VarBind length

    // OID
    encodeOID(snmpPacket, oid);

    // Value (INTEGER)
    if (valueType == 0x02) {
        encodeInteger(snmpPacket, value, isSigned);
    } else {
        qWarning() << "Unsupported value type:" << valueType;
        return;
    }

    // Update lengths
    int varBindLen = snmpPacket.size() - varBindLenPos - 1;
    snmpPacket[varBindLenPos] = static_cast<char>(varBindLen);

    int varBindListLen = snmpPacket.size() - varBindListLenPos - 1;
    snmpPacket[varBindListLenPos] = static_cast<char>(varBindListLen);

    int pduLen = snmpPacket.size() - pduLenPos - 1;
    snmpPacket[pduLenPos] = static_cast<char>(pduLen);

    int totalLen = snmpPacket.size() - totalLenPos - 1;
    snmpPacket[totalLenPos] = static_cast<char>(totalLen);
}

void SnmpConverter::sendSnmpPacket() {
    qint64 bytesWritten = m_udpSocket->writeDatagram(snmpPacket, m_udpAddress, m_udpPort);
    if (bytesWritten == -1) {
        QString err = "UDP send failed: " + m_udpSocket->errorString();
        qWarning() << err;
        emit errorOccurred(err);
    } else {
        qDebug() << "SNMP packet sent:" << snmpPacket.toHex(' ');
        emit snmpPacketSent(snmpPacket);
    }
}
