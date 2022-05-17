/** *
 * SimpleTelnet.cpp
 *
 * Implements basic telnet server functions with extensible help menu and function processing
 *
 * To Do.-
 *
 * V2.0 - 5th May 2022  Rewrite to use classes, PROGMEM variables and multiclient support
 * V2.1 - 14th May 2022 Initial release version
 *
 * */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Time.h>
#include <SimpleTelnet.h>

// Enable VCC monitoring for info report
#ifndef LOGVCC
ADC_MODE(ADC_VCC);
#define LOGVCC
#endif

//////////////////////////////////////////////////////
// Global variables
//////////////////////////////////////////////////////
WiFiServer _telnetServer(TELNETPORT); // Telnet server, describes the server
WiFiClient telnetClients[MAXCLIENTS]; // Telnet client, describes the connected clients
SimpleTelnet telnetServer;            // the telnet server class

//////////////////////////////////////////////////////
// Internal menu support functions
//////////////////////////////////////////////////////
void _showHelpMessage(byte clientID, char *buff);
void _telnetInfo(byte clientID, char *buff);
void _telnetWiFiinfo(byte clientID, char *buff);
void _telnetReboot(byte clientID, char *buff);
void _listSessions(byte clientID, char *buff);
void _setParm(byte clientID, char *buff);
void _endSession(byte clientID, char *buff);
void _killSession(byte clientID, char *buff);
char *_printElapsedTime(char *buff, time_t elapsedTime);

//////////////////////////////////////////////////////
// Node class support
//////////////////////////////////////////////////////

// Node is used to store the instance data and the pointer to the next node
class Node
{
public:
    const char *commandHelp;                      // Pointer to menu help text
    const char *commandText;                      // Pointer to command text to match
    void (*commandAction)(byte cID, char *cbuff); // pointer to function to process the command
    byte matchlen;                                // Length of command required for a match
    Node *next;                                   // Pointer to next instance

    Node(const char *text, const char *helptext, void (*action)(byte cID, char *cbuff), byte mlen) // Default constructor, adds the details to a new node
    {
        commandHelp = helptext; // Store pointer to command help text
        commandText = text;     // Store pointer to command text
        commandAction = action; // Store pointer to function to action the command
        matchlen = mlen;        // Store command match length required
        next = NULL;            // Init pointer to next item in the list
    }
    Node(int data) // Parameterised Constructor
    {
        commandHelp = NULL;
        commandText = NULL;
        commandAction = NULL;
        matchlen = 0;
        next = NULL;
    }
};

//////////////////////////////////////////////////////
// Constructor
//////////////////////////////////////////////////////
SimpleTelnet::SimpleTelnet(void)
{
}

void SimpleTelnet::begin(void)
{
    begin(TELNETPORT);
}

void SimpleTelnet::begin(uint16_t port)
{
    _telnetServer.begin(port);            // start TCP server on port 23
    _telnetServer.setNoDelay(true);       // Turns off nagle
    for (auto i = 0; i < MAXCLIENTS; i++) // Initialise parsing vars
        _resetParser(i);
    _addStdMenu();
}

