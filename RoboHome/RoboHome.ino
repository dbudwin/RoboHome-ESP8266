#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <RCSwitch.h>
#include <RestClient.h>

const char* ssid = "";
const char* password = "";
const char* robohomeWebUrl = "";
const char* robohomeWebSha1Fingerprint = "";
const char* mqttChannel = "";
const char* mqttServer = "";
const char* mqttUser = "";
const char* mqttPassword = "";
const int mqttPort = 12345;

RCSwitch rcSwitch = RCSwitch();
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

RestClient restClient = RestClient(robohomeWebUrl, 443, robohomeWebSha1Fingerprint);

void setup() {
    Serial.begin(115200);
    
    connectToWifi();
    
    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(callback);
}

void loop() {
    if (!mqttClient.connected()) {
        connectToBroker();
    }
    
    mqttClient.loop();
}

void connectToWifi() {
    delay(10);
    Serial.println();
    Serial.println("Connecting to " + String(ssid));

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
    }

    Serial.print("WiFi connected! IP address: ");
    Serial.println(WiFi.localIP());
}

void connectToBroker() {
    while (!mqttClient.connected()) {
        if (mqttClient.connect("", mqttUser, mqttPassword)) {
            Serial.println("Connected to MQTT broker!");
            mqttClient.subscribe(mqttChannel);
        } else {
            Serial.print("Failed to connect to MQTT broker, will try again in 5 seconds, rc=" + mqttClient.state());
            delay(5000);
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    restClient.setContentType("application/x-www-form-urlencoded");

    String userId = getSectionFromString(topic, 1);
    int deviceId = getSectionFromString(topic, 2).toInt();
    String data = "userId=" + userId + "&action=" + message + "&deviceId=" + deviceId;

    String response = "";
    
    int statusCode = restClient.post("/api/devices/info", data.c_str(), &response);

    String body = readResponseBody(response);
    
    JsonObject& json = toJson(body);

    const char* code = json["code"];

    rcSwitch.enableTransmit(0); //Pin 3 on NodeMCU
    rcSwitch.setPulseLength(184);
    rcSwitch.send(atoi(code), 24);
}

JsonObject& toJson(String string) {
    DynamicJsonBuffer jsonBuffer;

    JsonObject& root = jsonBuffer.parseObject(string);

    if (!root.success()) {
        Serial.println("Parsing to JSON failed!");
        //return 0;
    }

    return root;
}

String readResponseBody(String response) {
    response.trim();
    
    int chunkSize = sizeOfChunk(response);
    int endOfChunkSizeIndex = response.indexOf("\r\n");
    
    String responseAfterChunkSize = response.substring(endOfChunkSizeIndex, response.length());
    responseAfterChunkSize.trim();

    String responseBody;
    
    for (int i = 0; i < chunkSize; i++) {
        responseBody += responseAfterChunkSize.charAt(i);
    }

    return responseBody;
}

unsigned int sizeOfChunk(String response) {
    int endOfChunkSizeIndex = response.indexOf("\r\n");
    String chunkString = response.substring(0, endOfChunkSizeIndex);
    unsigned int chunkSize = hexToInt(chunkString);

    return chunkSize;
}

unsigned int hexToInt(String hexString) {
    return strtoul(hexString.c_str(), NULL, 16);
}

String getSectionFromString(String data, int index)
{
    int stringData = 0; 
    String value = "";
    
    for (int i = 0; i < data.length(); i++) { 
        if (data[i] == '/') {
            stringData++; 
        } else if (stringData == index) {
            value += data[i];
        } else if (stringData > index) {
            return value;
        }
    }

    return value;
}
