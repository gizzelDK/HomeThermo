#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <EthernetUdp.h>

/// Temperature sampling and control variables
#define DS1621_ADDRESS 0x48

//int upTemp = A4;
int RED = A4;
int downTemp = A5;
const char timeServer[] = "time.nist.gov"; // time.nist.gov NTP server
unsigned int localPort = 8888;
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

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
  raw_t = raw_t * 10 / 2;
  return raw_t;
}
/// Server variables
// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 177);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

void turnTemperatureUp() {
  digitalWrite(RED, HIGH);
  delay(1000);
  digitalWrite(RED, LOW);
  Serial.println("Turned heat up");
}
void turnTemperatureDown() {
  digitalWrite(downTemp, HIGH);
  delay(1000);
  digitalWrite(downTemp, LOW);
  Serial.println("Turned heat down");
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

char c_buffer[8], f_buffer[8]; // Char arrays til temperatur bytes

/// Get ledchange handling
void ledChangeStatus(EthernetClient client)
{
  int state = digitalRead(RED);
  Serial.println(state);
  if (state == 1) {
    digitalWrite(RED, LOW);
    client.print("OFF");
  }
  else {
    digitalWrite(RED, HIGH);
    client.print("ON");
  }
}
void ajaxRequest(EthernetClient client)
{
  for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
    int sensorReading = analogRead(analogChannel);
    client.print("analog input ");
    client.print(analogChannel);
    client.print(" is ");
    client.print(sensorReading);
    client.println("<br />");
  }
}
void setup() {    
  Wire.begin();
  Wire.beginTransmission(DS1621_ADDRESS);
  Wire.write(0xAC);
  Wire.write(0);
  Wire.beginTransmission(DS1621_ADDRESS);
  Wire.write(0xEE);
  Wire.endTransmission();
  pinMode(20, OUTPUT);

  pinMode(RED, OUTPUT);
  pinMode(downTemp, OUTPUT);
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
    Ethernet.begin(mac, ip);
  Serial.println("Ethernet WebServer Example");

  // start the Ethernet connection and the server:
  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }

  Udp.begin(localPort);
  // start the server
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
}


void loop() {
  ////Temperature reading
  delay(1000);    // wait a second
  // get temperature in tenths °C
  int16_t c_temp = get_temperature();
  // convert tenths °C to tenths °F
  int16_t f_temp = c_temp * 9 / 5 + 320 ;

  if (c_temp < 230) {  // if temperature < 23 °C
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
  if (f_temp < 0) {  // if temperature < 0 °F
    f_temp = abs(f_temp);  // absolute value
    sprintf(f_buffer, "-%02u.%1u%cF", f_temp / 10, f_temp % 10, 223);
    digitalWrite(20, HIGH);
  }
  else {
    if (f_temp >= 100)
    { // if temperature >= 100.0 °F
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

  // listen for incoming clients
  EthernetClient client = server.available();
  
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
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
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");

          client.println("<head>");
          client.println("<script type=\"text/javascript\">");///////// Action goees here


          client.println("function turnTemperatureUp(){console.log(true)}");///////// Action goees here

          client.println("</script>");///////// Action goees here

          client.println("<style>");///////// Styling goees here
          client.print("*{background-color:#000;color:hotpink;}h1{color:#00F}");
          client.println("</style>");

          client.println("</head>");

          client.println("<body>");
          client.println("<h1>");
          client.println("Gyygle Thermo");
          client.println("</h1>");

          client.println("<div><span id=\"led_status\">");
          if (digitalRead(RED) == 1)
            client.println("On");
          else
            client.println("Off");
          client.println("</span> | <button onclick=\"changeLEDStatus()\">Change Status</button> </div>");

          client.println("<button onclick=\"turnTemperatureUp()\">");// Inline script with ajax call?
          client.println("Turn up the Heat");
          client.println("</button>");
          client.println("<button onclick=\"turnTemperatureDown\">");// Inline script with ajax call?
          client.println("Turn down the Heat");
          client.println("</button>");
          client.println("<br />");
          // output the value of each analog input pin////////////////////
          client.print("Farenhiet = ");
          client.println(f_buffer);
          client.print("Celcius = ");
          client.println(c_buffer);
          client.println("<br />");

          /////JS
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
          client.println("request.open(\"GET\", \"ajaxrefresh\" + nocache, true);");
          client.println("request.send(null);");
          client.println("}, 5000);");
          client.println("function changeLEDStatus() {");
          client.println("nocache = \"&nocache=\" + Math.random() * 10;");
          client.println("var request = new XMLHttpRequest();");
          client.println("request.onreadystatechange = function() {");
          client.println("if (this.readyState == 4) {");
          client.println("if (this.status == 200) {");
          client.println("if (this.responseText != null) {");
          client.println("console.log(this.reesponseText)");///// DEBUG
          client.println("document.getElementById(\"led_status\").innerHTML = this.responseText;");
          client.println("}}}}");
          client.println("request.open(\"GET\", \"?ledstatus=1\" + nocache, true);");
          client.println("request.send(null);");
          client.println("}");
          client.println("</script>");

          client.println("</body>");
          client.println("</html>");
          break;
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
    Serial.println("client disconnected");
  }
}