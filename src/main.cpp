#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <EthernetUdp.h>

/// Temperature sampling and control variables
#define DS1621_ADDRESS 0x48

/// Temperature pins
int upTemp = A5;
int downTemp = A4;
/// Temperature settings/////////////////////////////////////////////////////
enum TemperatureSetting { Cold, Set1, Set2, Set3, Set4, Set5, Set6};
TemperatureSetting tsetting = Cold;
float targetTemperature = 21.00;

const char timeServer[] = "time.nist.gov"; // time.nist.gov NTP server
unsigned int localPort = 8888;
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
String newtime = "";
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

/// Http req holder
String HTTP_req;
/// Char arrays til temperatur bytes    
char c_buffer[8], f_buffer[8];

int16_t get_temperature() {
  Wire.beginTransmission(DS1621_ADDRESS); // connect to DS1621 (send DS1621 address)
  Wire.write(0xAA);                       // read temperature command
  Wire.endTransmission(false);            // send repeated start condition
  Wire.requestFrom(DS1621_ADDRESS, 2);    // request 2 bytes from DS1621 and release I2C bus at end of reading
  uint8_t t_msb = Wire.read();            // read temperature MSB register
  uint8_t t_lsb = Wire.read();            // read temperature LSB register
 
  // calculate full temperature (raw value)
  int16_t raw_t = (int8_t)t_msb << 1 | t_lsb >> 7;
  // convert raw temperature value to tenths °C
  raw_t = raw_t * 10/2;
  return raw_t;
}
/// Server variables
// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 0, 177);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);


void settingUp(EthernetClient client)////////////// sætter tsetting ++/-- og returnerer den ny setting
{
  switch(tsetting) {
                case 0: client.print("1");tsetting = Set1; break;
                case 1: client.print("2");tsetting = Set2; break;
                case 2: client.print("3");tsetting = Set3; break;
                case 3: client.print("4");tsetting = Set4; break;
                case 4: client.print("5");tsetting = Set5; break;
                case 5: client.print("6");tsetting = Set6; break;
                case 6: client.print("6");tsetting = Set6; break;
                default: client.print("Your heating system might be broken");
  }
}
void settingDown(EthernetClient client)
{
 switch(tsetting) {
              case 0: client.print("Cold");tsetting = Cold; break;
              case 1: client.print("Cold");tsetting = Cold; break;
              case 2: client.print("1");tsetting = Set1; break;
              case 3: client.print("2");tsetting = Set2; break;
              case 4: client.print("3");tsetting = Set3; break;
              case 5: client.print("4");tsetting = Set4; break;
              case 6: client.print("5");tsetting = Set5; break;
              default: client.print("Your heating system might be broken");
  }
}
/// Get ledchange handling //// de her skal være én void adjustTemp()betinges af targetTemperature og newTemperature - skru op, hvis for koldt og ned...
void adjustTemperature(int m16)
{
 float measured = m16 / 10.00;
  Serial.println("---------------------------------------");
  Serial.println(measured);
  Serial.println("---------------------------------------");
  /// doSomething = target - measure
  float doSomething = measured - targetTemperature;
  /// state= decides if heat open/close signal must be send
  int state = 0;
  Serial.println("---------------------------------------");
  Serial.println(doSomething);
  Serial.println("---------------------------------------");
  /// if doSomething !=0
  if(doSomething != 0){
    /// if doSomething > 0 luk mindre varme ind
    if(doSomething < 0){
      state = digitalRead(upTemp);
      if (state == 1) {
        // already running, so delay and turn off
        digitalWrite(downTemp, LOW);
        digitalWrite(upTemp, LOW);
      }
      else {
        digitalWrite(downTemp, LOW);
        digitalWrite(upTemp, HIGH);
      }
    }
    /// if doSomething < 0 luk mere varme ind
  if(doSomething > 0){
      state = digitalRead(downTemp);
      if (state == 1) {
        // already running, so delay and turn off
        digitalWrite(upTemp, LOW);
        digitalWrite(downTemp, LOW);
      }
      else {
        digitalWrite(upTemp, LOW);
        digitalWrite(downTemp, HIGH);
      }
    }
  }
}

