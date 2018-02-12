#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <RCSwitch.h>
#include <RestClient.h>

const char* ssid = "";
const char* password = "";
const char* robohomeWebUrl = "";
const char* robohomeWebSha1Fingerprint = "";
const char* mqttTopic = "";
const char* mqttServer = "";
const char* mqttUser = "";
const char* mqttPassword = "";
const int mqttPort = 12345;

RCSwitch rcSwitch = RCSwitch();
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

RestClient restClient = RestClient(robohomeWebUrl, 443, robohomeWebSha1Fingerprint);

void setup() {
    enableBuiltinLed();

    Serial.begin(115200);

    connectToWifi();

    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttMessageReceivedCallback);
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
        turnOnBuiltinLedForMs(50);
        delay(50);
    }

    Serial.print("WiFi connected! IP address: ");
    Serial.println(WiFi.localIP());
}

void connectToBroker() {
    while (!mqttClient.connected()) {
        if (mqttClient.connect("", mqttUser, mqttPassword)) {
            turnBuiltinLedOff();
            Serial.println("Connected to MQTT broker!");
            mqttClient.subscribe(mqttTopic);
        } else {
            turnBuiltinLedOn();
            Serial.print("Failed to connect to MQTT broker, will try again in 5 seconds, rc=" + mqttClient.state());
            delay(5000);
        }
    }
}

void mqttMessageReceivedCallback(char* topic, byte* payload, size_t length) {
    const size_t retryAttempts = 3;

    for (size_t i = 0; i < retryAttempts; i++) {
        String body = getBodyOfHttpRequestForDeviceInfo(topic, payload, length);

        if (body.length() != 0) {
            controlDevice(body);
            break;
        }

        yield();
    }
}

String getBodyOfHttpRequestForDeviceInfo(char* topic, byte* payload, size_t length) {
    turnOnBuiltinLedForMs(100);
    
    String message = "";
    
    for (size_t i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    restClient.setContentType("application/x-www-form-urlencoded");

    String userId = getSectionFromString(topic, 1);
    String deviceId = getSectionFromString(topic, 2);
    String data = "userId=" + userId + "&action=" + message + "&deviceId=" + deviceId;
    String response = "";

    unsigned int statusCode = restClient.post("/api/devices/info", data.c_str(), &response);
    
    String body = readResponseBody(response);

    Serial.println("Data: " + data);

    return isResponseValid(statusCode, body) ? body : String();
}

bool isResponseValid(unsigned int statusCode, String body) {
    const unsigned int httpOkay = 200;
  
    if (statusCode != httpOkay) {
        Serial.println("Error, HTTP status code: " + String(statusCode));

        return false;
    }

    if (body.length() == 0) {
        Serial.println("Error, empty body!");

        return false;
    }

    Serial.println("Status Code: " + String(statusCode));
    Serial.println("JSON Response: " + body);

    return true;
}

void controlDevice(String body) {
    JsonObject& json = toJson(body);

    const char* code = json["code"];

    turnOnBuiltinLedForMs(100);

    rcSwitch.enableTransmit(0); //Pin D3 on NodeMCU
    rcSwitch.setPulseLength(184);

    unsigned int numberOfTimesToSendSignal = 10;

    for (size_t i = 0; i < numberOfTimesToSendSignal; i++) {
        rcSwitch.send(atoi(code), 24);

        yield();
    }
}

JsonObject& toJson(String string) {
    DynamicJsonBuffer jsonBuffer;

    JsonObject& root = jsonBuffer.parseObject(string);

    if (!root.success()) {
        Serial.println("Parsing to JSON failed!");
    }

    return root;
}

String readResponseBody(String response) {
    response.trim();
    
    size_t chunkSize = sizeOfChunk(response);
    unsigned int endOfChunkSizeIndex = response.indexOf("\r\n");
    
    String responseAfterChunkSize = response.substring(endOfChunkSizeIndex, response.length());
    responseAfterChunkSize.trim();

    String responseBody;
    
    for (size_t i = 0; i < chunkSize; i++) {
        responseBody += responseAfterChunkSize.charAt(i);
    }

    return responseBody;
}

size_t sizeOfChunk(String response) {
    unsigned int endOfChunkSizeIndex = response.indexOf("\r\n");
    String chunkString = response.substring(0, endOfChunkSizeIndex);
    size_t chunkSize = hexToInt(chunkString);

    return chunkSize;
}

unsigned int hexToInt(String hexString) {
    return strtoul(hexString.c_str(), NULL, 16);
}

String getSectionFromString(String data, unsigned int index)
{
    unsigned int stringData = 0; 
    String value = "";
    
    for (size_t i = 0; i < data.length(); i++) { 
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

void enableBuiltinLed() {
    pinMode(LED_BUILTIN, OUTPUT);
}

void turnBuiltinLedOn() {
    digitalWrite(LED_BUILTIN, LOW);
}

void turnBuiltinLedOff() {
    digitalWrite(LED_BUILTIN, HIGH);
}

void turnOnBuiltinLedForMs(uint millisecondsToTurnOn) {
    turnBuiltinLedOn();
    delay(millisecondsToTurnOn);
    turnBuiltinLedOff();
}
