#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <MFRC522.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <SPI.h>
#define LED_PIN 0
#define NFC_PIN 2
#define RST_PIN 5    // D1
#define SS_PIN 4     // D2
#define DOOR_PIN 3   // D9(RX)
#define SERVO_PIN 15 // D8
#define SWITCH_PIN 1 // D10
#if __has_include("info.h")
#include <info.h>
#else
/**
 * info.h只是方便我自己不用每次填写配置
 * 其他人在下方配置wifi和mqtt即可
 * 第一次刷入需将platformio.ini的上传方式改为串口模式
 */
const char *ssid = "WiFi名称";
const char *password = "WiFi密码";
const char *mqtt_username = "MQTT用户名";
const char *mqtt_password = "MQTT密码";
const char *mqtt_broker = "MQTT服务器地址";
const int mqtt_port = 1883;
#endif

// 设备名称
String device_name = "DoorLock";
// NFC配置
const int CARDS = 4;                                         // 存储的卡片数量上限
const int CARD_SIZE = 4;                                     // 卡片UID的长度
byte User_ID[CARDS][CARD_SIZE] = {{0x01, 0xED, 0x4D, 0x1C}}; // 存储的卡片UID
String WaitReadCard = "OFF";
const char *nfc_uid_topic = "sensor/door/nfc";
const char *nfc_state_topic = "switch/nfc/state";
const char *nfc_cmd_topic = "switch/nfc/cmd";
// 房门开合状态
String door_state = "ON";
bool DoorState = HIGH;
const char *door_state_topic = "binary_sensor/door/state";
// 门锁
String lock_state = "UNLOCK";
const char *lock_state_topic = "lock/doorlock/state"; // 上传状态主题
const char *lock_cmd_topic = "lock/doorlock/cmd";     // 接受命令主题
// 定义物理开关状态（高电平松开，低电平按下）
bool prevSwitchState = true;
// 定义上一次按下物理开关的时间
unsigned long lastClickTime = 0;
// 定义上一次重启NFC模块的时间
unsigned long lastRestartTime = 0;
// WiFi对象
WiFiClient espClient;
// MQTT对象
PubSubClient client(espClient);
// 建立舵机对象 myservo
Servo myservo;
// 创建新的mfrc522实例
MFRC522 rfid(SS_PIN, RST_PIN);

void pub_mqtt_state()
{
    client.publish(nfc_state_topic, WaitReadCard.c_str());
    Serial.println("上传房门状态: " + door_state);
    client.publish(door_state_topic, door_state.c_str());
    Serial.println("上传房门状态: " + door_state);
    client.publish(lock_state_topic, lock_state.c_str());
    Serial.println("上传门锁状态: " + lock_state);
    ESP.wdtFeed();
}

void nfc_restart()
{
    unsigned long restartTime = millis();
    if (restartTime - lastRestartTime > 300000)
    {
        digitalWrite(NFC_PIN, LOW);
        delay(10);
        digitalWrite(NFC_PIN, HIGH);
        // SPI.begin();
        rfid.PCD_Init();
        lastRestartTime = millis();
    }
}

void DoorSensor()
{
    // 读取开关状态
    bool newDoorState = digitalRead(DOOR_PIN);
    // 如果开关状态改变
    if (newDoorState != DoorState)
    {
        // 延迟10毫秒避免状态抖动
        delay(10);
        // 再次读取开关状态
        newDoorState = digitalRead(DOOR_PIN);
        // 如果开关状态仍然改变
        if (newDoorState != DoorState)
        {
            // 更新开关状态变量
            DoorState = newDoorState;
            if (DoorState)
                door_state = "ON";
            else
                door_state = "OFF";
            pub_mqtt_state();
        }
    }
}