//////////////////////////////////////////////////////
// loop() service routine, needs to be called from loop()
//////////////////////////////////////////////////////
void SimpleTelnet::action(void) // Service routine, called by loop()
{
    if (!_bootTime)                // Set the clock
        _bootTime = time(nullptr); // Seconds since 1/1/1970

    // Check for new connection
    if (_telnetServer.hasClient()) // true if someone is trying to connect
    {
        bool ConnectionRequest = true; // flag set to indicate we have a new connection that needs to be dealt with
        Serial.printf_P(PSTR("New connection detected\r\n"));
        for (auto i = 0; i < MAXCLIENTS; i++)
        {
            if (!telnetClients[i].connected()) // Find a free connection
            {
                telnetClients[i] = _telnetServer.available(); // Store the client object
                telnetClients[i].setNoDelay(true);            // Turns off nagle
                telnetClients[i].printf_P(PSTR("Welcome to %s %s, Press <ESC> to exit\r\n"), __PROJECT, __VERSION_SHORT);
                printList(i);
                _parseChar(0x00, i);            // Force new prompt to output
                _connectionTimer[i] = millis(); // Set timeout timer
                _resetParser(i);                // Clear the parser vars for this new client
                IPAddress ip = telnetClients[i].remoteIP();
                Serial.printf_P(PSTR("Telnet client connected from %d.%d.%d.%d:%d on slot %d\r\n"), ip[0], ip[1], ip[2], ip[3], telnetClients[i].remotePort(), i);
                ConnectionRequest = false; // Clear request flag
                break;
            }
        }
        if (ConnectionRequest) // Connection request is still outstanding but we don't have any resource to deal with it so kill the request
        {
            WiFiClient abortConnection = _telnetServer.available(); // Store the client object
            IPAddress ip = abortConnection.remoteIP();
            Serial.printf_P(PSTR("Telnet connection request from %d.%d.%d.%d:%d rejected\r\n"), ip[0], ip[1], ip[2], ip[3], abortConnection.remotePort());
            abortConnection.setNoDelay(true); // Turns off nagle
            abortConnection.printf_P(PSTR("\r\nWelcome to %s %s, sorry no connections are available at the moment, please try again later...\r\n"), __PROJECT, __VERSION_SHORT);
            abortConnection.flush();
            abortConnection.stop();
        }
    }

    // Check for received data
    for (auto i = 0; i < MAXCLIENTS; i++)
    {
        if (telnetClients[i].connected()) // if this client has a connection
        {
            if (telnetClients[i].available()) // Received a char
            {
                _connectionTimer[i] = millis(); // Reset timeout timer
                _timeoutWarning[i] = false;     // Clear flag to say we have issued the timeout warning
                _parseChar(telnetClients[i].read(), i);
            }
            else // Nothing received, chack for idle timeout
            {
                if (_connectionTimeout[i]) // if timeout is set
                {
                    if ((millis() - _connectionTimer[i]) > (_connectionTimeout[i] - IDLEWARNING) && !_timeoutWarning[i]) // Check idle timeout
                    {
                        _timeoutWarning[i] = true; // Set flag to say we have issued the warning
                        Serial.printf_P(PSTR("Client %d Inactivity timeout in 300 seconds\r\n"), i);
                        telnetClients[i].printf_P(PSTR("Inactivity timeout in 300 seconds\r\n"));
                    }
                    if (millis() - _connectionTimer[i] > _connectionTimeout[i]) // Check idle timeout
                    {
                        Serial.printf_P(PSTR("Client %d Inactivity timeout\r\n"), i);
                        telnetClients[i].printf_P(PSTR("Inactivity timeout, bye\r\n"));
                        telnetClients[i].flush(); // Flush any tx data
                        telnetClients[i].stop();  // Session timeout, clear the connection
                    }
                }
            }
        }
    }
}

//////////////////////////////////////////////////////
// Clears the parser vars ready for a new client connection
//////////////////////////////////////////////////////
void SimpleTelnet::_resetParser(byte clientID)
{
    _rxbuff[clientID][0] = '\0';
    _lastbuff[clientID][0] = '\0';
    _timeoutWarning[clientID] = false;
    _uparrowState[clientID] = 0;
    _connectionTimeout[clientID] = IDLETIMEOUT;
}

