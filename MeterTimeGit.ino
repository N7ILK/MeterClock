#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

char ssid[] = "SSID";  //  your network SSID (name)
char pass[] = "PASS";       // your network password
char buf[8];
int hour, minute, second, colon = 0, refreshTime = 1, doSecondUpdate=1,GMTOffset;
int timerConstant = 985; //Tweak value with serial in until you find closest 
byte EEPROMvalue; // Holds GMT offset

WiFiUDP packet;
Ticker callGetTime, tickerSeconds;

void setup()
{
  Serial.begin(115200);
  pinMode(4,INPUT_PULLUP);
  pinMode(5,INPUT_PULLUP);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  EEPROM.begin(512);
  if((EEPROMvalue = EEPROM.read(0)) < 24)
    GMTOffset = EEPROMvalue;
  packet.begin(2390);
  getTime();
}

void triggerRefresh()
{
  refreshTime = 1;
}

void triggerSecondUpdate()
{
  doSecondUpdate = 1;
}

void updateTime()
{
  doSecondUpdate = 0;
  if (++second > 59)
  {
    second = 0;
    if (++minute > 59)
    {
      minute = 0;
      if (++hour > 23)
        hour = 0;
    }
    analogWrite(14,minute*17.35); //1024 / 59
    analogWrite(12,hour*44.5); //1024 / 23
    printTime(hour, minute, second);
 }
  //outputTime(hour, minute, second);
  tickerSeconds.attach_ms(timerConstant, triggerSecondUpdate);
}

void loop()
{
     if (Serial.available()) //Allows you to tweak constant to find closest value and use in definition
     {
         Serial.readBytesUntil('\n',buf,8);
         Serial.println(buf);
         timerConstant = atoi(buf);
         refreshTime = 1;
    }     
    if (refreshTime)
      getTime();
    if (doSecondUpdate)
      updateTime();
    if(digitalRead(4) == LOW) // GMT adjustment
    {
      --GMTOffset;
      ++hour;
      EEPROM.write(0,GMTOffset);
      EEPROM.commit();
      delay(500); //Sleazy debounce
    }
    if(digitalRead(5) == LOW) // GMT adjustment
    {
      ++GMTOffset;
      --hour;
      EEPROM.write(0,GMTOffset);
      EEPROM.commit();
      delay(500);
    }
    delay(20); //Arbitrary number that seemed to work
}

void getTime() {
  const int NTP_PACKET_SIZE = 48;
  byte packetBuffer[ NTP_PACKET_SIZE];
  //IPAddress timeServerIP;

  refreshTime = 0;
  IPAddress timeServerIP(1, 1, 1, 1); //This gets NTP from my router
  //WiFi.hostByName("time.nist.gov", timeServerIP); // Use this instead to go out to the real world
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  packet.beginPacket(timeServerIP, 123);
  packet.write(packetBuffer, NTP_PACKET_SIZE);
  packet.endPacket();
  // Wait for reply
  while (packet.parsePacket() < 1)
  {
    Serial.print(".");
    delay(50);
  }
  packet.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
  unsigned long secsSince1900 = highWord << 16 | lowWord;
  const unsigned long seventyYears = 2208988800UL;
  unsigned long epoch = secsSince1900 - seventyYears;
  epoch -= GMTOffset * 3600; //Adjust time zone
  hour = (epoch % 86400L) / 3600;
    Serial.println(second);
  minute = (epoch % 3600) / 60;
  second = epoch % 60;
  printTime(hour, minute, second);
  callGetTime.attach(3600, triggerRefresh);
}

void printTime(int hour, int minute, int second)
{
  if (hour < 10)
    Serial.print("0");
  Serial.print(hour);
  Serial.print(":");
  if (minute < 10)
    Serial.print("0");
  Serial.print(minute);
  Serial.print(":");
  if (second < 10)
    Serial.print("0");
  Serial.println(second);
}