void Lock()
{
    if (!DoorState && lock_state == "UNLOCK")
    {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("锁定");
        myservo.write(120);
        delay(100);
        lock_state = "LOCK";
    }
    else if (lock_state == "LOCK")
    {
        digitalWrite(LED_PIN, LOW);
        Serial.println("解锁");
        myservo.write(0);
        delay(100);
        lock_state = "UNLOCK";
    }
    pub_mqtt_state();
}

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.println("-----------------------");
    Serial.printf("消息来自主题: %s\n", topic);
    String message;
    for (unsigned int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }
    Serial.println("消息内容: " + message);
    Serial.print("MQTT执行操作: ");
    if (strcmp(topic, lock_cmd_topic) == 0)
    {
        if (message == "UNLOCK")
            lock_state = "LOCK";
        else
            lock_state = "UNLOCK";
        lastClickTime = millis();
        Lock();
    }
    else if (strcmp(topic, nfc_cmd_topic) == 0)
    {
        WaitReadCard = message;
        pub_mqtt_state();
    }
}

void eepromWrite()
{
    for (int i = 0; i < CARDS; i++)
    {
        EEPROM.put(i * CARD_SIZE, User_ID[i]);
    }
    EEPROM.commit();
}

void eepromRead()
{
    String all_uid = "";
    for (int i = 0; i < CARDS; i++)
    {
        EEPROM.get(i * CARD_SIZE, User_ID[i]);
        if (User_ID[i][0] != '\0')
        {
            for (int j = 0; j < CARD_SIZE; j++)
            {
                all_uid.concat(String(User_ID[i][j] < 0x10 ? "0" : ""));
                all_uid.concat(String(User_ID[i][j], HEX));
            }
            all_uid.concat(i < CARDS ? ",\n" : "");
        }
    }
    client.publish(nfc_uid_topic, all_uid.c_str());
}

String rfidReadUid(byte *buffer, byte bufferSize)
{
    String cardUid = "";
    for (byte i = 0; i < bufferSize; i++)
    {
        cardUid.concat(String(buffer[i] < 0x10 ? "0" : ""));
        cardUid.concat(String(buffer[i], HEX));
    }
    return cardUid;
}

void NFC()
{
    if (rfid.PICC_IsNewCardPresent())
    {
        // 获取当前时间
        unsigned long clickTime = millis();
        // 如果距离上次单击时间小于500毫秒则退出
        if (clickTime - lastClickTime < 1500)
            return;
        if (rfid.PICC_ReadCardSerial())
        {
            String cardUid = rfidReadUid(rfid.uid.uidByte, rfid.uid.size);
            Serial.println("卡片UID: " + cardUid);
            client.publish(nfc_uid_topic, ("UID:" + cardUid).c_str());
            bool found = false;
            for (byte num = 0; num < CARDS; num++)
            {
                String storedUid = rfidReadUid(User_ID[num], CARD_SIZE);
                if (WaitReadCard == "OFF" && cardUid.equals(storedUid))
                {
                    Serial.println("验证通过");
                    Lock();
                    break;
                }
                else if (WaitReadCard == "OFF" && num == CARDS - 1)
                {
                    Serial.println("验证失败");
                }
                else if (WaitReadCard == "ON" && cardUid.equals(storedUid))
                {
                    memset(User_ID[num], 0, CARD_SIZE);
                    eepromWrite();
                    Serial.println("已删除UID: " + cardUid);
                    found = true;
                    WaitReadCard = "OFF";
                    client.publish(nfc_state_topic, WaitReadCard.c_str());
                    break;
                }
            }
            if (WaitReadCard == "ON" && !found)
            {
                // 查看User_ID有几组数据
                int index = -1;
                for (int i = 0; i < CARDS; i++)
                {
                    if (User_ID[i][0] == '\0')
                    {
                        index = i;
                        break;
                    }
                }
                if (index != -1)
                {
                    // 将UID添加到User_ID变量
                    for (int i = 0; i < rfid.uid.size; i++)
                    {
                        User_ID[index][i] = rfid.uid.uidByte[i];
                    }
                    eepromWrite();
                    Serial.println("已添加UID到内部存储");
                }
                else
                {
                    Serial.println("无法添加UID，存储已满");
                }
                WaitReadCard = "OFF";
                client.publish(nfc_state_topic, WaitReadCard.c_str());
            }
            // 使放置在读卡区的IC卡进入休眠状态，不再重复读卡
            rfid.PICC_HaltA();
            // 停止读卡模块编码
            rfid.PCD_StopCrypto1();
            lastClickTime = clickTime;
        }
    }
    nfc_restart();
}

