# arduino_data_logger
Arduino -> ThingSpeak Data Logger

I'm learning C++. Forgive my bad coding :)

arduino_data_loger is a datalogger based on an Arduino Uno with data sent to ThingSpeak for reporting / analysis.

# Hardware required
- Arduino Uno or larger (it's very tight on an Uno, it wouldn't run on a smaller board) with ethernet port
- DHT22 temperature/humidity module
- DS3232 compatible real time clock module
- Watchdog timer module

# Libraries required for this project;
- DHT
- Ethernet
- EthernetUdp
- Dns
- ThingSpeak
- DS3232RTC

# Output
![Temperature Graph](https://i.ibb.co/hgPqfxM/temp.png)
![Humidity Graph](https://i.ibb.co/yPzLwQQ/humid.png)
![Temperature Meter](https://i.ibb.co/zx2kvx0/temp-meter.png)
![Humidity Meter](https://i.ibb.co/D8ZYJ3p/humid-meter.png)
