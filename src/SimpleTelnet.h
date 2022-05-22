#pragma once

#define TELNETPORT 23         // Default port
#define MAXCLIENTS 2          // Number of concurrent clients supported
#define IDLETIMEOUT 3600000LL // Default timeout for inactive clients in milliseconds
#define IDLEWARNING 300000LL  // Default timeout for inactive clients in milliseconds
#define RXBUFFLEN 20          // Length of the command receive buffer, set this to the length of the longest command to be received

#ifndef __PROJECT
#define __PROJECT "SimpleTelnet"
#define __VERSION_SHORT "V2.1"
#endif

extern WiFiClient telnetClients[];
class Node; // This defines an element on the liked list

class SimpleTelnet
{
public:
    SimpleTelnet(void);
    void begin(void);                                                                                              // Initialiser, called by setup(), uses default port 23
    void begin(uint16 port);                                                                                       // Initialiser, called by setup(), user defined port
    void action(void);                                                                                             // Service routine, called by loop()
    void insertNode(const char *text, const char *helptext, void (*action)(byte cID, char *cbuff));                // Function to insert a new node, exact match
    void insertNode(const char *text, const char *helptext, void (*action)(byte cID, char *cbuff), byte matchlen); // Function to insert a new node, matchlen is the length of cammand required to match for a hit
    void printList(byte clientID);                                                                                 // Display the menu command list
    void setTimeout(byte clientID, uint16_t tmins);                                                                // Set the inactivity timeout to tmins minutes
    uint16_t getTimeout(byte clientID);                                                                            // Returns the inactivity timeout remaining in minutes
    void setUserId(const char *id);                                                                                // Set a user id
    void setUserPw(const char *pw);                                                                                // Set a user Pw

private:
    Node *head;                            // pointer to first element of the linked list
    time_t _bootTime;                      // Set to the rtc time when we booted. Note, depends on NTP working
    char _rxbuff[MAXCLIENTS][RXBUFFLEN];   // Store received data
    char _lastbuff[MAXCLIENTS][RXBUFFLEN]; // Store previous received data
    time_t _connectionTimer[MAXCLIENTS];   // Stores the millis() time when the last data was received from the client.  Used to timeout clients
    time_t _connectionTimeout[MAXCLIENTS]; // Stores the millis() timeout time
    bool _timeoutWarning[MAXCLIENTS];      // Flag set to say we are about to timeout the session
    byte _uparrowState[MAXCLIENTS];        // state pointer for up arrow processing
    const char *_loginid;                  // Points to a userid or null if none set/required
    const char *_loginpw;                  // Points to a password or null if none set/required
    time_t _timeNow;                       // Buffer to hold the current time as secondssince 1/1/1970
    bool _authenticated[MAXCLIENTS];       // client is logged in sucessfully flag
    bool _idOK[MAXCLIENTS];                // id is ok flag
    bool _pwOK[MAXCLIENTS];                // pw is ok flag

    time_t _uptime(void); // Returns the time_t elapsed since boot
    time_t now(void);     // Returns the current time'
    void _addStdMenu(void);
    void _parseChar(char rxval, byte clientID);
    bool _ProcessList(char *command, byte cID); // Function to process the linked list. command parameter is the command to be processed
    Node *_findCommand(const char *command);    // Search the node list to see if a command alreay exists
    void _resetParser(byte clientID);           // Clears the parser vars ready for a new client connection
    bool _checkid(byte clientID, char *rxbuff); // Check id/pw for client login

    friend void _telnetInfo(byte clientID, char *buff);
    friend void _showHelpMessage(byte clientID, char *buff);
};
extern SimpleTelnet telnetServer; // the telnet server class

// ANSI COLOURs
#define COLOUR_RESET "\x1B[0m"
#define COLOUR_BLACK "\x1B[0;30m"
#define COLOUR_RED "\x1B[0;31m"
#define COLOUR_GREEN "\x1B[0;32m"
#define COLOUR_YELLOW "\x1B[0;33m"
#define COLOUR_BLUE "\x1B[0;34m"
#define COLOUR_MAGENTA "\x1B[0;35m"
#define COLOUR_CYAN "\x1B[0;36m"
#define COLOUR_WHITE "\x1B[0;37m"
#define COLOUR_DARK_BLACK "\x1B[1;30m"
#define COLOUR_LIGHT_RED "\x1B[1;31m"
#define COLOUR_LIGHT_GREEN "\x1B[1;32m"
#define COLOUR_LIGHT_YELLOW "\x1B[1;33m"
#define COLOUR_LIGHT_BLUE "\x1B[1;34m"
#define COLOUR_LIGHT_MAGENTA "\x1B[1;35m"
#define COLOUR_LIGHT_CYAN "\x1B[1;36m"
#define COLOUR_LIGHT_WHITE "\x1B[1;37m"
#define COLOUR_BACKGROUND_BLACK "\x1B[40m"
#define COLOUR_BACKGROUND_RED "\x1B[41m"
#define COLOUR_BACKGROUND_GREEN "\x1B[42m"
#define COLOUR_BACKGROUND_YELLOW "\x1B[43m"
#define COLOUR_BACKGROUND_BLUE "\x1B[44m"
#define COLOUR_BACKGROUND_MAGENTA "\x1B[45m"
#define COLOUR_BACKGROUND_CYAN "\x1B[46m"
#define COLOUR_BACKGROUND_WHITE "\x1B[47m"
