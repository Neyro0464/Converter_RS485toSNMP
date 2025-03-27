#include "snmpconverter.h"
#include <QDebug>

SnmpConverter::SnmpConverter(const QHostAddress &udpAddress, quint16 udpPort, int listenAddress, QObject *parent)
    : QObject(parent), m_udpSocket(new QUdpSocket(this)), m_udpAddress(udpAddress), m_udpPort(udpPort), m_listenAddress(listenAddress) {
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
    pack.destSrc = sciData[1]; // high nibble is destination; low nibble is source
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
        if (m_listenAddress != -1 && src != m_listenAddress) {
            return; // Skip if not listening to this address and not "all"
        }

        QString unitPrefix = "2B06010401E2F704"; // Fixed OID prefix: 1.3.6.1.4.1.58039.4
        QString muteSuffix, summaryAlarmSuffix, tempAlarmSuffix, tempSuffix, gainSuffix, powerSuffix,
            statusSuffix, reflectedPowerSuffix, inputVoltageSuffix, operatingIFSuffix, outoflockAlarmSuffix,
            inputVoltageAlarmSuffix, overPowerAlarmSuffix, alarmLog1Suffix, alarmLog2Suffix, alarmLog3Suffix;

        // Defining OID suffixes depending on the unit
        if (src == 0xA) { // PA A
            statusSuffix = "01";          // unitquery.1
            powerSuffix = "03";           // unitquery.3
            reflectedPowerSuffix = "04";  // unitquery.4
            tempSuffix = "05";            // unitquery.5
            inputVoltageSuffix = "06";    // unitquery.6
            gainSuffix = "07";            // unitquery.7
            muteSuffix = "08";            // unitquery.8
            operatingIFSuffix = "09";     // unitquery.9
            summaryAlarmSuffix = "0A";    // unitquery.10
            outoflockAlarmSuffix = "0B";  // unitquery.11
            tempAlarmSuffix = "0C";       // unitquery.12
            inputVoltageAlarmSuffix = "0D"; // unitquery.13
            overPowerAlarmSuffix = "0E";  // unitquery.14
            alarmLog1Suffix = "3E";       // unitquery.62
            alarmLog2Suffix = "3F";       // unitquery.63
            alarmLog3Suffix = "40";       // unitquery.64
        } else if (src == 0xB) { // PA B
            statusSuffix = "02";          // unitquery.2
            powerSuffix = "14";           // unitquery.20
            reflectedPowerSuffix = "15";  // unitquery.21
            tempSuffix = "16";            // unitquery.22
            inputVoltageSuffix = "17";    // unitquery.23
            gainSuffix = "18";            // unitquery.24
            muteSuffix = "19";            // unitquery.25
            operatingIFSuffix = "1A";     // unitquery.26
            summaryAlarmSuffix = "1B";    // unitquery.27
            outoflockAlarmSuffix = "1C";  // unitquery.28
            tempAlarmSuffix = "1D";       // unitquery.29
            inputVoltageAlarmSuffix = "1E"; // unitquery.30
            overPowerAlarmSuffix = "1F";  // unitquery.31
            alarmLog1Suffix = "41";       // unitquery.65
            alarmLog2Suffix = "42";       // unitquery.66
            alarmLog3Suffix = "43";       // unitquery.67
        } else if (src == 0xC) { // PA C
            statusSuffix = "28";          // unitquery.40
            powerSuffix = "29";           // unitquery.41
            reflectedPowerSuffix = "2A";  // unitquery.42
            tempSuffix = "2B";            // unitquery.43
            inputVoltageSuffix = "2C";    // unitquery.44
            gainSuffix = "2D";            // unitquery.45
            muteSuffix = "2E";            // unitquery.46
            operatingIFSuffix = "2F";     // unitquery.47
            summaryAlarmSuffix = "30";    // unitquery.48
            outoflockAlarmSuffix = "31";  // unitquery.49
            tempAlarmSuffix = "32";       // unitquery.50
            inputVoltageAlarmSuffix = "33"; // unitquery.51
            overPowerAlarmSuffix = "34";  // unitquery.52
            alarmLog1Suffix = "4A";       // unitquery.74
            alarmLog2Suffix = "4B";       // unitquery.75
            alarmLog3Suffix = "4C";       // unitquery.76
        } else {
            return; // Skip unsupported sources
        }

        // Processing SCI commands
        switch (pack.cmd) {
        case 0x8: { // All commands with the prefix 0x8
            if (pack.data.size() < 2 || pack.data[0] != 0xFF) {
                return;
            }

            uint8_t subCommand = pack.data[1];
            if (subCommand == 0x09) { // UPD (Update PA Status)
                if (pack.data.size() < 11) {
                    return;
                }

                // OID for all parameters
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
                // Обработка других подкоманд с префиксом 0x8
                switch (subCommand) {
                case 0x00: { // Update SW Version (7.3)
                    if (pack.data.size() < 10) {
                        return;
                    }
                    QString versionBase = QString("%1.%2.%3.%4")
                                              .arg(pack.data[2], 2, 16, QChar('0'))
                                              .arg(pack.data[3], 2, 16, QChar('0'))
                                              .arg(pack.data[4], 2, 16, QChar('0'))
                                              .arg(pack.data[5], 2, 16, QChar('0'));
                    QString versionConfig = QString("%1.%2")
                                                .arg(pack.data[6], 2, 16, QChar('0'))
                                                .arg(pack.data[7], 2, 16, QChar('0'));
                    QString versionRevision = QString("%1%2")
                                                  .arg(static_cast<char>(pack.data[8]))
                                                  .arg(static_cast<char>(pack.data[9]));
                    QString fullVersion = versionBase + "-" + versionConfig + "-" + versionRevision;

                    // Отправляем в product.version (1.3.6.1.4.1.58039.1.2)
                    QByteArray versionOid = QByteArray::fromHex("2B06010401E2F70102");
                    snmpPacket.clear();
                    snmpPacket.append(0x30); // Sequence
                    int totalLenPos = snmpPacket.size();
                    snmpPacket.append(static_cast<char>(0x00)); // Placeholder for total length

                    // Version (SNMPv1)
                    snmpPacket.append(0x02); // INTEGER
                    snmpPacket.append(0x01); // Length
                    snmpPacket.append(static_cast<char>(0x00)); // Version 1

                    // Community
                    snmpPacket.append(0x04);
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
                    requestId++;

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
                    encodeOID(snmpPacket, versionOid);

                    // Value
                    snmpPacket.append(0x04);
                    encodeLength(snmpPacket, fullVersion.length());
                    snmpPacket.append(fullVersion.toLatin1());

                    // Update lengths
                    int varBindLen = snmpPacket.size() - varBindLenPos - 1;
                    snmpPacket[varBindLenPos] = static_cast<char>(varBindLen);

                    int varBindListLen = snmpPacket.size() - varBindListLenPos - 1;
                    snmpPacket[varBindListLenPos] = static_cast<char>(varBindListLen);

                    int pduLen = snmpPacket.size() - pduLenPos - 1;
                    snmpPacket[pduLenPos] = static_cast<char>(pduLen);

                    int totalLen = snmpPacket.size() - totalLenPos - 1;
                    snmpPacket[totalLenPos] = static_cast<char>(totalLen);

                    sendSnmpPacket();

                    QString firmwareSuffix;
                    if (src == 0xA) firmwareSuffix = "04"; // info.4 (paAVer)
                    else if (src == 0xB) firmwareSuffix = "05"; // info.5 (paBVer)
                    else if (src == 0xC) firmwareSuffix = "06"; // info.6 (paCVer)
                    if (!firmwareSuffix.isEmpty()) {
                        QByteArray firmwareOid = QByteArray::fromHex(("2B06010401E2F702" + firmwareSuffix).toLatin1());
                        snmpPacket.clear();
                        snmpPacket.append(0x30); // Sequence
                        totalLenPos = snmpPacket.size();
                        snmpPacket.append(static_cast<char>(0x00)); // Placeholder for total length

                        // Version (SNMPv1)
                        snmpPacket.append(0x02); // INTEGER
                        snmpPacket.append(0x01); // Length
                        snmpPacket.append(static_cast<char>(0x00)); // Version 1

                        // Community
                        snmpPacket.append(0x04); // OCTET STRING
                        snmpPacket.append(static_cast<char>(community.length()));
                        snmpPacket.append(community.toLatin1());

                        // PDU (GetResponse)
                        snmpPacket.append(0xA2); // GetResponse-PDU
                        pduLenPos = snmpPacket.size();
                        snmpPacket.append(static_cast<char>(0x00)); // Placeholder for PDU length

                        // Request ID
                        snmpPacket.append(0x02); // INTEGER
                        snmpPacket.append(0x04); // Length 4
                        snmpPacket.append((requestId >> 24) & 0xFF);
                        snmpPacket.append((requestId >> 16) & 0xFF);
                        snmpPacket.append((requestId >> 8) & 0xFF);
                        snmpPacket.append(requestId & 0xFF);
                        requestId++;

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
                        varBindListLenPos = snmpPacket.size();
                        snmpPacket.append(static_cast<char>(0x00)); // Placeholder for VarBindList length

                        // VarBind
                        snmpPacket.append(0x30); // Sequence
                        varBindLenPos = snmpPacket.size();
                        snmpPacket.append(static_cast<char>(0x00)); // Placeholder for VarBind length

                        // OID
                        encodeOID(snmpPacket, firmwareOid);

                        // Value (OCTET STRING)
                        snmpPacket.append(0x04); // OCTET STRING tag
                        encodeLength(snmpPacket, fullVersion.length());
                        snmpPacket.append(fullVersion.toLatin1());

                        // Update lengths
                        varBindLen = snmpPacket.size() - varBindLenPos - 1;
                        snmpPacket[varBindLenPos] = static_cast<char>(varBindLen);

                        varBindListLen = snmpPacket.size() - varBindListLenPos - 1;
                        snmpPacket[varBindListLenPos] = static_cast<char>(varBindListLen);

                        pduLen = snmpPacket.size() - pduLenPos - 1;
                        snmpPacket[pduLenPos] = static_cast<char>(pduLen);

                        totalLen = snmpPacket.size() - totalLenPos - 1;
                        snmpPacket[totalLenPos] = static_cast<char>(totalLen);

                        sendSnmpPacket();
                    }
                    break;
                }
                case 0x03: { // Update Frequency Band (7.3)
                    if (pack.data.size() < 3) {
                        return;
                    }
                    int32_t freqBand = pack.data[2];
                    QByteArray operatingIFOid = QByteArray::fromHex((unitPrefix + operatingIFSuffix).toLatin1());
                    buildSnmpPacket(operatingIFOid, freqBand == 0 ? 13050 : 12800, 0x02, false); // LO freq
                    sendSnmpPacket();
                    break;
                }
                case 0x04: { // Update Frequency Setting (7.3)
                    // Nothing in MIB so skip
                    break;
                }
                case 0x05: { // Update Alarm Log History (7.1)
                    if (pack.data.size() < 5) {
                        return;
                    }
                    uint8_t eventId = pack.data[1];
                    if (eventId >= 0x11 && eventId <= 0x15) {
                        uint8_t unit = pack.data[4];
                        QString alarmLog;
                        if (unit == 0x01) { // PA
                            alarmLog = QString("PA %1: %2%3")
                                           .arg(src == 0xA ? "A" : src == 0xB ? "B" : "C")
                                           .arg(pack.data[2], 2, 16, QChar('0'))
                                           .arg(pack.data[3], 2, 16, QChar('0'));
                        } else if (unit == 0x04) { // Switches
                            alarmLog = QString("Switches: %1%2")
                                           .arg(pack.data[2], 2, 16, QChar('0'))
                                           .arg(pack.data[3], 2, 16, QChar('0'));
                        } else {
                            break;
                        }

                        // Determining which alarm log to write to (1, 2, or 3)
                        QString logSuffix;
                        int logIndex = eventId - 0x11 + 1; // 0x11 -> 1, 0x12 -> 2, 0x13 -> 3
                        if (logIndex >= 1 && logIndex <= 3) {
                            if (unit == 0x01) { // PA
                                logSuffix = (src == 0xA) ? (logIndex == 1 ? "3E" : logIndex == 2 ? "3F" : "40") :
                                                (src == 0xB) ? (logIndex == 1 ? "41" : logIndex == 2 ? "42" : "43") :
                                                (src == 0xC) ? (logIndex == 1 ? "4A" : logIndex == 2 ? "4B" : "4C") : "";
                            } else if (unit == 0x04) { // Switches
                                logSuffix = (logIndex == 1 ? "44" : logIndex == 2 ? "45" : "46"); // switchAlarmLog1-3
                            }
                        }

                        if (!logSuffix.isEmpty()) {
                            QByteArray alarmLogOid = QByteArray::fromHex((unitPrefix + logSuffix).toLatin1());
                            snmpPacket.clear();
                            snmpPacket.append(0x30); // Sequence
                            int totalLenPos = snmpPacket.size();
                            snmpPacket.append(static_cast<char>(0x00)); // Placeholder for total length

                            // Version (SNMPv1)
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
                            requestId++;

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
                            encodeOID(snmpPacket, alarmLogOid);

                            // Value (OCTET STRING)
                            snmpPacket.append(0x04); // OCTET STRING tag
                            encodeLength(snmpPacket, alarmLog.length());
                            snmpPacket.append(alarmLog.toLatin1());

                            // Update lengths
                            int varBindLen = snmpPacket.size() - varBindLenPos - 1;
                            snmpPacket[varBindLenPos] = static_cast<char>(varBindLen);

                            int varBindListLen = snmpPacket.size() - varBindListLenPos - 1;
                            snmpPacket[varBindListLenPos] = static_cast<char>(varBindListLen);

                            int pduLen = snmpPacket.size() - pduLenPos - 1;
                            snmpPacket[pduLenPos] = static_cast<char>(pduLen);

                            int totalLen = snmpPacket.size() - totalLenPos - 1;
                            snmpPacket[totalLenPos] = static_cast<char>(totalLen);

                            sendSnmpPacket();
                        }
                    }
                    break;
                }
                case 0x06: { // Update Redundant System Status (9.1)
                    if (pack.data.size() < 5) {
                        return;
                    }
                    // Format: FF 08 00 WW 00 YY
                    uint8_t systemStatus = pack.data[3];
                    uint8_t switchStatus = pack.data[4];

                    // System Type (info.unitType, 1.3.6.1.4.1.58039.2.1)
                    int32_t systemType = (systemStatus & 0x01) ? 3 : 0; // Bit 0: 0=1:1, 1=1:2
                    if (systemStatus & 0x80) systemType = 1; // Bit 7: 1=standalone
                    QByteArray unitTypeOid = QByteArray::fromHex("2B06010401E2F70201");
                    buildSnmpPacket(unitTypeOid, systemType, 0x02, false);
                    sendSnmpPacket();

                    // Operation Mode (info.opMode, 1.3.6.1.4.1.58039.2.2)
                    int32_t opMode = (systemStatus & 0x02) ? 1 : 0; // Bit 1: 0=auto, 1=manual
                    QByteArray opModeOid = QByteArray::fromHex("2B06010401E2F70202");
                    buildSnmpPacket(opModeOid, opMode, 0x02, false);
                    sendSnmpPacket();

                    // Switch Position (config.uplinkChain, 1.3.6.1.4.1.58039.3.6)
                    int32_t switchPos = (switchStatus == 0x01) ? 0 : (switchStatus == 0x02) ? 1 : 2; // 01=side A, 02=side B, else=standalone
                    QByteArray uplinkChainOid = QByteArray::fromHex("2B06010401E2F70306");
                    buildSnmpPacket(uplinkChainOid, switchPos, 0x02, false);
                    sendSnmpPacket();

                    // PA Status (unitquery.pAAStatus/pABStatus/pACStatus)
                    int32_t paStatus = (switchStatus == 0x01) ? 0 : 1; // 01=side A (active), else=standby
                    QByteArray statusOid = QByteArray::fromHex((unitPrefix + statusSuffix).toLatin1());
                    buildSnmpPacket(statusOid, paStatus, 0x02, false);
                    sendSnmpPacket();
                    break;
                }
                case 0x0C: { // Update System and Switches Alarm Status (9.1)
                    if (pack.data.size() < 5) {
                        return;
                    }
                    // Format: FF 0C VV WW 00 YY
                    uint8_t systemAlarm1 = pack.data[2]; // VV
                    uint8_t systemAlarm2 = pack.data[3]; // WW
                    uint8_t switchAlarm = pack.data[4];  // YY

                    // Uplink Switch Alarms (unitquery.upSwitchAlarm, 1.3.6.1.4.1.58039.4.60)
                    int32_t switch1Alarm = 0;
                    if (switchAlarm & 0x01) switch1Alarm = 2; // out of position
                    else if (switchAlarm & 0x04) switch1Alarm = 3; // unable to move
                    else if (systemAlarm1 & 0x01) switch1Alarm = 1; // communication alarm
                    QByteArray upSwitchAlarmOid = QByteArray::fromHex("2B06010401E2F7043C");
                    buildSnmpPacket(upSwitchAlarmOid, switch1Alarm, 0x02, false);
                    sendSnmpPacket();

                    // Uplink Switch 2 Alarms (unitquery.upSwitch2Alarm, 1.3.6.1.4.1.58039.4.61)
                    int32_t switch2Alarm = 0;
                    if (switchAlarm & 0x08) switch2Alarm = 2; // out of position
                    else if (switchAlarm & 0x20) switch2Alarm = 3; // unable to move
                    else if (systemAlarm1 & 0x02) switch2Alarm = 1; // communication alarm
                    QByteArray upSwitch2AlarmOid = QByteArray::fromHex("2B06010401E2F7043D");
                    buildSnmpPacket(upSwitch2AlarmOid, switch2Alarm, 0x02, false);
                    sendSnmpPacket();

                    // PA Summary Alarms
                    if (src == 0xA) {
                        int32_t summaryAlarm = (systemAlarm2 & 0x01) ? 1 : 0; // Unit A summary alarm
                        QByteArray summaryAlarmOid = QByteArray::fromHex((unitPrefix + summaryAlarmSuffix).toLatin1());
                        buildSnmpPacket(summaryAlarmOid, summaryAlarm, 0x02, false);
                        sendSnmpPacket();
                    } else if (src == 0xB) {
                        int32_t summaryAlarm = (systemAlarm2 & 0x02) ? 1 : 0; // Unit B summary alarm
                        QByteArray summaryAlarmOid = QByteArray::fromHex((unitPrefix + summaryAlarmSuffix).toLatin1());
                        buildSnmpPacket(summaryAlarmOid, summaryAlarm, 0x02, false);
                        sendSnmpPacket();
                    } else if (src == 0xC) {
                        int32_t summaryAlarm = (systemAlarm2 & 0x04) ? 1 : 0; // Unit C summary alarm
                        QByteArray summaryAlarmOid = QByteArray::fromHex((unitPrefix + summaryAlarmSuffix).toLatin1());
                        buildSnmpPacket(summaryAlarmOid, summaryAlarm, 0x02, false);
                        sendSnmpPacket();
                    }
                    break;
                }
                case 0x17: { // Update LO Frequency and Tx Freq Band (7.3) или Update IF Frequency (7.3)
                    if (pack.data.size() < 4) {
                        return;
                    }
                    if (pack.data[2] == 0x17) { // LO Frequency and Tx Freq Band
                        // Format: FF 17 L1 L2 M1 M2 M3 M4
                        uint16_t loFreq = (pack.data[3] << 8) | pack.data[4];
                        // Saving only LO Frequency in operatingIF
                        QByteArray operatingIFOid = QByteArray::fromHex((unitPrefix + operatingIFSuffix).toLatin1());
                        buildSnmpPacket(operatingIFOid, loFreq, 0x02, false);
                        sendSnmpPacket();
                    } else if (pack.data[2] == 0xFF && pack.data[3] == 0x17) { // IF Frequency
                        // Format: FF 17 FF YY YY
                        uint16_t ifFreq = (pack.data[4] << 8) | pack.data[5];
                        QByteArray operatingIFOid = QByteArray::fromHex((unitPrefix + operatingIFSuffix).toLatin1());
                        buildSnmpPacket(operatingIFOid, ifFreq, 0x02, false);
                        sendSnmpPacket();
                    }
                    break;
                }
                case 0x18: { // Update Output Frequency (7.3)
                    if (pack.data.size() < 4) {
                        return;
                    }
                    // Format: FF 18 YY YY
                    uint16_t txFreq = (pack.data[2] << 8) | pack.data[3];
                    QByteArray operatingIFOid = QByteArray::fromHex((unitPrefix + operatingIFSuffix).toLatin1());
                    buildSnmpPacket(operatingIFOid, txFreq, 0x02, false);
                    sendSnmpPacket();
                    break;
                }
                case 0x19: { // Update Input DC Voltage Value (7.3)
                    if (pack.data.size() < 4) {
                        return;
                    }
                    // Format: FF 19 VV VV
                    uint16_t voltage = (pack.data[2] << 8) | pack.data[3];
                    QByteArray inputVoltageOid = QByteArray::fromHex((unitPrefix + inputVoltageSuffix).toLatin1());
                    buildSnmpPacket(inputVoltageOid, voltage, 0x02, false);
                    sendSnmpPacket();
                    break;
                }
                case 0x21: { // Update Host Name (8.1)
                    if (pack.data.size() < 13) {
                        return;
                    }
                    // Format: FF 21 Y1 Y2 Y3 Y4 Y5 Y6 Y7 Y8 Y9 Y10 Y11
                    QString hostName;
                    for (int i = 2; i <= 12; ++i) {
                        hostName += static_cast<char>(pack.data[i]);
                    }
                    // We are sending to product.name (1.3.6.1.4.1.58039.1.1)
                    QByteArray nameOid = QByteArray::fromHex("2B06010401E2F70101");
                    snmpPacket.clear();
                    snmpPacket.append(0x30); // Sequence
                    int totalLenPos = snmpPacket.size();
                    snmpPacket.append(static_cast<char>(0x00)); // Placeholder for total length

                    // Version (SNMPv1)
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
                    requestId++;

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
                    encodeOID(snmpPacket, nameOid);

                    // Value (OCTET STRING)
                    snmpPacket.append(0x04); // OCTET STRING tag
                    encodeLength(snmpPacket, hostName.length());
                    snmpPacket.append(hostName.toLatin1());

                    // Update lengths
                    int varBindLen = snmpPacket.size() - varBindLenPos - 1;
                    snmpPacket[varBindLenPos] = static_cast<char>(varBindLen);

                    int varBindListLen = snmpPacket.size() - varBindListLenPos - 1;
                    snmpPacket[varBindListLenPos] = static_cast<char>(varBindListLen);

                    int pduLen = snmpPacket.size() - pduLenPos - 1;
                    snmpPacket[pduLenPos] = static_cast<char>(pduLen);

                    int totalLen = snmpPacket.size() - totalLenPos - 1;
                    snmpPacket[totalLenPos] = static_cast<char>(totalLen);

                    sendSnmpPacket();
                    break;
                }
                case 0x20: // Update MAC Address (8.1)
                case 0x31: // Update DHCP Configuration (8.1)
                    // Nothing in MIB so skip
                    break;
                default:
                    break; /// Nothing in MIB so skip
                }
            }
            break;
        }
        case 0xE: { // ACK (Acknowledge)
            // Nothing in MIB so skip
            break;
        }
        case 0xF: { // NACK (Not Acknowledge)
            // Nothing in MIB so skip
            break;
        }
        default:
            break; // Nothing in MIB so skip
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
            // Signed value
            bool isNegative = value < 0;
            uint32_t absValue = isNegative ? -value : value;
            while (absValue > 0) {
                intBytes.prepend(static_cast<char>(absValue & 0xFF));
                absValue >>= 8;
            }
            if (isNegative && (intBytes[0] & 0x80)) {
                intBytes.prepend(0xFF); // Добавляем ведущий 0xFF для отрицательного числа
            } else if (!isNegative && (intBytes[0] & 0x80)) {
                intBytes.prepend(static_cast<char>(0x00)); // Добавляем ведущий 0x00 для положительного числа
            }
        } else {
            // Unsigned value
            uint32_t absValue = static_cast<uint32_t>(value);
            while (absValue > 0) {
                intBytes.prepend(static_cast<char>(absValue & 0xFF));
                absValue >>= 8;
            }
            if (intBytes[0] & 0x80) {
                intBytes.prepend(static_cast<char>(0x00)); // Добавляем ведущий 0x00, чтобы избежать интерпретации как отрицательного
            }
        }
    }

    encodeLength(buffer, intBytes.size());
    buffer.append(intBytes);
}

void SnmpConverter::buildSnmpPacket(const QByteArray &oid, int32_t value, int valueType, bool isSigned) {
    snmpPacket.clear();
    snmpPacket.append(0x30); // Sequence
    int totalLenPos = snmpPacket.size();
    snmpPacket.append(static_cast<char>(0x00)); // Placeholder for total length

    // Version (SNMPv1)
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
