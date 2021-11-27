#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <EthernetUdp.h>
#include <list>
#include <iostream>
using namespace std;

/// Temperature sampling and control variables
#define DS1621_ADDRESS 0x48

/// Temperature pins
int upTemp = A5;
int downTemp = A4;
/// Temperature settings
enum TemperatureSetting { Cold, Set1, Set2, Set3, Set4, Set5, Set6};
TemperatureSetting tsetting = Cold;

float targetTemperature = 21.00;

const char* PARAM_INPUT_1 = "input1";



/// timeSchedule class
class TimeSchedule{
  public:
    String name;
    int fromTime;
    int toTime;
    TemperatureSetting setting;

    TimeSchedule(String name, int fromTime,int toTime, TemperatureSetting setting){
        this->name = name;
        this->fromTime = fromTime;
        this->toTime = toTime;
        this->setting = setting;
    }
};
/// collection of time schedules
list<TimeSchedule> schedules;


const char timeServer[] = "time.nist.gov"; // time.nist.gov NTP server
unsigned int localPort = 8888;
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
String newtime = ""; //
String nyTid = ""; //
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

//IPAddress ip(192, 168, 1, 177); // IP Adresse for webserveren.
IPAddress ip(192, 168, 0, 177); // IP Adresse for webserveren.

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

/// createSchedule function
void createSchedule(String _name, int _fromTime, int _toTime,TemperatureSetting _setting){
  TimeSchedule nyt(_name, _fromTime,_toTime,_setting);
  schedules.push_back(nyt);
}
/// Factory presets for time schedules
void factorySettings(){
  TimeSchedule spareTidskema("spar", 800, 1400, Cold);
  TimeSchedule comfortTidskema("comfort", 1400, 2300, Set3);
  TimeSchedule nightTidskema("night", 2300, 700, Set2);

    schedules.push_back(spareTidskema);
    schedules.push_back(comfortTidskema);
    schedules.push_back(nightTidskema);
}

/// settingUp function
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
/// settingDown function
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
void editTime(EthernetClient client)//////////////Giver nyTid værdien fra HTTP.req og returnerer den redigeret tid. 
{
  nyTid = HTTP_req.substring(HTTP_req.indexOf("=")+1, HTTP_req.indexOf("=")+5);
  //nyTid = String(HTTP_req.indexOf("input1"));
  Serial.println("------------------------" + nyTid); //Debugging for nyTid.
}

void turnTemperatureUp(EthernetClient client)////////////// sætter tsetting ++/-- og returnerer den ny setting
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
void turnTemperatureDown(EthernetClient client)
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

// send an NTP request to the time server at the given address
void sendNTPpacket(const char * address) {////////////// Sender en NTP request til en server(time.nist.gov)
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
  while (!Serial) {//Vent på Serial port forbinder.
    ; // wait for serial port to connect. Needed for native USB port only
  }

  /// start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);

  /// Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    while (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
      /// User gets a chance to plug a cable in
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
     break;
    }
    

  }
   /// start the Ethernet connection and the server:
  Ethernet.begin(mac, ip); //Start Ethernet, ved hjælp af mac og ip adresse.
  
  if (Ethernet.linkStatus() == LinkOFF) { //Hvis Ethernet kablet ikke kunne findes, så giver den besked over Serial monitoren. 
    Serial.println("Ethernet cable is not connected.");
  }
  Udp.begin(localPort);
  ///start the server
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP()); //////Debug
  /// Load factory settings
  factorySettings();
}


