// #include "AirQuality.h"
#include "TH02_dev.h"
#include "Arduino.h"
#include "Wire.h"
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include <avr/wdt.h>
#include <stdlib.h>


#define ADAFRUIT_CC3000_IRQ   3
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
#define WLAN_SSID       "Project RHINO"
#define WLAN_PASS       "kingandbathurst"
#define WLAN_SECURITY   WLAN_SEC_WPA2


Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT, SPI_CLOCK_DIVIDER);
// AirQuality airqualitysensor;


// int current_quality = -1;
int buffer_size = 20;

uint32_t server_ip = 0;

const char endpoint[] = "/entries";
const char user_agent[] = "Arduino homemonitor 1.0";

const char server_host[] = "homemonitor.hipolabs.com";
const unsigned long connectTimeout = 15L * 1000L;


// Similar to F(), but for PROGMEM string pointers rather than literals
#define F2(progmem_ptr) (const __FlashStringHelper *)progmem_ptr


void setup() {

  Serial.begin(115200);

  Serial.println(F("\nInitialising the CC3000"));
  
  if (!cc3000.begin()) {
    Serial.println(F("Unable to initialise the CC3000! Check your wiring?"));
    while(1);
  }

  Serial.println(F("\nDeleting old connection profiles"));

  if (!cc3000.deleteProfiles()) {
    Serial.println(F("Failed!"));
    while(1);
  }

  /* Attempt to connect to an access point */
  Serial.print(F("\nAttempting to connect to WLAN: "));

  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
  
  Serial.println(F("Connected!"));
  
  /* Wait for DHCP to complete */
  Serial.println(F("\nRequest DHCP"));

  while (!cc3000.checkDHCP()) {
    delay(100);
  }

  Serial.println(F("\nGetting IP"));

  /* Display the IP address DNS, Gateway, etc. */  
  while (!displayConnectionDetails()) {
    delay(1000);
  }

  Serial.println(F("\nResolving host"));

  // Get IP
  while (server_ip == 0) {
    if (!cc3000.getHostByName((char *)server_host, &server_ip)) {
      Serial.println(F("Couldn't resolve!"));
      while(1){}
    }

    delay(500);
  }
  
  cc3000.printIPdotsRev(server_ip);



  // Ping server
  Serial.print(F("\nPinging server: "));

  uint8_t ping_replies = cc3000.ping(server_ip, 3);

  if (ping_replies == 0) {
    Serial.println(F("Failed"));
    while(1){}
  }

  Serial.println(F("Ping success!"));

  
  
  Serial.println(F("\n"));

  delay(100);

  TH02.begin();

  // delay(100);

  // airqualitysensor.init(14);

}


/**************************************************************************/
/*!
    @brief  Tries to read the IP address and other connection details
*/
/**************************************************************************/
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}


