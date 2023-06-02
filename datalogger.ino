#include <DHT.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Dns.h>
#include <ThingSpeak.h>
#include <DS3232RTC.h>

// Define Arduino pins and module types
#define PIN_WATCHDOG A1
#define PIN_DHT A2
#define DHTTYPE DHT22

byte packetBuffer[48];
EthernetUDP Udp;

void setup() {
  
  Serial.begin(9600);
  
  const PROGMEM byte mac[] = { 0x54, 0x10, 0xEC, 0x3C, 0x67, 0x2B };
  const PROGMEM unsigned long seventyYears = 2208988800UL;
  unsigned long highWord;
  unsigned long lowWord;
  unsigned long secsSince1900;
  unsigned long epoch;
  const PROGMEM char *ntp_pool_server = "0.au.pool.ntp.org";
  IPAddress ip(192, 168, 0, 1);
  IPAddress dns(192, 168, 0, 254);
  IPAddress gateway(192, 168, 0, 254);
  IPAddress subnet(255, 255, 255, 0);
  DNSClient Dns;
  IPAddress timeServer;

  // Initialize networking
  Ethernet.begin(mac, ip, dns, gateway, subnet);
  Dns.begin(Ethernet.dnsServerIP());
  Udp.begin(80);

  // Set watchdog pin to output
  pinMode(PIN_WATCHDOG, OUTPUT);

  // Reset watchdog module
  resetWatchdog();
  
  // Resolve ntp server domain name to IP
  while (!Dns.getHostByName(ntp_pool_server, timeServer))
  {
    delay(1000);
    Dns.getHostByName(ntp_pool_server, timeServer);
  }
  Serial.print(F("NTP server: "));
  Serial.println(timeServer);

  // Send NTP packat, retry until successful
  while (!Udp.parsePacket()) {
    delay(1000);
    sendNTPpacket(timeServer);
  } 

  // Read NTP packet, convert to epoch time, and set RTC module
  Udp.read(packetBuffer, sizeof(packetBuffer));
  highWord = word(packetBuffer[40], packetBuffer[41]);
  lowWord = word(packetBuffer[42], packetBuffer[43]);
  secsSince1900 = highWord << 16 | lowWord;
  epoch = secsSince1900 - seventyYears;

  Serial.print(F("Epoch: "));
  Serial.println(epoch);

  RTC.set(epoch);
}

