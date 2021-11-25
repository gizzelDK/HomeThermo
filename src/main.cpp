#include <SPI.h>
#include <Ethernet.h>
#include <Wire.h>

/// Temperature sampling and control variables
#define DS1621_ADDRESS 0x48

/// Temperature pins
int upTemp = A5;
int downTemp = A4;

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

/// Get ledchange handling
void turnTemperatureUp(EthernetClient client)
{
  int state = digitalRead(upTemp);
  Serial.println(state);
  if (state == 1) {
    digitalWrite(upTemp, LOW);
    client.print("OFF");
  }
  else {
    digitalWrite(downTemp, LOW);
    digitalWrite(upTemp, HIGH);
    client.print("Heating");
  }
}
void turnTemperatureDown(EthernetClient client)
{
  int state = digitalRead(downTemp);
  Serial.println(state);
  if (state == 1) {
    digitalWrite(downTemp, LOW);
    client.print("OFF");
  }
  else {
    digitalWrite(upTemp, LOW);
    digitalWrite(downTemp, HIGH);
    client.print("Cooling");
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

    pinMode(upTemp, OUTPUT); 
    pinMode(downTemp, OUTPUT); 
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("Ethernet WebServer Example");

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);

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
  
  //////// listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
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
           if (HTTP_req.indexOf("turnTemperatureUp") >= 0 ) {
            // read switch state and analog input
            turnTemperatureUp(client);
            break;
          }else if (HTTP_req.indexOf("turnTemperatureDown") >= 0 ) {
            // read switch state and analog input
            turnTemperatureDown(client);
            break;
          }
          else{

          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          
          client.println("<head>");
          client.println("<script type=\"text/javascript\">");///////// Action goees here
          
          client.println("function turnTemperatureUp(){console.log(true)}");///////// Action goees here
          client.println("function turnTemperatureDown(){console.log(true)}");///////// Action goees here
          
          
          client.println("</script>");///////// Action goees here
           
          client.println("<style>");///////// Styling goees here
          client.print("*{background-color:#000;color:hotpink;}h1{color:#00F}");
          client.println("</style>");
          
          client.println("</head>");

          client.println("<body>");
            client.println("<h1>");
            client.println("Smart Thermo");
            client.println("</h1>");

            client.println("<div><span id=\"up_Temp\">");
            if(digitalRead(upTemp) == 1)
             client.println("Heating");
            else
              client.println("Off");
            client.println("</span> | <button onclick=\"turnLedUp()\">Turn up temperature</button> </div>");
            client.println("<div><span id=\"down_Temp\">");

            if(digitalRead(downTemp) == 1){
             client.println("Cooling");
            } else{
              client.println("Off");
            }

            client.println("</span> | <button onclick=\"turnLedDown()\">Turn down temperature</button> </div>");

            client.println("<br />");
          // output the value of temperature////////////////////
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
            client.println("}, 1000);");
            ////// Function to toggle Heating
            client.println("function turnLedUp() {");
            client.println("nocache = \"&nocache=\" + Math.random() * 10;");
            client.println("var request = new XMLHttpRequest();");
            client.println("request.onreadystatechange = function() {");
            client.println("if (this.readyState == 4) {");
            client.println("if (this.status == 200) {");
            client.println("if (this.responseText != null) {");
            client.println("console.log(this.reesponseText)");///// DEBUG
            client.println("document.getElementById(\"up_Temp\").innerHTML = this.responseText;");
            client.println("}}}}");
            client.println("request.open(\"GET\", \"?turnTemperatureUp=1\" + nocache, true);");
            client.println("request.send(null);");
            client.println("}");
           ///// Function to toggle Cooling
            client.println("function turnLedDown() {");
            client.println("nocache = \"&nocache=\" + Math.random() * 10;");
            client.println("var request = new XMLHttpRequest();");
            client.println("request.onreadystatechange = function() {");
            client.println("if (this.readyState == 4) {");
            client.println("if (this.status == 200) {");
            client.println("if (this.responseText != null) {");
            client.println("console.log(this.reesponseText)");///// DEBUG
            client.println("document.getElementById(\"down_Temp\").innerHTML = this.responseText;");
            client.println("}}}}");
            client.println("request.open(\"GET\", \"?turnTemperatureDown=1\" + nocache, true);");
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
