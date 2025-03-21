#ifndef TOOLS_H
#define TOOLS_H
#include <QQueue>
#include <QDebug>
const uint8_t STX = 0x7E; // start byte
const uint8_t ETX = 0x7F; // end byte


struct SCIPacket{
    uint8_t DestSrc_Address = 0x00;
    uint8_t CMD = 0x00;
    uint8_t Len = 0x00;
    QVector<uint8_t> data{};
    uint8_t crc = 0x00;
};

bool CheckCMC (const SCIPacket pack){
    uint8_t CRC_check = pack.DestSrc_Address ^ (pack.CMD | pack.Len);
    if(pack.Len != 0)
    {
        for(uint8_t a : pack.data)
        {
            CRC_check = CRC_check ^ a;
        }
    }
    if(static_cast<uint8_t>(~CRC_check) == pack.crc)
    {
        return true;
    }
    return false;
}

void showPacket(SCIPacket pack){
    qDebug() << "Destionation_Source byte: " << pack.DestSrc_Address << "\nCMD: " << pack.CMD << "\nLength: " << pack.Len << "\nData: " ;
    for (auto t : std::as_const(pack.data)){
        qDebug() << t << " ";
    }
    qDebug() << "CRC: "<< pack.crc << "\n";
}

void readSCI(QByteArray data){
    SCIPacket pack{};
    if (static_cast<uint8_t>(data[0]) == STX){
        pack.DestSrc_Address = static_cast<uint8_t>(data[1]);

        pack.CMD = static_cast<uint8_t>(data[2] & 0xf0);
        pack.Len = static_cast<uint8_t>(data[2] & 0x0f);
        if((pack.CMD | pack.Len) == static_cast<uint8_t>(data[2]))
            qDebug() << "TRUE" << "\n";
        std::size_t i = 3;
        while(i - 3 < static_cast<std::size_t>(pack.Len)){
            pack.data.append(static_cast<uint8_t>(data[i]));
            i++;
        }
        pack.crc = static_cast<uint8_t>(data[i]);
        if(!CheckCMC(pack)){
            throw std::invalid_argument("CRC is not correct");
        }
        if(static_cast<uint8_t>(data[i+1]) == ETX){
            qDebug() << "Successfully readed!";
        }
    }
    showPacket(pack);

}

void handleResult(QByteArray data){
    QQueue<QByteArray> buffer;
    buffer.enqueue(data);
    qDebug() << data << "\n";
    readSCI(buffer.dequeue());
}


#endif // TOOLS_H
