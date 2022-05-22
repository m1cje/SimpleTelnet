# SimpleTelnet

## A Library for implementing a simple telnet interface suitable for logging program output

This is a library providing simple to use telnet support suitable for logging output to a remote client.

* Small footprint
* Simple to implement
* No library dependancies
* Supports multiple clients
* Extensible user menu

See example.cpp for typical usage

### About clients and servers
There are two parts to the SimpleTelnet library.  There is a server object called telnetServer.  This object is reponsible for listening out for new clients and for receiving client data and then buffering the data and passing it back to your program for processing.<br>
When a client connects to the server a new client object is created.  There can be multiple client connected simultaneously so these are all held in an array called telnetClients[].  There will be one entry for each remote client that connects to he server.  The maximum number of clients allowed is set by MAXCLIENTS which is defined in the header file.

### Using the Library
To use the library you will need to include the header file.
```
#include <SimpleTelnet.h>
```
Then in your setup() function you will need to call the begin function.  This will initialise the telnet server once you have connected to WiFi.
```
void setup()
{
  initialise_wifi_here();
  telnetServer.begin();
}
```
This will enable the library functions.  The only other requirement is to call the action() routine regularly within your loop() function.
```
void loop()
{
  telnetServer.action();
  do_your_stuff_here();
}
```
With just these three lines of code you should be able to connect to your IoT device from any telnet client and receive a default menu and command prompt.
```
  telnet your.IP
```

### Sending output to the telnet clients
Telnet clients are supported through a client array called telnetClients[].  Typically when logging unsolicited output from your program you will want to send it to all connected clients.  To do this you will need to iterate through the telnetClients array sending your output to each client in turn.
```
For (auto i=0;i<MAXCLIENTS;i++)
  telnetClients[i].print("Hello Client\r\n");
```
The client supports all the standard client stream methods. MAXCLIENTS is defined in SimpleTelnet.h and defaults to two clients.  Each additional client will require about 100 additional bytes of heap memory and will incur processing time to check for received data, so don't increase this number unless you really need to.

### Extending the user menu
The menu the client sees when logging in can be extended to add your own commands.  Commands are added using the insertNode() method.  See the function reference below for usage details.  Each command you add will need an associated call back function that will process the command according to your applications requirements.<br>
Your callback function will receive two parameters to help you service the request.  The first will be the client id number, this will index into the telnetClients[] array so you can send any reply as required.  The second parameter received will be a pointer to the command buffer that was entered by the user.  This may be required if you are expecting the user to provide additional information to support the command.  Note that the buffer contents are only valid until your function completes so if you need to persist any of the information there then you will need to store it somewhere else.

### Security
The telnet protocol is inherently unsecure because it sends the userid and password in clear text over the network.  The library provides a simple userid/password security mechanism that can be invoked by setting a user id and/or a user password.  If either are set then the user will be prompted appropriately at login and will be unable to enter commands until these have been correctly matched.  Note that non solicited output will still be received by the client pending a sucessful login.

### Function Reference
#### void telnetServer.begin(void), void telnetServer.begin(int port)
This function needs to be called within your setup() function after you have initialised and connected to the network.  If port is not specified then it will default to port 23 which is the well known port used for telnet.
##### Example
```
telnetServer.begin();  // Initialise telnet
```
#### void telnetServer.action(void)
This function needs to be called from within your loop() function.  It is this function that will check for new connection requests and also for data received from already connected clients, so it needs to be called regularly.  If you are losing receive data then it is likely that your loop() function is taking too long to process. Check that you don't have any delay() function calls in your loop for example.
##### Example
```
telnetServer.action();  // Service telnet
```
#### void telnetServer.insertNode(const char *text, const char *helptext, void (*action)(byte cID, char *cbuff) [,byte matchlen])
This function will add a new command to the client menu.  If the command already exists it will be updated with the new values passed.  This could be used for example to override the default menu commands or to dynamically change the commands available.<br>The function is PROGMEM aware.  As menu commands and helptext are typically static and may consume a large amount of memory, it would be good practice to declare these using the PSTR() macro to keep them in flash memory.
##### Parameters
  _const char *text_ - This is the command to add. Typically it should be a single short word<br>
  _const char *helptext_ - This is the command description that will be displayed alongside the menu command.  If this is set to a null string then the command will be added to the list but it will not be displayed on the menu.  This can be used to set an allias for an already existing command.  e.g. the default menu shows the command _quit_ but also aliases the command _exit_ which is not shown on the menu.<br>
  _void (*action)(byte cID, char *cbuff)_ - This is a pointer to your callback service routine that will process the command that you are adding.  Your function must be declared as _void name(byte cID,char*cbuff);_<br>
  _byte matchlen_ - This optional parameter specifies how many characters need to be matched to accept the command.  It is used where for example you are passing a value into your function.  e.g. _set value=x_, for this command you would only want to match _set value=_ then parse the value of x within your callback function.  Setting matchlen to 10 would mean the command was matched for any value of x entered.  If matchlen is omitted then only an exact match will be accepted.
##### Returns
  Nothing.<br>
##### Example
```
insertNode(PSTR("set"), PSTR("Set parameter"), _setParm, 3);  // Add set command to menu
```
#### void printList(byte clientID)
This function will send the user menu to the remote client specified.  This is the command called by default when the user enters the _help_ command<br>
##### Parameters
  _byte clientID_ - This is client that the menu will be sent to.
##### Returns
  Nothing.
##### Example
```
printList(0); // Display menu for client 0
```
#### void setTimeout(byte clientID, uint16_t tmins)
This function will set the inactivity timeout for a client.  Setting a timeout of 0 will disable the timeout leaving the client connected until manually cleared. Note timeouts are per client and are reset on client disconnection<br>
##### Parameters
  _byte clientID_ - This is client that will be set.
  _uint16_t tmins_ - This is the timeout period in minutes, 0 = no timeout.
##### Returns
  Nothing.
##### Example
```
setTimeout(0,0); // Set client 0 to never timeout
```
#### uint16_t getTimeout(byte clientID)
This function returns the timeout remaining until a client will be disconnected<br>
##### Parameters
  _byte clientID_ - This is client that will be set.
  _uint16_t tmins_ - This is the timeout period in minutes, 0 = no timeout.
##### Returns
  _uint16_t mins_ - This is the timeout period remaining in minutes, 0 = no timeout.
##### Example
```
uint16_t timeRemaining = getTimeout(0); // Get client 0 timeout remaining
```
#### void setUserId(const char *id)
This function sets the user id. The function is PROGMEM aware.<br>
##### Parameters
  _const char *id_ - This is the user is that needs to be entered at login.
##### Returns
  Nothing.
##### Example
```
telnetServer.setUserId(PSTR("myUsrID"));
```
#### void setUserPw(const char *id)
This function sets the user password.  Note the pw will be echoed to the console by the server. The function is PROGMEM aware.<br>
##### Parameters
  _const char *id_ - This is the user is that needs to be entered at login.
##### Returns
  Nothing.
##### Example
```
telnetServer.setUserPw(PSTR("myUsrPW"));
```