void Button()
{
    // 读取开关状态
    bool newSwitchState = digitalRead(SWITCH_PIN);
    // 如果开关状态改变
    if (newSwitchState != prevSwitchState)
    {
        // 延迟10毫秒避免状态抖动
        delay(10);
        // 再次读取开关状态
        newSwitchState = digitalRead(SWITCH_PIN);
        // 如果开关状态仍然改变
        if (newSwitchState != prevSwitchState)
        {
            // 更新开关状态变量
            prevSwitchState = newSwitchState;
            // 如果开关打开
            if (!prevSwitchState)
            {
                // 获取当前时间
                unsigned long clickTime = millis();
                // 如果距离上次单击时间超过500毫秒
                if (clickTime - lastClickTime > 500)
                {
                    // 记录当前单击时间，并打印单击状态
                    lastClickTime = clickTime;
                    Lock();
                }
            }
        }
    }
}

void WiFiConnect()
{
    WiFi.hostname(device_name);
    WiFi.begin(ssid, password);
    Serial.print("\n正在连接WiFi, 请稍等");
    while (WiFi.status() != WL_CONNECTED && digitalRead(SWITCH_PIN))
    {
        delay(1000);
        Serial.print(".");
    }
    if (digitalRead(SWITCH_PIN))
    {
        Serial.printf("\nWiFi名称: %s\n", ssid);
        Serial.print("IP地址: ");
        Serial.println(WiFi.localIP());
        Serial.println("MAC地址: " + WiFi.macAddress());
    }
}

void MQTTConnect()
{
    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(callback);
    while (!client.connected() && digitalRead(SWITCH_PIN))
    {
        String client_id = device_name + "-" + WiFi.macAddress();
        Serial.printf("设备%s正在连接到MQTT服务器...\n", client_id.c_str());
        if (client.connect(client_id.c_str(), mqtt_username, mqtt_password))
        {
            Serial.println("MQTT服务器已连接");
            client.subscribe(lock_cmd_topic);
            client.subscribe(nfc_cmd_topic);
            DoorSensor();
            pub_mqtt_state();
        }
        else
        {
            Serial.print("连接失败, 即将重试 代码：");
            Serial.println(client.state());
            delay(1000);
        }
    }
}

void setup()
{
    Serial.begin(9600);
    pinMode(LED_PIN, OUTPUT);
    pinMode(NFC_PIN, OUTPUT);
    pinMode(SERVO_PIN, OUTPUT);
    pinMode(DOOR_PIN, INPUT_PULLUP);
    pinMode(SWITCH_PIN, INPUT_PULLUP);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(NFC_PIN, HIGH);
    myservo.attach(SERVO_PIN, 500, 2500);
    myservo.write(0);
    // 连接 WiFi
    WiFiConnect();
    // 连接 MQTT 服务器
    MQTTConnect();
    SPI.begin();
    rfid.PCD_Init();
    EEPROM.begin(CARDS * CARD_SIZE);
    eepromRead();
    // OTA设置并启动
    ArduinoOTA.setHostname(device_name.c_str());
    ArduinoOTA.begin();
    Serial.println("初始化完成");
}

void loop()
{
    ArduinoOTA.handle();
    client.loop();
    if (WiFi.status() != WL_CONNECTED && digitalRead(SWITCH_PIN))
    {
        Serial.print("WiFi已断开, 即将重新连接WiFi");
        WiFiConnect();
        ArduinoOTA.setHostname(device_name.c_str());
        ArduinoOTA.begin();
    }
    if (!client.connected() && digitalRead(SWITCH_PIN))
    {
        Serial.println("MQTT已断开, 即将重新连接MQTT");
        MQTTConnect();
    }
    DoorSensor();
    Button();
    NFC();
}