void loop() {

  // Reset watchdog module
  resetWatchdog();

  const PROGMEM int max_samples = 10;
  const unsigned long server_room_channel = <thingspeak channel key>;
  const char server_room_control_key = "<thingspeak room key>";
  const PROGMEM float humiditiy_calibration = 5;
  float temperature_readings[max_samples];
  float humidity_readings[max_samples];
  float temperature_sample_median;
  float humidity_sample_median;
  tmElements_t tm;
  float dht_temperature;
  float dht_humidity;
  DHT dht(PIN_DHT,DHTTYPE);
  EthernetClient client;

  ThingSpeak.begin(client);
  dht.begin();
  
  // Read RTC time, get data samples, send median values to ThingSpeak
  RTC.read(tm);

  // Run every fifteen minutes, using GMT time not local time
  if ((tm.Minute == 00 || tm.Minute == 15 || tm.Minute == 30 || tm.Minute == 45) && tm.Second == 00)
  {
    Serial.print(F("Time (GMT): "));
    if (tm.Hour == 0)
    {
      Serial.print(F("00"));
    }
    else
    {
      if ((tm.Hour >= 1) && (tm.Hour <=9))
      {
        Serial.print(F("0"));
        Serial.print(tm.Hour);
      }
      else
      {
        Serial.print(tm.Hour);
      }  
    }
    Serial.print(F(":"));
    if (tm.Minute == 0)
    {
      Serial.print(F("00"));
    }
    else
    {
      if ((tm.Minute >= 1) && (tm.Minute <=9))
      {
        Serial.print(F("0"));
        Serial.print(tm.Minute);
      }
      else
      {
        Serial.print(tm.Minute);
      }  
    }
    Serial.print(F(":"));
    if (tm.Second == 0)
    {
      Serial.println(F("00"));
    }
    else
    {
      if ((tm.Second >= 1) && (tm.Second <=9))
      {
        Serial.print(F("0"));
        Serial.print(tm.Second);
      }
      else
      {
        Serial.print(tm.Second);
      }  
    }

    // Get samples up to max_samples
    for (int sample = 0; sample < max_samples; sample++) {
      dht_temperature = dht.readTemperature();
      dht_humidity = dht.readHumidity();
      if (((dht_temperature >= -40.00) && (dht_temperature <= 80.00)) && ((dht_humidity >= 0.00) && (dht_humidity <= 100.00)))
      {
        temperature_readings[sample] = dht_temperature;
        humidity_readings[sample] = dht_humidity;
      }
      else {
        Serial.println("Sensor returned invalid value");
        temperature_readings[sample] = 255;
        humidity_readings[sample] = 255;
      }
      delay(2000);
    }

    // Sorting temperature and humidity arrays, using median values as the DHT sensor can produce a skewed dataset
    qsort(temperature_readings, sizeof(temperature_readings) / sizeof(temperature_readings[0]), sizeof(temperature_readings[0]), sort_desc);
    qsort(humidity_readings, sizeof(humidity_readings) / sizeof(humidity_readings[0]), sizeof(humidity_readings[0]), sort_desc);

     // If max_samples is even
    if (max_samples % 2 == 0)
    {
        temperature_sample_median = (temperature_readings[max_samples / 2 - 1] + temperature_readings[max_samples / 2]) / 2;
        humidity_sample_median = (humidity_readings[max_samples / 2 - 1] + humidity_readings[max_samples / 2]) / 2;
    }
    // If max_samples is odd
    else 
    {
        temperature_sample_median = temperature_readings[max_samples / 2];
        humidity_sample_median = humidity_readings[max_samples / 2];
    }

    Serial.print(F("Median temperature: "));
    Serial.println(temperature_sample_median);
    Serial.print(F("Median humidity: "));
    Serial.println(humidity_sample_median);

    // Send values to ThingSpeak
    if (((temperature_sample_median >= -40.00) && (temperature_sample_median <= 80.00)) && ((humidity_sample_median >= 0.00) && (humidity_sample_median <= 100.00))) 
    {
      // ThingSpeak return codes
      // 200, OK / Success
      // 404, Incorrect API key(or invalid ThingSpeak server address)
      // -101, Value is out of range or string is too long(> 255 characters)
      // -201, Invalid field number specified
      // -210, setField() was not called before writeFields()
      // -301, Failed to connect to ThingSpeak
      // -302, Unexpected failure during write to ThingSpeak
      // -303, Unable to parse response
      // -304, Timeout waiting for server to respond
      // -401, Point was not inserted(most probable cause is the rate limit of once every 15 seconds)
      // 0, Other error

      Serial.print(F("setField 1: "));
      Serial.println(ThingSpeak.setField(1, temperature_sample_median));
      Serial.print(F("setField 2: "));
      Serial.println(ThingSpeak.setField(2, humidity_sample_median));
      Serial.print(F("writeFields: "));
      Serial.println(ThingSpeak.writeFields(server_room_channel, server_room_control_key));
    }
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
  memset(packetBuffer, 0, sizeof(packetBuffer)); 
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49; 
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  Udp.beginPacket(address, 123);
  Udp.write(packetBuffer, sizeof(packetBuffer));
  Udp.endPacket();
}

// Qsort sort function
int sort_desc(const void *cmp1, const void *cmp2)
{
  float sort_desc_cmp1 = *((float *)cmp1);
  float sort_desc_cmp2 = *((float *)cmp2);
  return sort_desc_cmp1 > sort_desc_cmp2 ? -1 : (sort_desc_cmp1 < sort_desc_cmp2 ? 1 : 0);
}
