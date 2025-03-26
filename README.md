# Converter_RS485toSNMP
## Configurate:  
In main.cpp file configurate next:  
- port name (dev/ttyUSB0 for linux; COM1 for windows)
- serial port settings based on device settings (pay attention to the baudrate)
- ip address and port of the server

## Compile:
> script downloading required libraries for Distributor ID: Debian; Release: 12  
$./build_project.sh  
$./RS485_2    
  
## Description
This program connects to the serial port and starts listening to this port in a separate thread. The received data is output from the stream and processed. The processing process includes reading the SCI packet, dividing it into bytes, calculating the control byte and verifying it. After that, the Snmpv1 packet is built based on the received data (according to the MIB specification), the OID is compiled and the compiled packet is sent to the designated address.

## Structure
- snmpconventer:  
Contains functions that read SCI, process received datta, process SCI data, build SNMP packet and send them.    
- portlistener:  
Contains functions that configurate serial port reader, listen port and send data to snmpconverter.  
