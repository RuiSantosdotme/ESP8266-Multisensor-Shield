/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

// Load libraries
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Replace with your network credentials
const char* ssid     = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";

// Change the variable to your Raspberry Pi IP address, so it connects to your MQTT broker
const char* mqtt_server = "REPLACE_WITH_YOUR_MQTT_BROKER";
// MQTT Broker IP example
//const char* mqtt_server = "192.168.1.144";

// Initializes the espClient. You should change the espClient name if you have multiple ESPs running in your home automation system
WiFiClient espClient;
PubSubClient client(espClient);

// Variable to hold the temperature reading
String temperatureString = "";

// Set GPIOs for: output variable, status LED, PIR Motion Sensor, and LDR
const int output = 15;
const int statusLed = 12;
const int motionSensor = 5;
const int ldr = A0;
// Store the current output state
String outputState = "off";

// GPIO where the DS18B20 is connected to
const int oneWireBus = 4;          
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// Timers - Auxiliary variables
unsigned long now = millis();
unsigned long lastMeasure = 0;
boolean startTimer = false;
unsigned long currentTime = millis();
unsigned long previousTime = 0; 

// Don't change the function below. 
// This function connects your ESP8266 to your router
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected - ESP IP address: ");
  Serial.println(WiFi.localIP());
}

// This functions is executed when some device publishes a message to a topic that your ESP8266 is subscribed to
// Change the function below to add logic to your program, so when a device publishes a message to a topic that 
// your ESP8266 is subscribed you can actually do something
void callback(String topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT
  // If a message is received on the topic esp8266/output, you check if the message is either on or off. 
  // Turns the output according to the message received
  if(topic=="esp8266/output"){
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      digitalWrite(output, LOW);
      Serial.print("on");
    }
    else if(messageTemp == "off"){
      digitalWrite(output, HIGH);
      Serial.print("off");
    }
  }
  Serial.println();
}

// This functions reconnects your ESP8266 to your MQTT broker
// Change the function below if you want to subscribe to more topics with your ESP8266 
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");  
      // Subscribe or resubscribe to a topic
      // You can subscribe to more topics (to control more outputs)
      client.subscribe("esp8266/output");  
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// Checks if motion was detected and the sensors are armed. Then, starts a timer.
ICACHE_RAM_ATTR void detectsMovement() {
  Serial.println("MOTION DETECTED!");
  client.publish("esp8266/motion", "MOTION DETECTED!");
  previousTime = millis();
  startTimer = true;
}

void setup() {
  // Start the DS18B20 sensor
  sensors.begin();

  // Serial port for debugging purposes
  Serial.begin(115200);

  // PIR Motion Sensor mode, then set interrupt function and RISING mode
  pinMode(motionSensor, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(motionSensor), detectsMovement, RISING);
  
  // Initialize the output variable and the LED as OUTPUTs
  pinMode(output, OUTPUT);
  pinMode(statusLed, OUTPUT);
  digitalWrite(output, HIGH);
  digitalWrite(statusLed, LOW);
  
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Timer variable with current time
  now = millis();

  // Publishes new temperature and LDR readings every 30 seconds
  if (now - lastMeasure > 30000) {
    lastMeasure = now;
    sensors.requestTemperatures(); 
    // Temperature in Celsius degrees
    temperatureString = String(sensors.getTempCByIndex(0));
    // Uncomment the next line for temperature in Fahrenheit degrees
    //temperatureString = String(sensors.getTempFByIndex(0));
    // Publishes Temperature values
    client.publish("esp8266/temperature", temperatureString.c_str());
    Serial.println("Temperature published");
    
    // Publishes LDR values
    client.publish("esp8266/ldr", String(analogRead(ldr)).c_str());
    Serial.println("LDR values published");    
  }
  // After 10 seconds have passed since motion was detected, publishes a "No motion" message
  if ((now - previousTime > 10000) && startTimer) { 
    client.publish("esp8266/motion", "No motion");
    Serial.println("Motion stopped");
    startTimer = false;
  }
}
