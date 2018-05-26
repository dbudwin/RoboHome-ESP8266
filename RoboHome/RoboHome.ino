#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <RCSwitch.h>
#include <RestClient.h>

const char* SSID = "";
const char* WIFI_PASSWORD = "";
const char* ROBOHOME_URL = "";
const char* ROBOHOME_SHA1_FINGERPRINT = "";
const String ROBOHOME_USERNAME = "";
const String ROBOHOME_PASSWORD = "";
const String ROBOHOME_PASSWORD_GRANT_CLIENT_SECRET = "";
const int ROBOHOME_PASSWORD_GRANT_CLIENT_ID = 2;

const char* mqttTopic = "";
const char* mqttUser = "";
const char* mqttPassword = "";

RCSwitch rcSwitch = RCSwitch();
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
String ACCESS_TOKEN;

void setup() {
    enableBuiltinLed();

    rcSwitch.setRepeatTransmit(100);

    Serial.begin(115200);

    connectToWifi();

    ACCESS_TOKEN = getAccessToken();

    setMqttSettingsFromRoboHomeServer();
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
    Serial.println("Connecting to " + String(SSID));

    WiFi.begin(SSID, WIFI_PASSWORD);

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

String getAccessToken() {
    RestClient restClient = RestClient(ROBOHOME_URL, 443, ROBOHOME_SHA1_FINGERPRINT);

    restClient.setContentType("application/x-www-form-urlencoded");

    String data = "grant_type=password&client_id=" + String(ROBOHOME_PASSWORD_GRANT_CLIENT_ID) + "&client_secret=" + ROBOHOME_PASSWORD_GRANT_CLIENT_SECRET + "&username=" + ROBOHOME_USERNAME + "&password=" + ROBOHOME_PASSWORD + "&scope=info";
    String response = "";

    unsigned int statusCode = restClient.post("/oauth/token", data.c_str(), &response);

    String body = readResponseBody(response);

    if (isResponseValid(statusCode, body)) {
        JsonObject& json = toJson(body);

        const char* accessToken = json["access_token"];

        return String(accessToken);
    }
}

void setMqttSettingsFromRoboHomeServer() {
    RestClient restClient = RestClient(ROBOHOME_URL, 443, ROBOHOME_SHA1_FINGERPRINT);

    String authorizationBearerHeader = "Authorization: Bearer " + ACCESS_TOKEN;

    restClient.setHeader(authorizationBearerHeader.c_str());

    String response = "";

    unsigned int statusCode = restClient.get("/api/settings/mqtt", &response);

    String body = readResponseBody(response);

    if (isResponseValid(statusCode, body)) {
        JsonObject& json = toJson(body);

        const char* mqttServer = json["mqtt"]["server"];
        const unsigned int mqttPort = 17431; // To use json["mqtt"]["tlsPort"]; the PubSubClient needs to support TLS first
        mqttUser = json["mqtt"]["user"];
        mqttPassword = json["mqtt"]["password"];
        mqttTopic = json["mqtt"]["topic"];

        mqttClient.setServer(mqttServer, mqttPort);
        mqttClient.setCallback(mqttMessageReceivedCallback);
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

    String authorizationBearerHeader = "Authorization: Bearer " + ACCESS_TOKEN;

    RestClient restClient = RestClient(ROBOHOME_URL, 443, ROBOHOME_SHA1_FINGERPRINT);
    
    restClient.setHeader(authorizationBearerHeader.c_str());
    restClient.setContentType("application/x-www-form-urlencoded");

    String publicDeviceId = getSectionFromString(topic, 2);
    String data = "action=" + message + "&publicDeviceId=" + publicDeviceId;
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

    turnOnBuiltinLedForMs(100);

    const char* code = json["code"];
    const char* pulseLength = json["pulse_length"];

    rcSwitch.enableTransmit(0); //Pin D3 on NodeMCU
    rcSwitch.setPulseLength(atoi(pulseLength));
    rcSwitch.send(atoi(code), 24);
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
