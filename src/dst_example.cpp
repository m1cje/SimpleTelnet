/*
  https://werner.rothschopf.net/202011_arduino_esp8266_ntp_en.htm

  NTP TZ DST - bare minimum
  NetWork Time Protocol - Time Zone - Daylight Saving Time

  Our target for this MINI sketch is:
  - get the SNTP request running
  - set the timezone
  - (implicit) respect daylight saving time
  - how to "read" time to be printed to Serial.Monitor

  This example is a stripped down version of the NTP-TZ-DST (v2)
  And works for ESP8266 core 2.7.4 and 3.0.2

  by noiasca
  2020-09-22
*/

#ifndef STASSID
#define STASSID "your-ssid"    // set your SSID
#define STAPSK "your-password" // set your wifi password
#endif

/* Configuration of NTP */
#define MY_NTP_SERVER "at.pool.ntp.org"
#define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03"

/* Necessary Includes */
#include <ESP8266WiFi.h> // we need wifi to get internet access
#include <time.h>        // time() ctime()

/* Globals */
time_t now; // this is the epoch
tm tm;      // the structure tm holds time information in a more convient way

void showTime()
{
  time(&now);             // read the current time
  localtime_r(&now, &tm); // update the structure tm with the current time
  Serial.print("year:");
  Serial.print(tm.tm_year + 1900); // years since 1900
  Serial.print("\tmonth:");
  Serial.print(tm.tm_mon + 1); // January = 0 (!)
  Serial.print("\tday:");
  Serial.print(tm.tm_mday); // day of month
  Serial.print("\thour:");
  Serial.print(tm.tm_hour); // hours since midnight  0-23
  Serial.print("\tmin:");
  Serial.print(tm.tm_min); // minutes after the hour  0-59
  Serial.print("\tsec:");
  Serial.print(tm.tm_sec); // seconds after the minute  0-61*
  Serial.print("\twday");
  Serial.print(tm.tm_wday); // days since Sunday 0-6
  if (tm.tm_isdst == 1)     // Daylight Saving Time flag
    Serial.print("\tDST");
  else
    Serial.print("\tstandard");
  Serial.println();
}

void tsetup()
{
  Serial.begin(115200);
  Serial.println("\nNTP TZ DST - bare minimum");

  configTime(MY_TZ, MY_NTP_SERVER); // --> Here is the IMPORTANT ONE LINER needed in your sketch!

  // start network
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  // by default, the NTP will be started after 60 secs
}

void tloop()
{
  showTime();
  delay(1000); // dirty delay
}