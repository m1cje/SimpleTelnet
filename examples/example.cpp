/**
 * example.cpp
 *
 * 16th May 2022 - Version 2.1, initial release
 *
 * To Do.-
 *
 **/
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include <SimpleTelnet.h>

#define TZ_DST "GMT0BST,M3.5.0/1,M10.5.0" // UK TZ string

void toggleSecs(byte cID, char *cbuff); // Menu command to toggle time display
char *cleanAsctime(char *timeStr);      // Clean up the asctime output

const char *ssid = "yourssid";             // The SSID (name) of the Wi-Fi network you want to connect to
const char *password = "yourpsk";          // The password of the Wi-Fi network
const char *ntpserver = "uk.pool.ntp.org"; // The ntp server we will use
bool secDisp[MAXCLIENTS] = {false};        // note 2nd element etc always inits to false

void setup()
{
    // Start the Serial communication to send messages to the computer
    Serial.begin(74880);
    delay(100);
    Serial.println('\n');

    // Connect WiFi
    WiFi.hostname((const char *)__PROJECT); // Set hostname
    WiFi.persistent(false);
    WiFi.begin(ssid, password); // Connect to the network
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println(" ...");
    int i = 0;
    while (WiFi.status() != WL_CONNECTED) // Wait for the Wi-Fi to connect
    {
        delay(1000);
        Serial.print(++i);
        Serial.print(' ');
    }
    Serial.println('\n');
    Serial.println("Connection established!");
    Serial.print("IP address:\t");
    Serial.println(WiFi.localIP()); // Send the IP address of the ESP8266 to the computer

    // Set the clock
    configTime(TZ_DST, ntpserver);
    while (time(nullptr) < 180) // Wait for clock to be set, assumes it's not still 1/1/1970
        delay(10);              // Wait a mo for NTP
    time_t now = time(nullptr);
    Serial.printf_P(PSTR("Time syncronised: %s"), asctime(localtime(&now)));

    // Set up telnet server
    telnetServer.begin();
    telnetServer.insertNode(PSTR("showtime"), PSTR("Toggle seconds display"), toggleSecs); // Add a menu command

    // telnetServer.setUserId(PSTR("andrew"));
    // telnetServer.setUserPw(PSTR("login"));
    Serial.printf_P(PSTR("Telnet server started:\r\n"));
}

void loop(void)
{
    telnetServer.action(); // Telnet server processing time

    static time_t lastnow = 0;
    time_t now = time(nullptr); // Get the time

    if (now != lastnow) // 1 second has passed
    {
        static int lastHour = 61;
        tm tm;
        gmtime_r(&now, &tm); // split time components
        lastnow = now;
        for (auto i = 0; i < MAXCLIENTS; i++) // Iterate through all clients
        {
            if (secDisp[i]) // If time display is enabled
            {
                telnetClients[i].printf_P(PSTR("\r%s\r"), cleanAsctime(asctime(localtime(&now)))); // Print time string
                if (tm.tm_hour != lastHour)
                    telnetClients[i].printf_P(PSTR("\n")); // 1 hour has passed
            }
        }
        if (tm.tm_hour != lastHour) // reset the hour marker
            lastHour = tm.tm_hour;
    }
}

// Menu callback function to enable time display.  Must always be declared as void name(byte, char*)
void toggleSecs(byte cID, char *cbuff)
{
    secDisp[cID] = !secDisp[cID];
    telnetClients[cID].printf_P(PSTR("Seconds display %s"), secDisp[cID] ? "enabled" : "disabled");
}

// Remove the trailing newline from the time string so it stays on a single line
char *cleanAsctime(char *timeStr)
{
    timeStr[strlen(timeStr) - 1] = 0;
    return timeStr;
}
