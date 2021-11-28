#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>
#include <EthernetUdp.h>
#include <list>
#include <iostream>
using namespace std;

/// Temperatur sampling og control variabler
#define DS1621_ADDRESS 0x48
/// Temperature pins
int upTemp = A5;
int downTemp = A4;
/// Temperature settings
enum TemperatureSetting { Cold, Set1, Set2, Set3, Set4, Set5, Set6};
TemperatureSetting tsetting = Cold;


float targetTemperature = 21.00;
int chosenProgramme = 0;
bool userOverride = false;

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
/// en liste af TimeSchedule kaldet schedules
list<TimeSchedule> schedules;

const char timeServer[] = "time.nist.gov"; /// time.nist.gov NTP server
unsigned int localPort = 8888;
const int NTP_PACKET_SIZE = 48; /// NTP time stamp er inde i de første 48 bytes af beskeden.
String newtime = ""; /// newtime instalizeres
String nyTid = ""; /// nyTid instalizeres
byte packetBuffer[NTP_PACKET_SIZE]; ///buffer for at holde de indkommene og udgående pakker.

EthernetUDP Udp; /// EN UDP instance som lader programmet sende og modtage pakker over UDP.

String HTTP_req; /// Http req holder


char c_buffer[8], f_buffer[8]; /// Char arrays til temperatur bytes    

int16_t get_temperature() {
  Wire.beginTransmission(DS1621_ADDRESS); /// Forbind til DS1621 (Send en DS1621 adresse)
  Wire.write(0xAA);                       /// Læs temperatur kommando
  Wire.endTransmission(false);            /// Send en gentaget start condition.
  Wire.requestFrom(DS1621_ADDRESS, 2);    /// Request 2 bytes fra DS1621 og release I2C bus ved slutningen af læsningen.
  uint8_t t_msb = Wire.read();            /// Læs temperatur med MSB register.
  uint8_t t_lsb = Wire.read();            /// Læs temperatur med LSB register.
 
  /// Udregner den fulde temperatur (i rå værdi).
  int16_t raw_t = (int8_t)t_msb << 1 | t_lsb >> 7;
  /// Konverter den rå temperatur til en tiendedele °C
  raw_t = raw_t * 10/2;
  return raw_t;
}
/// Server variables
/// Indsæt en mac adresse og en ip adresse for controller´en nedenunder. 
/// Ip adressen vil være afhængig af ens lokale netwærk:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

IPAddress ip(192, 168, 1, 177); /// IP Adresse for webserveren.

/// Initializer Ethernet server library
/// Med den IP address og port som du vil bruge
/// (port 80 er default porten for HTTP):
EthernetServer server(80);

