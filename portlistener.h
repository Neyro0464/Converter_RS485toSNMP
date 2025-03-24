#ifndef PORTLISTENER_H
#define PORTLISTENER_H

#include <QObject>
#include <QSerialPort>

/*
Contains QSerialPort settings:
>QString name - portName: default ""; For windows "COM1", for Unix "dev/ttyUSB0"
>BaudRate - default 9600 / specific 19200
>DataBits - default Data8
>Parity - default NoParity
>StopBits - default OneStop
>FlowControl - default NoFlowControl
*/
struct portSettings {
    QString name{};
    QSerialPort::BaudRate baudRate{QSerialPort::Baud19200};
    QSerialPort::DataBits dataBits{QSerialPort::Data8};
    QSerialPort::Parity parityMode{QSerialPort::NoParity};
    QSerialPort::StopBits stopBits{QSerialPort::OneStop};
    QSerialPort::FlowControl flowControlMode{QSerialPort::NoFlowControl};
};

class PortListener : public QObject {
    Q_OBJECT
public:
    /*
    Constructor sets port parameters at startup
    */
    explicit PortListener(const portSettings &config, QObject *parent = nullptr);
    ~PortListener();

private:
    // Object that provides reading from serial port
    QSerialPort *m_serialPort;
    // Configurate serial port parameters
    void writeSettingsPort(const portSettings &s);

public slots:
    // Slot that openes port in ReadOnly Mode
    void connectPort();
    // Slot: Listener in a separate thread: listens to and transmits the read data for processing
    void readSerialData();

signals:
    // Signal: transmit readed data
    void readedInfo(const QByteArray &result);
    // Signal for error
    void errorOccurred(const QString &err);
};

#endif // PORTLISTENER_H