//////////////////////////////////////////////////////
// Process a received data character
//////////////////////////////////////////////////////
void SimpleTelnet::_parseChar(char rxval, byte clientID)
{
    static byte rxptr[MAXCLIENTS] = {0}; // Pointer to next free space in tx buffer
    bool eol[MAXCLIENTS] = {false};      // end of line received indicator causing input to be processed

    Serial.printf("[%d]%c", clientID, rxval);

    // Check for up-arrow (Esc 5b 41)
    if (rxval == 0x1B) // Esc
    {
        _uparrowState[clientID] = 1;
        rxval = 0xFF; // reset val to be ignored
    }
    else if (rxval == 0x5B && _uparrowState[clientID] == 1) // Esc 5B
    {
        _uparrowState[clientID] = 2;
        rxval = 0xFF; // reset val to be ignored
    }
    else if (rxval == 0x41 && _uparrowState[clientID] == 2) // Esc 5B 41, got an up arrow
    {
        strcpy(_rxbuff[clientID], _lastbuff[clientID]);                     // restore previous command
        rxptr[clientID] = strlen(_rxbuff[clientID]);                        // restore pointer
        telnetClients[clientID].printf_P(PSTR("\r>%s"), _rxbuff[clientID]); // rewrite command to screen
        rxval = 0xFF;                                                       // reset val to be ignored
    }
    else
        _uparrowState[clientID] = 0; //  reset state, not up arrow string

    switch (rxval)
    {
    case 0x00:                       // Special case to Flush buffer and Display command prompt
        eol[clientID] = true;        // reset eol flag
        rxptr[clientID] = 0;         // Reset ptr to start new line
        _rxbuff[clientID][0] = '\0'; // reset buffer
        break;
    case 0x08: // backspace
        if (rxptr[clientID])
        {
            rxptr[clientID]--;
            telnetClients[clientID].printf_P(PSTR(" \x08")); // clear last char 1B 5B 44 is left arrow, 08 is backspace
        }
        else
            telnetClients[clientID].printf_P(PSTR(">")); // Replace the prompt if nothing in the buffer
        break;
    case 0x0D: // cr
        if (strlen(_rxbuff[clientID]))
            eol[clientID] = true; // new command entered so process it
        else
            telnetClients[clientID].print(F(">")); // crlf ready for the next output
        break;
    case 0x1B: // Escape
    case '\t': // tab
    case 0x0A: // nl
    case 0xFF: // invalid char or char removed
        break; // Discard these chars
    default:
        if (rxptr[clientID] < RXBUFFLEN - 1)              // if there is space in the command buffer
            _rxbuff[clientID][rxptr[clientID]++] = rxval; // store the received char and point to the next free space
        if (rxptr[clientID] == RXBUFFLEN - 1)
            eol[clientID] = true; // We filled the rx buffer so process it
        break;
    }
    _rxbuff[clientID][rxptr[clientID]] = '\0'; // Add new null terminator to rx buffer

    if (eol[clientID])
    {
        strcpy(_lastbuff[clientID], _rxbuff[clientID]);         // save previous command
        telnetClients[clientID].print(F("\r"));                 // crlf ready for the next output
        if (rxptr[clientID])                                    // if we have a command to check
        {                                                       //
            if (!_ProcessList(_rxbuff[clientID], clientID))     // Run the command entered past the handler functions
                telnetClients[clientID].print(F(">What?\r\n")); // print new prompt
            else
                telnetClients[clientID].print(F("\r\n")); // crlf ready for the next output
        }
        telnetClients[clientID].print(F("\r>")); // crlf ready for the next output
        eol[clientID] = false;                   // reset eol flag
        rxptr[clientID] = 0;                     // Reset ptr to start new line
        _rxbuff[clientID][0] = '\0';             // reset buffer
    }
}

//////////////////////////////////////////////////////
// Linked list support functions
//////////////////////////////////////////////////////

bool SimpleTelnet::_ProcessList(char *command, byte cID) // Search the list for thie command in command parm
{
    Node *flist = head;
    while (flist != NULL) // Traverse the list.
    {
        int cresult = 0;
        if (flist->matchlen) // limited length match required
            cresult = strncmp_P(command, flist->commandText, flist->matchlen);
        else
            cresult = strcmp_P(command, flist->commandText); // s2 is in PROGMEM, rets 0 if matched
        if (!cresult)                                        // Check for command match
        {
            flist->commandAction(cID, command); // Run command
            return true;                        // indicate that we matched the command
        }
        flist = flist->next; // Iterate to next member
    }
    return false; // failed to match the command
}