boolean logData(float temperature, float humidity, long uv_exposure, int luminance, int loudness) {

  Serial.println(F("\nLogging data"));

  // Enable watchdog
  wdt_enable(WDTO_8S);

  char temp[10];
  char humid[10];
  String payload = "";

  dtostrf(temperature, 1, 2, temp);
  dtostrf(humidity, 1, 2, humid);

  payload = payload + "\n" + "{\"temperature\": \"" + temp + "\", " + 
                              "\"humidity\": \"" + humid + "\", " + 
                              "\"uv\": \"" + String(uv_exposure) + "\", " + 
                              "\"luminance\": \"" + String(luminance) + "\", " + 
                              "\"loudness\": \"" + String(loudness) + "\"}";

  Serial.println(F("\nPayload ready: "));
  Serial.println(payload);

  // Reset watchdog
  wdt_reset();

  // Check connection to WiFi
  Serial.print(F("\nChecking WiFi connection: "));

  if (!cc3000.checkConnected()) {
    while(1){}
  }

  Serial.println(F("Connected"));

  // Reset watchdog
  wdt_reset();
  
  // Ping server
  Serial.print(F("\nPinging server: "));

  uint8_t ping_replies = cc3000.ping(server_ip, 3);

  if (ping_replies == 0) {
    Serial.println(F("Failed"));
    while(1){}
  }

  Serial.println(F("Ping success!"));
  
  // Reset watchdog
  wdt_reset();

  Adafruit_CC3000_Client client = cc3000.connectTCP(server_ip, 80);

  if (!client.connected()) {
    Serial.println(F("Not connected"));
    // Reset watchdog & disable
    wdt_reset();
    wdt_disable();
  
    return false;
  }

  /*
  POST /entries HTTP/1.1
  Content-Type: application/json
  Host: homemonitor.hipolabs.com
  Connection: close
  User-Agent: Arduino Homemonitor 1.0
  Content-Length: 52

  {
    "uv": 102,
    "pressure": 151,
    "temperature": 22
  }
  */

  Serial.println(F("\nCreating request"));
    
  client.fastrprint(F("POST "));
  client.fastrprint(endpoint);
  client.fastrprintln(F(" HTTP/1.1"));
  client.fastrprintln(F("Content-Type: application/json"));
  client.fastrprint(F("Host: "));
  client.fastrprintln(server_host);
  client.fastrprint(F("User-Agent: "));
  client.fastrprintln(user_agent);
  client.fastrprint(F("Content-Length: "));
  client.println(payload.length());
  client.fastrprint(F("Connection: close"));

  // Reset watchdog
  wdt_reset();
  
  // Send data
  Serial.println(F("\nSending request"));  

  client.fastrprintln(F(""));
  sendData(client, payload, buffer_size);
  client.fastrprintln(F(""));

  // Reset watchdog
  wdt_reset();

  Serial.println(F("Reading answer"));

  while (client.connected()) {
    while (client.available()) {
      char c = client.read();
      Serial.print(c);
    }
  }

  // Reset watchdog
  wdt_reset();

   
  // Close connection and disconnect
  client.close();
  Serial.println(F("Closing connection"));
  
  // Reset watchdog & disable
  wdt_reset();
  wdt_disable();
  
}

void loop() {
  
  Serial.println(F("\nCollecting stats"));

  /* Temperature & Humidity */
  float temperature = TH02.ReadTemperature(); 
  float humidity = TH02.ReadHumidity();
  
  delay(20);

  /* UV */
  int sensorValue;
  long uv_exposure = 0;
  
  for (int i = 0; i < 1024; i++) {
    sensorValue = analogRead(A2);
    uv_exposure = sensorValue + uv_exposure;

    delay(2);
  }   
  
  uv_exposure = uv_exposure >> 10;
  uv_exposure = uv_exposure * 4980.0 / 1023.0;

  delay(20);

  /* Luminance & Loudness */
  int luminance = analogRead(A3);
  int loudness = analogRead(1);

  delay(20);

  // current_quality = airqualitysensor.slope();

  // delay(20);

  Serial.println(F("\nCollection done"));

  logData(temperature, humidity, uv_exposure, luminance, loudness);
  
  // Wait 10 seconds until next update
  wait(10000);
}

// Air quality loop
// ISR(TIMER2_OVF_vect) {
  
//   if (airqualitysensor.counter == 122) {
//       airqualitysensor.last_vol = airqualitysensor.first_vol;
//       airqualitysensor.first_vol = analogRead(A0);
//       airqualitysensor.counter = 0;
//       airqualitysensor.timer_index = 1;
//       PORTB=PORTB^0x20;
//   } else {
//     airqualitysensor.counter++;
//   }
// }

// Send data chunk by chunk
void sendData(Adafruit_CC3000_Client& client, String input, int chunkSize) {
  
  // Get String length
  int length = input.length();
  int max_iteration = (int)(length / chunkSize);
  
  for (int i = 0; i < length; i++) {
    client.print(input.substring(i * chunkSize, (i + 1) * chunkSize));

    wdt_reset();
  }  
}

// Wait for a given time using the watchdog
void wait(int total_delay) {
  
  int number_steps = (int)(total_delay / 5000);

  wdt_enable(WDTO_8S);

  for (int i = 0; i < number_steps; i++) {
    delay(5000);
    wdt_reset();
  }

  wdt_disable();
}
