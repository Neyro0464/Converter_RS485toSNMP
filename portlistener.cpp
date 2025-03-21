#include "portlistener.h"
#include "qdebug.h"

portListener::portListener(const portSettings config){
    m_serialPort = new QSerialPort;
    portListener::Write_Settings_Port(config);
    qDebug() << "Listener is created";
}

portListener::~portListener(){
    qDebug("By in Thread!");
    //emit finishedPort();//Сигнал о завершении работы
}

void portListener::Write_Settings_Port(const portSettings s){
    this->m_serialPort->setPortName(s.name);
    if(!this->m_serialPort->setBaudRate(s.baudRate))
        throw std::invalid_argument("BaudRate wrong value");
    if(!this->m_serialPort->setDataBits(s.dataBits))
        throw std::invalid_argument("dataBits wrong value");
    if(!this->m_serialPort->setFlowControl(s.flowControlMode))
         throw std::invalid_argument("flowControlMode wrong value");
    if(!this->m_serialPort->setStopBits(s.stopBits))
        throw std::invalid_argument("stopBits wrong value");
    if(!this->m_serialPort->setParity(s.parityMode))
        throw std::invalid_argument("parityMode wrong value");
    qDebug() << "Port configured successfully";
}

void portListener::showPortSettings(){
    qDebug() <<this->m_serialPort->portName();
    emit readedInfo("Data is work");
};

void portListener::ConnectPort(){
    if(m_serialPort->open(QSerialPort::ReadWrite)){
        qDebug()<<"Port is opened";
    }
    else{
        qDebug() << "Port is not opened";
        throw std::runtime_error("Port can not be opened");
    }
}

// void portListener::ListenPort(){
//     qDebug() << "Listening...\n";
//     char buffer[50];
//     for (;;) {
//         memset(buffer, '\0', sizeof(buffer));
//         const qint64 numRead  = this->m_serialPort->read(buffer, 50);
//         if(numRead == -1){
//             throw std::bad_exception();
//         }
//         if(numRead > 0){
//             emit readedInfo(buffer);
//             // this->m_serialPort->write("Hello!!");
//         }
//         if (numRead == 0 && !m_serialPort->waitForReadyRead(-1))
//         {
//             qDebug() << "End Listening!";
//             break;
//         }
//     }
// }

void portListener::ListenPort(){
    qDebug() << "Listening...\n";
    QByteArray buffer;
    for (;;) {
        buffer = this->m_serialPort->read(50);
        if(buffer.size() == -1){
            throw std::bad_exception();
        }
        if(buffer.size()  > 0){
            emit readedInfo(buffer);
            // this->m_serialPort->write("Hello!!");
        }
        if (buffer.size()  == 0 && !m_serialPort->waitForReadyRead(-1))
        {
            qDebug() << "End Listening!";
            break;
        }
    }
}