// Compare two strings in flash, compatible with strcmp()
int strcmp_PP(const char *a, const char *b)
{
    const char *aa = reinterpret_cast<const char *>(a);
    const char *bb = reinterpret_cast<const char *>(b);

    while (true)
    {
        uint8_t ca = pgm_read_byte(aa);
        uint8_t cb = pgm_read_byte(bb);
        if (ca != cb)
            return (int)ca - (int)cb;
        if (ca == 0)
            return 0;
        aa++;
        bb++;
    }
}

// Search the node list to see if a command alreay exists
Node *SimpleTelnet::_findCommand(const char *commandtext)
{
    Node *flist = head;
    while (flist != NULL) // Traverse the list.
    {
        if (!strcmp_PP(commandtext, flist->commandText)) // s2 is in PROGMEM, rets 0 if matched
            break;
        flist = flist->next; // Iterate to next member
    }
    return flist;
}

// Adds a new node to the list.  P1 = command string to match, P2 = Help message to display, P3 = function to process the command
void SimpleTelnet::insertNode(const char *commandtext, const char *helptext, void (*action)(byte cID, char *cbuff)) // Function to insert a new node.
{
    insertNode(commandtext, helptext, action, 0); // Function to insert a new node. default to exact match
}
void SimpleTelnet::insertNode(const char *commandtext, const char *helptext, void (*action)(byte cID, char *cbuff), byte matchLen) // Function to insert a new node, matchlen is the length of cammand required to match for a hit
{
    Node *enode = _findCommand(commandtext);
    if (enode) // command already exists update it
    {
        enode->commandAction = action;
        enode->commandHelp = helptext;
        enode->commandText = commandtext;
        enode->matchlen = matchLen;
        return;
    }

    Node *newNode = new Node(commandtext, helptext, action, matchLen); // Create the new Node.
    if (head == NULL)
    {
        head = newNode; // First item so assign to head
        return;
    }
    // Find the last item in the list
    Node *temp = head;
    while (temp->next != NULL)
        temp = temp->next; // Update temp
    temp->next = newNode;  // Insert the new node at the last.
}

void SimpleTelnet::printList(byte clientID)
{
    Node *flist = head;
    while (flist != NULL) // Traverse the list.
    {
        if (strlen_P(flist->commandHelp)) // Only list the command if there is some help text assosiated, this allows aliases to be defined
            telnetClients[clientID].printf_P(PSTR("%s%10s%s\t%s\r\n"), COLOUR_YELLOW, FPSTR(flist->commandText), COLOUR_RESET, FPSTR(flist->commandHelp));
        flist = flist->next; // Iterate to next member
    }
}

//////////////////////////////////////////////////////
// Set the inactivity timeout to tmins minutes
//////////////////////////////////////////////////////
void SimpleTelnet::setTimeout(byte clientID, uint16_t tmins)
{
    _connectionTimeout[clientID] = tmins * 60 * 1000;
}

//////////////////////////////////////////////////////
// Get the remaining inactivity timeout in minutes
//////////////////////////////////////////////////////
uint16_t SimpleTelnet::getTimeout(byte clientID)
{
    //    return ((uint16_t)((_connectionTimeout[clientID] - _connectionTimer[clientID]) / 60000));
    if (_connectionTimeout[clientID])
    {
        time_t elapsed = millis() - _connectionTimer[clientID];
        time_t remaining = _connectionTimeout[clientID] - elapsed;
        return (uint16_t)round(remaining / 60000);
    }
    else
        return 0;
}