/// createSchedule funktion
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
/// Checker tid fra schedules vælger den rigtige "setting" hvis den passer med tiden af dagen.
void runCurrentSchedule(String time){
  String tempString = "";
  int t4digit[4];
  /// Henter tiden (time og minutter)
    for (unsigned int i = 0; i <=4; i++){

      Serial.print(time[i]);
      if (time[i] != ':')
      {
        tempString += time[i];
      }
    }
  
    int t = tempString.toInt();
    ///Setting vælges udfra hvilken tid det er på dagen.
    for (list<TimeSchedule>::iterator j = schedules.begin(); j != schedules.end(); j++)
    {
      if (t < 1200 && j->toTime < 1200)
      {
         tsetting = j->setting;
      }
      if(j->fromTime >= t && j->toTime < t){
            tsetting = j->setting;

       }
    }

}
/// settingUp funktion
void settingUp(EthernetClient client)
{
  userOverride = true;
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
/// settingDown funktionen
void settingDown(EthernetClient client)
{
    userOverride = true;
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
/// Sætter targetTemperature for tsetting value og skruer varmen op eller ned
void adjustTemperature(int m16)
{
   switch(tsetting) {
              case 0: targetTemperature = 0.00; break;
              case 1: targetTemperature = 6.00; break;
              case 2: targetTemperature = 10.00; break;
              case 3: targetTemperature = 15.00; break;
              case 4: targetTemperature = 21.00; break;
              case 5: targetTemperature = 26.00; break;
              case 6: targetTemperature = 30.00; break;
              default: targetTemperature = 21.00;
  }
 float measured = m16 / 10.00;
 Serial.println("------------------");
 Serial.println(targetTemperature);
 Serial.println("------------------");
  /// doSomething = target - measure
  float doSomething = measured - targetTemperature;
  /// state= decides if heat open/close signal must be send
  int state = 0;
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
/// Giver nyTid værdien fra HTTP.req og returnerer den redigeret tid. 
void editTime(EthernetClient client)
{
  nyTid = HTTP_req.substring(HTTP_req.indexOf("=")+1, HTTP_req.indexOf("=")+5);
  Serial.println("------------------------" + nyTid); /// Debugging for nyTid.
}
/// sætter tsetting ++/-- og returnerer den ny setting
void turnTemperatureUp(EthernetClient client)
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

/// sender en NTP request til tids serveren ved hjælp af dens adresse.
void sendNTPpacket(const char * address) {////////////// Sender en NTP request til en server(time.nist.gov)
  /// set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  /// Initializer værdier som slaæ bruges til at forme NTP requesten.
  /// (se URL ovenover for detaljer for pakken).
  packetBuffer[0] = 0b11100011;   /// LI, Version, Mode
  packetBuffer[1] = 0;     /// Stratum, eller type of clock
  packetBuffer[2] = 6;     /// Polling Interval
  packetBuffer[3] = 0xEC;  /// Peer Clock Precision
  /// 8 bytes of zero for Root Delay og Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  /// alle NTP fields er blevet givet en værdi, nu.
  /// du kan sende en pakke som spørg efter en timestamp
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
  /// Åben serial kommunikationen og vent for porten til at åbne for debugging formål:
  Serial.begin(115200);
  while (!Serial) {/// Vent på Serial port forbindes. Den skal kun bruge en native USB port. 
    ; 
  }

  /// starter Ethernet forbindelsen og serveren:
  Ethernet.begin(mac, ip);

  /// Check om Ethernet hardware er forbundet.
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    while (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
      /// Brugeren får chancen for at indsætte sit kabel.
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
     break;
    }
  }
   /// starter Ethernet forbindelsen og serveren:
  Ethernet.begin(mac, ip); //Start Ethernet, ved hjælp af mac og ip adresse.
  
  if (Ethernet.linkStatus() == LinkOFF) { //Hvis Ethernet kablet ikke kunne findes, så giver den besked over Serial monitoren. 
    Serial.println("Ethernet cable is not connected.");
  }
  Udp.begin(localPort);
  /// Start serveren
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP()); //////Debug
  /// Loader factory settings
  factorySettings();
}


void loop() {
  ////Temperaturen læses
  delay(1000);

  int16_t c_temp = get_temperature(); ///Henter temperatur i tiendedele °C

  int16_t f_temp = c_temp * 9/5 + 320 ; /// Konverter en tiendedele °C til en tiendedele °F

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
    /// timestamp
    // or two words, long. First, extract the two words:
    ///
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    /// kombinere de fire bytes (to ord) til en long integer
    ///Dette er NTP tid (sekunder siden Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    /// Nu konverter NTP tiden indtil hverdags tid
    ///Unix time starter i Jan 1 1970. i sekunder, hvilket svare til 2208988800: 
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print the hour, minute and second:
    
    newtime = "";
    if ((epoch  % 86400L) / 3600) {
      // In the first 10 hours, we'll want a leading '0'
      newtime = "0";
    }
    newtime += (epoch  % 86400L) / 3600;
    newtime += ":";
  
    if (((epoch % 3600) / 60) < 10) {
      newtime += "0";  /// For de første 10 minutter af hver time, så vil den starte ud med et 0 før hvert tal.
    }
    newtime += (epoch  % 3600) / 60;
    newtime += ":"; /// Tilføjer semikolon til newtime, sammen med timerne tilføjet fra sidste linje.
    if ((epoch % 60) < 10) {
      newtime += "0"; /// For de første 10 sekunder af hver minut, så vil den starte ud med et 0 før hvert tal.
    }
    newtime += epoch % 60;
      /// Checker schedules for om programmet skal køres
      if(!userOverride){
        runCurrentSchedule(newtime);
      }
  Serial.println("--------**********");
  Serial.println(newtime);
  Serial.println("--------************");
  }
  Ethernet.maintain();
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
         if( HTTP_req.length() < 120) 
           HTTP_req += c; /// Gemmer HTTP request´en en char ad gangen. 
           Serial.write(c); 

        /// hvis du er nået til slutningen af linjen(modtaget en ny linje
        /// karakter) og linjen er blank, så er http requesten sluttet 
        /// Så kan du sende et svar
        if (c == '\n' && currentLineIsBlank) {         
          client.println("HTTP/1.1 200 OK"); /// Sender en standard http response header.
          client.println("Content-Type: text/html"); /// Sætter typen for hjemmesiden.
          client.println("Connection: close"); /// Forbindelsen vil blive lukket efter at responsen er fuldendt.
          client.println("Refresh: 5"); /// Genstarter siden automatisk efter 5 sekunder.
          client.println();

          Serial.println(HTTP_req);
           if (HTTP_req.indexOf("settingUp") >= 0 ) {
            /// Læser swtich stat og analog inputet
            settingUp(client);
            break;
          }else if (HTTP_req.indexOf("settingDown") >= 0 ) {
            /// Læser swtich stat og analog inputet
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

          client.println("<script type=\"text/javascript\">");///////// Js Action starter her

          client.println("</script>");///////// Js Action slutter her
           
          client.println("<style>");///////// Styling starter her
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

            switch(tsetting) { ///Checker tsetting værdi og sætter viser den overensstemme værdi(fra cold til 6) /// Sætter den nuværende status for temperaturen ved hjælp af switch case.
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
            client.println("<script>window.setInterval(function(){"); /// Scriptet køres når siden starter op.

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

            ////// Funktion for setting up

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

           ///// Funktion for setting down

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
            client.println("function editTime() {"); /// Funktionen navngives.
            client.println("event.preventDefault();"); /// Stopper siden fra at gå videre til andre sider.
            client.println("nocache = \"&nocache=\" + Math.random() * 10;");
            client.println("var request = new XMLHttpRequest();");
            client.println("request.onreadystatechange = function() {");
            client.println("if (this.readyState == 4) {");
            client.println("if (this.status == 200) {");
            client.println("if (this.responseText != null) {");
            client.println("console.log(this.reesponseText)");///// DEBUG
            client.println("document.getElementById(\"Spartid\").innerHTML = this.responseText;");
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
          currentLineIsBlank = true; /// Du starter en ny linje
        } else if (c != '\r') {      
          currentLineIsBlank = false; /// Du har en karakter på den nuværende linje.
        }
      }
    }
    
    delay(1); /// Giver webserveren tid til at modtage data´en.

    client.stop(); ///Forbindelsen sluttes:

    HTTP_req =""; ///HTTP requsten nulstilles.
    Serial.println("client disconnected");
  }
}