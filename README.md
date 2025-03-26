# Converter_RS485toSNMP
## Configurate:  
In main.cpp file configurate next:  
- port name (dev/ttyUSB0 for linux; COM1 for windows)
- serial port settings based on device settings (pay attention to the baudrate)
- ip address and port of the server

## Compile:
> sudo apt update
sudo apt install build-essential qtbase5-dev qtserialport5-dev libqt5network5 qt5-qmake
$qmake  
$make    
./RS485_2  

## Description
This program connects to the serial port and starts listening to this port in a separate thread. The received data is output from the stream and processed. The processing process includes reading the SCI packet, dividing it into bytes, calculating the control byte and verifying it. After that, the Snmpv1 packet is built based on the received data (according to the MIB specification), the OID is compiled and the compiled packet is sent to the designated address.

## Structure
- snmpconventer:  
Contains functions that read SCI, process received datta, process SCI data, build SNMP packet and send them.    
- portlistener:  
Contains functions that configurate serial port reader, listen port and send data to snmpconverter.  