void loop() {
  ////Temperature reading
  delay(1000);

  // get temperature in tenths °C
  int16_t c_temp = get_temperature();
  // convert tenths °C to tenths °F
  int16_t f_temp = c_temp * 9/5 + 320 ;

    if(c_temp < 230) {   // Hvis temperaturen < 23 °C
    c_temp = abs(c_temp);  // absolute value af c_temp
    sprintf(c_buffer, "-%02u.%1u%cC", c_temp / 10, c_temp % 10, 223);
    //digitalWrite(20,LOW);
  }
  else {
    if (c_temp >= 250)    // Hvis temperaturen >= 100.0 °C
    {
      sprintf(c_buffer, "%03u.%1u%cC", c_temp / 10, c_temp % 10, 223);
      //digitalWrite(20, HIGH);
    }
    else
    {
      sprintf(c_buffer, " %02u.%1u%cC", c_temp / 10, c_temp % 10, 223); //sprintf = String print format
      
    }
  }
    if(f_temp < 0) {   // Hvis temperaturen < 0 °F
    f_temp = abs(f_temp);  // absolute value
    sprintf(f_buffer, "-%02u.%1u%cF", f_temp / 10, f_temp % 10, 223);
    digitalWrite(20, HIGH);
  }
  else {
    if (f_temp >= 100) // Hvis temperaturen >= 100.0 °F
    {
      sprintf(f_buffer, "%03u.%1u%cF", f_temp / 10, f_temp % 10, 223);
      digitalWrite(20, LOW);
    }
    else
    {
      sprintf(f_buffer, " %02u.%1u%cF", f_temp / 10, f_temp % 10, 223);
    }
  }

  Serial.println(c_buffer); //Udskriver c_buffer i serial monitoren (Temperatur i °C)
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
  
    Serial.print((epoch  % 86400L) / 3600); // Udskriver Timerne (86400 er lig med sekunder per dag)
    newtime = "";
    newtime = (epoch  % 86400L) / 3600;//Tilføj timer til newtime
    newtime += ":";//Tilføjer semikolon til newtime, sammen med timerne tilføjet fra sidste linje.
    Serial.print(':');
    if (((epoch % 3600) / 60) < 10) { // Udskriver Minutterne, hvis minutterne er under 10 (værdien checkes om den er under 10)
      newtime += "0";// For de første 10 minutter af hver time, så vil den starte ud med et 0 før hvert tal.
      Serial.print('0');
    }
    newtime += (epoch  % 3600) / 60;
    Serial.print((epoch  % 3600) / 60); // Udskriver Minutterne (3600 er lig med sekunder per minut)

    newtime += ":";
  
    if ((epoch % 60) < 10) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      // For de første 10 sekunder af hver minut, så vil vi starte ud med et 0 før hvert tal.
      Serial.print('0');
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
           HTTP_req += c; // Gemmer HTTP request´en en char ad gangen. 
           Serial.write(c); 

        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {         
          client.println("HTTP/1.1 200 OK"); // Sender en standard http response header.
          client.println("Content-Type: text/html"); // Sætter typen for hjemmesiden.
          client.println("Connection: close"); // Forbindelsen vil blive lukket efter at responsen er fuldendt.
          client.println("Refresh: 5"); // Genstarter siden automatisk efter 5 sekunder.
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
          else if (HTTP_req.indexOf("Spartid") >= 0 ) {
            //String nyTid = HTTP_req.editTime;
            editTime(client);
            break;
          }
          else{
          //HTML Sektion
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

            switch(tsetting) { //Sætter den nuværende status for temperaturen ved hjælp af switch case.
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

            client.println();
            client.print("<h1>Spare tid</h1>");
            client.print("<form action=\"/get\">");
            client.print("<input type=\"text\" id=\"Spartid\" name=\"input1\">");
            client.print("<button onclick=\"editTime()\">Skift tid</button>");
            client.print("</form>");

            client.print("<div>");
            client.print(nyTid);
            client.print("</div>");
            //// Tidsskemaer
            client.print("<div>");
            client.print("<ul>");
              for(list<TimeSchedule>::iterator i = schedules.begin(); i != schedules.end(); i++){
              client.print("<li>");
              client.print(i->name);
              client.print(" ");
              client.print(i->fromTime);
              client.print(" ");
              client.print(i->toTime);
              client.print(" ");
              client.print(i->setting);
              client.print("</li>");
              }

            client.print("</ul>");
            client.print("</div>");

            
            /////JavaScript Sektion
            /// Refresh script
            client.println("<script>window.setInterval(function(){");//Scriptet køres når siden starter op.

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
            ///// Funktion for Change Time
            client.println("function editTime() {");//Funktionen navngives.
            client.println("event.preventDefault();");//Stopper siden fra at gå videre til andre sider.
            client.println("nocache = \"&nocache=\" + Math.random() * 10;");
            client.println("var request = new XMLHttpRequest();");
            client.println("request.onreadystatechange = function() {");
            client.println("if (this.readyState == 4) {");
            client.println("if (this.status == 200) {");
            client.println("if (this.responseText != null) {");
            client.println("console.log(this.reesponseText)");///// DEBUG
            client.println("document.getElementById(\"Spartid\").innerHTML = this.responseText;");//
            client.println("}}}}");
            client.println("request.open(\"GET\", \"?Spartid=\"+inputText + nocache, true);");
            client.println("request.send(null);");
            client.println("}");
            client.println("var inputText = document.getElementById(\"Spartid\").innerHTML;");
            client.println("</script>");

            client.println("</body>");
            client.println("</html>");
            break;
          }
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true; // Du starter en ny linje
        } else if (c != '\r') {      
          // you've gotten a character on the current line
          currentLineIsBlank = false; // du har en karakter på den nuværende linje.
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop(); //Forbindelsen sluttes:

    HTTP_req =""; //HTTP requsten nulstilles.
    Serial.println("client disconnected");
  }
}