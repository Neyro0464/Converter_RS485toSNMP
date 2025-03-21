#include <QCoreApplication>
//#include "portlistener.h"
#include <QDebug>
#include <QThread>
#include "portlistener.h"
#include "tools.h"


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    // tester A, B;
    // A.connect(&A, &tester::testSignal, &B, &tester::testSlot);
    // A.testSignal(10);

    portSettings port;
    port.name = "COM1"; // Имя порта "COM1" для Windows или "/dev/ttyUSB0" для Linux
    port.baudRate = QSerialPort::Baud9600;
    port.dataBits = QSerialPort::Data8;
    port.parityMode = QSerialPort::NoParity;
    port.stopBits = QSerialPort::OneStop;
    port.flowControlMode = QSerialPort::NoFlowControl;


    portListener* m_port = new portListener(port);
    QThread workerThread;
    m_port->ConnectPort();
    m_port->moveToThread(&workerThread);
    m_port->m_serialPort->moveToThread(&workerThread);
    m_port->connect(&workerThread, &QThread::started, m_port, &portListener::ListenPort);
    m_port->connect(m_port, &portListener::finishedPort, &workerThread, &QThread::quit);
    m_port->connect(m_port, &portListener::finishedPort, m_port, &QObject::deleteLater);
    m_port->connect(&workerThread, &QThread::finished, m_port, &QObject::deleteLater); // Thread coonection signal
    m_port->connect(m_port, &portListener::readedInfo, &handleResult);



    workerThread.start();


    //workerThread.quit();
    workerThread.wait();
    return a.exec();
}