//////////////////////////////////////////////////////
// Return the time_t passed since last boot.
//////////////////////////////////////////////////////
time_t SimpleTelnet::_uptime(void) // Returns the seconds elapsed since boot
{
    return now() - _bootTime;
}

time_t SimpleTelnet::now(void)
{
    _timeNow = time(nullptr);
    return _timeNow;
}

//////////////////////////////////////////////////////
// Prints an elapsed time in seconds to buff.  Note buff should be at least 80 chars long
//////////////////////////////////////////////////////
char *_printElapsedTime(char *buff, time_t elapsedTime)
{
    tm tm;                // Struct to store time parts
    const char *pl = "s"; // plural

    gmtime_r(&elapsedTime, &tm);
    tm.tm_mday--;     // days start at 1
    tm.tm_year -= 70; // year is returned offset from 1900

    buff[0] = '\0';
    if (tm.tm_year)
        sprintf_P(buff, PSTR("%d Year%s, "), tm.tm_year, tm.tm_year > 1 ? pl : "");
    if (tm.tm_mon)
        sprintf_P(buff + strlen(buff), PSTR("%d Month%s, "), tm.tm_mon, tm.tm_mon > 1 ? pl : "");
    if (tm.tm_mday)
        sprintf_P(buff + strlen(buff), PSTR("%d Day%s, "), tm.tm_mday, tm.tm_mday > 1 ? pl : "");
    if (tm.tm_hour)
        sprintf_P(buff + strlen(buff), PSTR("%d Hour%s, "), tm.tm_hour, tm.tm_hour > 1 ? pl : "");
    if (tm.tm_min)
        sprintf_P(buff + strlen(buff), PSTR("%d Minute%s & "), tm.tm_min, tm.tm_min == 1 ? "" : pl);
    sprintf_P(buff + strlen(buff), PSTR("%d Second%s"), tm.tm_sec, tm.tm_sec == 1 ? "" : pl);
    return buff;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Menu support functions
//////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Menu support functions are passed the client id and a pointer to the command buffer and return void
//
// All functions must be declared as void name(byte clientID, char *buff)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////
// Remove the trailing newline from the time string
char *_cleanAsctime(char *timeStr)
{
    timeStr[strlen(timeStr) - 1] = 0;
    return timeStr;
}
//////////////////////////////////////////////////////
// Displays the system help message
//////////////////////////////////////////////////////
void _showHelpMessage(byte clientID, char *buff)
{
    time_t now = time(nullptr);
    telnetClients[clientID].printf_P(PSTR("[Client %d] Menu options.-\r\n"), clientID + 1);
    telnetClients[clientID].printf_P(PSTR("Current time is %s\r"), asctime(localtime(&now)));
    telnetServer.printList(clientID);
}

//////////////////////////////////////////////////////
// Displays system information
//////////////////////////////////////////////////////
void _telnetInfo(byte clientID, char *buff)
{
    IPAddress ip;
    char timeBuff[80];
    telnetClients[clientID].printf_P(PSTR("This is %s %s - system info:\r\n"), __PROJECT, __VERSION_SHORT);
    telnetClients[clientID].printf_P(PSTR("\tMCU: Flash id 0x%06X:0x%06X\r\n"), ESP.getChipId(), ESP.getFlashChipId());
    telnetClients[clientID].printf_P(PSTR("\tLast boot code%s:\r\n"), ESP.getResetReason().c_str());
    telnetClients[clientID].printf_P(PSTR("\tLast boot time %sz\r\n"), _cleanAsctime(asctime(gmtime(&telnetServer._bootTime))));
    telnetClients[clientID].printf_P(PSTR("\tUptime %s\r\n"), _printElapsedTime(timeBuff, telnetServer.now() - telnetServer._bootTime));
    telnetClients[clientID].printf_P(PSTR("\tFlash size %u\r\n"), ESP.getFlashChipRealSize());
    telnetClients[clientID].printf_P(PSTR("\tFree cont stack  %u\r\n"), ESP.getFreeContStack());
    telnetClients[clientID].printf_P(PSTR("\tFree memory (heap) %u\r\n"), ESP.getFreeHeap());
    telnetClients[clientID].printf_P(PSTR("\tMax free block size %u\r\n"), ESP.getMaxFreeBlockSize());
    telnetClients[clientID].printf_P(PSTR("\tHeap fragmentation %u%%\r\n"), ESP.getHeapFragmentation());
    telnetClients[clientID].printf_P(PSTR("\tFree sketch space %u\r\n"), ESP.getFreeSketchSpace());
    telnetClients[clientID].printf_P(PSTR("\tHostname %s\r\n"), WiFi.hostname());
    telnetClients[clientID].print(F("\tIP Address "));
    WiFi.localIP().printTo(telnetClients[clientID]);
    telnetClients[clientID].print(F("\r\n\tIP Mask    "));
    WiFi.subnetMask().printTo(telnetClients[clientID]);
    telnetClients[clientID].print(F("\r\n\tIP Gateway "));
    WiFi.gatewayIP().printTo(telnetClients[clientID]);
    telnetClients[clientID].print(F("\r\n\tDNS server "));
    WiFi.dnsIP().printTo(telnetClients[clientID]);
    telnetClients[clientID].print(F("\r\n\tYour IP    "));
    telnetClients[clientID].remoteIP().printTo(telnetClients[clientID]);
    telnetClients[clientID].printf_P(PSTR(":%d\r\n"), telnetClients[clientID].remotePort());
    telnetClients[clientID].printf_P(PSTR("\tMAC address %s\r\n"), WiFi.macAddress().c_str());
    telnetClients[clientID].printf_P(PSTR("\tSSID %s\r\n"), WiFi.SSID().c_str());
    telnetClients[clientID].printf_P(PSTR("\tRSSI %ddBm\r\n"), WiFi.RSSI());
#ifdef LOGVCC
    telnetClients[clientID].printf_P(PSTR("\tVCC %.3f Volts\r\n"), (float)ESP.getVcc() / 1000);
#endif
}

//////////////////////////////////////////////////////
// Displays wifi info
//////////////////////////////////////////////////////
void _telnetWiFiinfo(byte clientID, char *buff)
{
    telnetClients[clientID].printf_P(PSTR("This is %s %s - WiFi status:\r\n"), __PROJECT, __VERSION_SHORT);
    WiFi.printDiag(telnetClients[clientID]);
}

//////////////////////////////////////////////////////
// Soft reboots the MCU
//////////////////////////////////////////////////////
void _telnetReboot(byte clientID, char *buff)
{
    telnetClients[clientID].println(F("* Reset ...\r\n* Closing telnet connection ...\r\n* Resetting the ESP8266 ..."));
    telnetClients[clientID].stop();
    _telnetServer.stop();
    delay(500);    // Give it time to stop
    ESP.restart(); // reboot
}

//////////////////////////////////////////////////////
// List current sessions
//////////////////////////////////////////////////////
void _listSessions(byte clientID, char *buff)
{
    for (auto i = 0; i < MAXCLIENTS; i++)
    {
        if (telnetClients[i].connected())
        {
            telnetClients[clientID].printf_P(PSTR("\tClient [%d] IP "), i + 1);
            telnetClients[i].remoteIP().printTo(telnetClients[clientID]);
            telnetClients[clientID].printf_P(PSTR(":%d (T:%d)%c\r\n"), telnetClients[i].remotePort(), telnetServer.getTimeout(i) + 1, clientID == i ? '*' : ' ');
        }
    }
}

//////////////////////////////////////////////////////
// Set parameters
//////////////////////////////////////////////////////
void _setParm(byte clientID, char *buff)
{
    char *cmd; // Pointer to the set command
    char *p1;  // Pointer to the command parameter
    bool cmdInvalid = true;

    if (strtok(buff, " ")) // if we have a space in the buff
    {
        cmd = strtok(NULL, "="); // get the cmd word terminated by =
        if (cmd)
        {
            if (!strcmp_P(cmd, PSTR("timeout")))
            {
                p1 = strtok(NULL, " "); // get the parameter
                if (p1)
                {
                    int tmins = atoi(p1);
                    if (tmins >= 0)
                    {
                        telnetServer.setTimeout(clientID, tmins);
                        telnetClients[clientID].printf_P(PSTR("\tInactivity timeout set to %d minutes"), tmins);
                        cmdInvalid = false;
                    }
                }
            }
        }
    }
    if (cmdInvalid)
        telnetClients[clientID].printf_P(PSTR("Invalid set command\r\n\tUse: set parameter=value\r\n\tSupported commands.-\r\n\ttimeout=minutes"));
}

//////////////////////////////////////////////////////
// Kills the session
//////////////////////////////////////////////////////
void _endSession(byte clientID, char *buff)
{
    telnetClients[clientID].print(F("\r\nBye bye, thanks for connecting\r\n"));
    telnetClients[clientID].flush(); // Flust tx buff
    telnetClients[clientID].stop();  // Kill session
}
//////////////////////////////////////////////////////
// Kills a session syntax kill session=X
//////////////////////////////////////////////////////
void _killSession(byte clientID, char *buff)
{
    char *cmd; // Pointer to the kill command
    char *p1;  // Pointer to the session parameter
    bool cmdInvalid = true;

    if (strtok(buff, " ")) // if we have a space in the buff
    {
        cmd = strtok(NULL, "="); // get the cmd word terminated by =
        if (cmd)
        {
            if (!strcmp_P(cmd, PSTR("session")))
            {
                p1 = strtok(NULL, " "); // get the parameter
                if (p1)
                {
                    int sessionid = atoi(p1);
                    if ((sessionid > 0) && (sessionid <= MAXCLIENTS) && telnetClients[sessionid - 1].connected())
                    {
                        telnetClients[clientID].printf_P(PSTR("\tSession %d ended forcefully by client [%d]"), sessionid, clientID + 1);
                        if (sessionid != clientID)
                            telnetClients[sessionid].printf_P(PSTR("\tSession %d ended forcefully by client [%d]"), sessionid, clientID + 1);
                        _endSession(sessionid - 1, NULL); // Kill the session
                        cmdInvalid = false;
                    }
                }
            }
        }
    }
    if (cmdInvalid)
    {
        telnetClients[clientID].printf_P(PSTR("Invalid kill command or session is not active\r\n\tUse: kill session=value\r\n\tvalue should be between 1 and %d\r\n\tAvailable clients.-\r\n\t"), MAXCLIENTS);
        _listSessions(clientID, NULL);
    }
}

//////////////////////////////////////////////////////
// Adds the standard menu items to the help command
//////////////////////////////////////////////////////
void SimpleTelnet::_addStdMenu(void)
{
    insertNode(PSTR("help"), PSTR("Display this help1 message"), _showHelpMessage);
    insertNode(PSTR("info"), PSTR("System Information"), _telnetInfo);
    insertNode(PSTR("wifi"), PSTR("WiFi Information"), _telnetWiFiinfo);
    insertNode(PSTR("set"), PSTR("Set parameter"), _setParm, 3);
    insertNode(PSTR("sessions"), PSTR("List connected sessions"), _listSessions);
    insertNode(PSTR("kill"), PSTR("Kill a session connection"), _killSession, 4);
    insertNode(PSTR("quit"), PSTR("End the connection"), _endSession);
    insertNode(PSTR("exit"), "", _endSession); // alias on quit command
    insertNode(PSTR("reboot"), PSTR("Reboot the system"), _telnetReboot);
}
