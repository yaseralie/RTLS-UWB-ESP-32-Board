#include <SPI.h>
#include <DW1000Ranging.h>
#include "link.h"

//Library for MQTT:
#include <WiFi.h>
#include <PubSubClient.h>

#define SPI_SCK 18
#define SPI_MISO 19
#define SPI_MOSI 23
#define DW_CS 4

// connection pins
const uint8_t PIN_RST = 27; // reset pin
const uint8_t PIN_IRQ = 34; // irq pin
const uint8_t PIN_SS = 4;   // spi select pin

// TAG antenna delay defaults to 16384
// leftmost two bytes below will become the "short address"
char tag_addr[] = "7D:00:22:EA:82:60:3B:9C";

const char* ssid = "YOUR WIFI SSID";
const char* password = "PASSWORD";
const char* mqtt_server = "123.456.789.101";

//WiFiClient client;
WiFiClient espClient;
PubSubClient client(espClient);

//declare topic for publish message
const char* msg_topic = "UWB_Tag";

struct MyLink *uwb_data;
int index_num = 0;
long runtime = 0;
String all_json = "";

//function for set up wifi===========================================
void setup_wifi() {
  delay(100);
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length)
{
} //end callback


//function for MQTT connection=======================================
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    //if you MQTT broker has clientID,username and password
    //please change following line to    if (client.connect(clientId,userName,passWord))
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
      //once connected to MQTT broker, subscribe command if any
      client.subscribe("test");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
} //end reconnect()

void setup()
{
  Serial.begin(115200);
  delay(1000);
  setup_wifi();

  //MQTT setup
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  //init the configuration
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ); //Reset, CS, IRQ pin
  
  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.attachNewDevice(newDevice);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);

  //enable blinking
  //DW1000.enableDebounceClock();
  //DW1000.enableLedBlinking();
  //DW1000.setGPIOMode(MSGP0, LED_MODE);

  //we start the module as a tag
  DW1000Ranging.startAsTag(tag_addr, DW1000.MODE_LONGDATA_RANGE_LOWPOWER, false);

  uwb_data = init_link();
}

void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  DW1000Ranging.loop();
  if ((millis() - runtime) > 1000)
  {
    make_link_json(uwb_data, &all_json);
    Serial.println(all_json);
    client.publish(msg_topic,all_json.c_str());
    runtime = millis();
  }
}

void newRange()
{
  char c[30];

  Serial.print("from: ");
  Serial.print(DW1000Ranging.getDistantDevice()->getShortAddress(), HEX);
  Serial.print("\t Range: ");
  Serial.print(DW1000Ranging.getDistantDevice()->getRange());
  Serial.print(" m");
  Serial.print("\t RX power: ");
  Serial.print(DW1000Ranging.getDistantDevice()->getRXPower());
  Serial.println(" dBm");
  fresh_link(uwb_data, DW1000Ranging.getDistantDevice()->getShortAddress(), DW1000Ranging.getDistantDevice()->getRange(), DW1000Ranging.getDistantDevice()->getRXPower());
}

void newDevice(DW1000Device *device)
{
  Serial.print("ranging init; 1 device added ! -> ");
  Serial.print(" short:");
  Serial.println(device->getShortAddress(), HEX);

  add_link(uwb_data, device->getShortAddress());
}

void inactiveDevice(DW1000Device *device)
{
  Serial.print("delete inactive device: ");
  Serial.println(device->getShortAddress(), HEX);

  delete_link(uwb_data, device->getShortAddress());
}
