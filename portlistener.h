#ifndef PORTLISTENER_H
#define PORTLISTENER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QThread>

struct portSettings{
    QString name{};
    QSerialPort::BaudRate baudRate{QSerialPort::Baud9600};
    QSerialPort::DataBits dataBits{QSerialPort::Data8};
    QSerialPort::Parity parityMode{QSerialPort::NoParity};
    QSerialPort::StopBits stopBits{QSerialPort::OneStop};
    QSerialPort::FlowControl flowControlMode{QSerialPort::NoFlowControl};
};

class portListener : public QObject
{
    Q_OBJECT
public:
    explicit portListener(const portSettings);
    virtual ~portListener();

    QSerialPort* m_serialPort;

private:

public slots:
    void ListenPort(); // Main body
    //void DisconnectPort();
    void ConnectPort();//Done
    void Write_Settings_Port(const portSettings s); //Done
    //void WriteToPort(const QByteArray data);
    void showPortSettings();//??? need it ???

private slots:
    //void handleError(QSerialPort::SerialPortError error);//Слот обработки ошибок
    //void ReadInPort(); //Слот чтения из порта по ReadyRead

signals:
    void readedInfo(const QByteArray &result);
    void statusInfo(const QString &info);
    void finishedPort();
    void _error(QString err);
};

#endif // PORTLISTENER_H
