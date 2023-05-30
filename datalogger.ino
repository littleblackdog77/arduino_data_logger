#include <DHT.h>
#include <OneWire.h>
#include <Wire.h> 
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Dns.h>
#include <ThingSpeak.h>
#include <DallasTemperature.h>
#include <DS3232RTC.h>

// Define Arduino pins and module types
#define PIN_WATCHDOG A1
#define PIN_DHT A2
#define DHTTYPE DHT22

// Variables used in both Setup() and Loop()
const int packet_size = 48;
byte packetBuffer[packet_size];
DHT dht(PIN_DHT,DHTTYPE);
EthernetClient client;
EthernetUDP Udp;

void setup() {
  byte mac[] = { 0x54, 0x10, 0xEC, 0x3C, 0x67, 0x2B };
  unsigned long seventyYears = 2208988800UL;
  unsigned long highWord;
  unsigned long lowWord;
  unsigned long secsSince1900;
  unsigned long epoch;
  char *ntp_pool_server = "pool.ntp.org";
  int localPort = 80;
  IPAddress ip(192, 168, 0, 1);
  IPAddress dns(192, 168, 0, 254);
  IPAddress gateway(192, 168, 0, 254);
  IPAddress subnet(255, 255, 255, 0);
  DNSClient Dns;
  IPAddress timeServer;

  // Initialize ThingSpeak and the DHT module
  ThingSpeak.begin(client);
  dht.begin();
  // Initialize networking
  Ethernet.begin(mac, ip, dns, gateway, subnet);
  Dns.begin(Ethernet.dnsServerIP());
  Udp.begin(localPort);

  // Set watchdog pin to output
  pinMode(PIN_WATCHDOG, OUTPUT);

  // Reset watchdog module
  resetWatchdog();
  
  // Resolve ntp server domain name to IP
  while (!Dns.getHostByName(ntp_pool_server, timeServer))
  {
    delay(5000);
    Dns.getHostByName(ntp_pool_server, timeServer);
  }

  // Send NTP packat, retry until successful
  sendNTPpacket(timeServer);
  delay(5000);
  while (!Udp.parsePacket()) {
    sendNTPpacket(timeServer);
    delay(5000);
  } 

  // Read NTP packet, convert to epoch time, and set RTC module
  Udp.read(packetBuffer, packet_size);
  highWord = word(packetBuffer[40], packetBuffer[41]);
  lowWord = word(packetBuffer[42], packetBuffer[43]);
  secsSince1900 = highWord << 16 | lowWord;
  epoch = secsSince1900 - seventyYears;
  RTC.set(epoch);
}

void loop() {

  // Reset watchdog module
  resetWatchdog();
  
  int server_room_channel = <thingspeak channel key>;
  char *server_room_control_key = "<thingspeak room key>";
  int humiditiy_calibration = 5;
  int selected_element_min = 4;
  int selected_element_max = 5;
  float temperature_readings[10];
  float humidity_readings[10];
  tmElements_t tm;

  // Read RTC time, get data samples, send median values to ThingSpeak
  RTC.read(tm);
  if ((tm.Minute == 00 && tm.Second == 00) || (tm.Minute == 30 && tm.Second == 00))
  {
    for (int t = 0; t < sample_size; t++) {
      temperature_readings[t] = dht.readTemperature();
      humidity_readings[t] = dht.readHumidity() + humiditiy_calibration;
      delay(370);

    }
    // Sorting temperature and humidity arrays, using median values as the DHT sensor can produce abnormal readings
    qsort(temperature_readings, sizeof(temperature_readings) / sizeof(temperature_readings[0]), sizeof(temperature_readings[0]), sort_desc);
    qsort(humidity_readings, sizeof(humidity_readings) / sizeof(humidity_readings[0]), sizeof(humidity_readings[0]), sort_desc);
    ThingSpeak.setField(1, (temperature_readings[selected_element_min] + temperature_readings[selected_element_max]) / 2);
    ThingSpeak.setField(2, (humidity_readings[selected_element_min] + humidity_readings[selected_element_max]) / 2);
    ThingSpeak.writeFields(server_room_channel, server_room_control_key);
  }
}

// Reset watchdog module
void resetWatchdog() {
  digitalWrite(PIN_WATCHDOG, HIGH);
  delay(20);
  digitalWrite(PIN_WATCHDOG, LOW);
}

// Send NTP packet
unsigned long sendNTPpacket(IPAddress& address)
{
  memset(packetBuffer, 0, packet_size); 
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49; 
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, packet_size);
  Udp.endPacket();
}

// Qsort sort function
int sort_desc(const void *cmp1, const void *cmp2)
{
  float sort_desc_cmp1 = *((float *)cmp1);
  float sort_desc_cmp2 = *((float *)cmp2);
  return sort_desc_cmp1 > sort_desc_cmp2 ? -1 : (sort_desc_cmp1 < sort_desc_cmp2 ? 1 : 0);
}