// send an NTP request to the time server at the given address
void sendNTPpacket(const char * address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
void setup() {
///// DS1621 setup
    Wire.begin();
    Wire.beginTransmission(DS1621_ADDRESS);
    Wire.write(0xAC);
    Wire.write(0);
    Wire.beginTransmission(DS1621_ADDRESS);
    Wire.write(0xEE);
    Wire.endTransmission();
     pinMode(20, OUTPUT);

    pinMode(upTemp, OUTPUT); 
    pinMode(downTemp, OUTPUT); 
  /// Open serial communications and wait for port to open for debugging purposes:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  /// start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);

  /// Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware ///////////////////// Man kunne overveje et break; i stedet, så den får chancen igen
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }
  Udp.begin(localPort);
  ///start the server
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP()); //////Debug
}


void loop() {
  ////Temperature reading
  delay(1000);    // wait a second //////////////////måske ikke nødvendigt mere?

  // get temperature in tenths °C
  int16_t c_temp = get_temperature();
  // convert tenths °C to tenths °F
  int16_t f_temp = c_temp * 9/5 + 320 ;

    if(c_temp < 230) {   // if temperature < 23 °C
    c_temp = abs(c_temp);  // absolute value
    sprintf(c_buffer, "-%02u.%1u%cC", c_temp / 10, c_temp % 10, 223);
    //digitalWrite(20,LOW);
  }
  else {
    if (c_temp >= 250)    // if temperature >= 100.0 °C
    {
      sprintf(c_buffer, "%03u.%1u%cC", c_temp / 10, c_temp % 10, 223);
      //digitalWrite(20, HIGH);
    }
    else
    {
      sprintf(c_buffer, " %02u.%1u%cC", c_temp / 10, c_temp % 10, 223); //sprintf = String print format
      
    }
  }
    if(f_temp < 0) {   // if temperature < 0 °F
    f_temp = abs(f_temp);  // absolute value
    sprintf(f_buffer, "-%02u.%1u%cF", f_temp / 10, f_temp % 10, 223);
    digitalWrite(20, HIGH);
  }
  else {
    if (f_temp >= 100)  
    {// if temperature >= 100.0 °F
      sprintf(f_buffer, "%03u.%1u%cF", f_temp / 10, f_temp % 10, 223);
      digitalWrite(20, LOW);
    }
    else
    {
      sprintf(f_buffer, " %02u.%1u%cF", f_temp / 10, f_temp % 10, 223);
    }
  }

   Serial.println(c_buffer);
  Serial.println(f_buffer);  // print f_buffer (temperature in °F)
  adjustTemperature(c_temp);
  //////// listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    sendNTPpacket(timeServer); // send an NTP packet to a time server

  // wait to see if a reply is available
  delay(1000);
  if (Udp.parsePacket()) {
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    // the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print the hour, minute and second:
    
    newtime = "";
    newtime = (epoch  % 86400L) / 3600;
    newtime += ":";
  
    if (((epoch % 3600) / 60) < 10) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      newtime += "0";
    }
    newtime += (epoch  % 3600) / 60;
  
    newtime += ":";
  
    if ((epoch % 60) < 10) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      newtime += "0";
    }
    newtime += epoch % 60;
  }
  // wait ten seconds before asking for the time again
 // delay(10000);
  Ethernet.maintain();
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
         if( HTTP_req.length() < 120) 
           HTTP_req += c; // save the HTTP request 1 char at a time 
           Serial.write(c); 

        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          Serial.println(HTTP_req);
           if (HTTP_req.indexOf("settingUp") >= 0 ) {
            // read switch state and analog input
            settingUp(client);
            break;
          }else if (HTTP_req.indexOf("settingDown") >= 0 ) {
            // read switch state and analog input
            settingDown(client);
            break;
          }
          else{

          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          client.println("<head>");
          client.println("<script type=\"text/javascript\">");///////// Action goees here
                   
          // client.println("function turnTemperatureUp(){console.log(true)}");///////// Action goees here////bruges ikke i øjeblikket
          // client.println("function settingDown(){console.log(true)}");///////// Action goees here

          client.println("</script>");///////// Action ends here
           
          client.println("<style>");///////// Styling goees here
          client.print("*{background-color:#000;color:hotpink;margin:6px auto;}h1{color:#00F;}");
          client.println("</style>");
          
          client.println("</head>");

          client.println("<body>");
            client.println("<h1>");
            client.println("Smart Thermo");
            client.println(newtime);
            client.println("</h1>");
            client.println("<div><span id=\"temp_setting\">");
            client.print("Current temperature setting: ");
            switch(tsetting) {
              case 0: client.println("Cold"); break;
              case 1: client.println("1"); break;
              case 2: client.println("2"); break;
              case 3: client.println("3"); break;
              case 4: client.println("4"); break;
              case 5: client.println("5"); break;
              case 6: client.println("6"); break;
              default: client.println("Your heating system might be broken");
}
            client.println("</div></span>");///////////////////////////////////////////////////

            client.println("<button onclick=\"turnLedUp()\">Next Setting</button> </div>");
            client.println("<button onclick=\"turnLedDown()\">Prev Setting</button> </div>");
            client.println("<br />");
          // output the value of temperature////////////////////
            client.print("Farenhiet = ");
            client.println(f_buffer);
            client.print("Celcius = ");
            client.println(c_buffer);
            client.println("<br />");

              /////JS
              /// Refresh script
            client.println("<script>window.setInterval(function(){");
            client.println("nocache = \"&nocache=\" + Math.random() * 10;");
            client.println("var request = new XMLHttpRequest();");
            client.println("request.onreadystatechange = function() {");
            client.println("if (this.readyState == 4) {");
            client.println("if (this.status == 200) {");
            client.println("if (this.responseText != null) {");
            client.println("console.log(this.reesponseText)");
            client.println("document.getElementById(\"analoge_data\").innerHTML = this.responseText;");
            client.println("}}}}");
            client.println("}, 1000);");
            ////// Function to go setting up
            client.println("function turnLedUp() {");
            client.println("nocache = \"&nocache=\" + Math.random() * 10;");
            client.println("var request = new XMLHttpRequest();");
            client.println("request.onreadystatechange = function() {");
            client.println("if (this.readyState == 4) {");
            client.println("if (this.status == 200) {");
            client.println("if (this.responseText != null) {");
            client.println("console.log(this.reesponseText)");///// DEBUG
            client.println("document.getElementById(\"temp_setting\").innerHTML = this.responseText;");
            client.println("}}}}");
            client.println("request.open(\"GET\", \"?settingUp=1\" + nocache, true);");
            client.println("request.send(null);");
            client.println("}");
           ///// Function to go setting down
            client.println("function turnLedDown() {");
            client.println("event.preventDefault();");
            client.println("nocache = \"&nocache=\" + Math.random() * 10;");
            client.println("var request = new XMLHttpRequest();");
            client.println("request.onreadystatechange = function() {");
            client.println("if (this.readyState == 4) {");
            client.println("if (this.status == 200) {");
            client.println("if (this.responseText != null) {");
            client.println("console.log(this.reesponseText)");///// DEBUG
            client.println("document.getElementById(\"temp_setting\").innerHTML = this.responseText;");
            client.println("}}}}");
            client.println("request.open(\"GET\", \"?settingDown=1\" + nocache, true);");

            client.println("request.send(null);");
            client.println("}");
            client.println("</script>");

            client.println("</body>");
            client.println("</html>");
            break;
          }
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    HTTP_req ="";
    Serial.println("client disconnected");
  }
}