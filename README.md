## NFC门锁
1. 使用物理开关、NFC、HomeAssistant控制。
2. 支持检测房门开启和关闭。
3. 房门开启时无法上锁。
4. 定时重启NFC模块防止休眠无法刷卡。
5. 支持WiFi OTA更新固件(第一次刷入需将platformio.ini的上传方式改为串口模式)。
6. 使用NFC卡片前，需先扫描一次卡片，卡片UID将会显示在串口或mqtt主题“sensor/door/nfc”处，请将UID写入程序的NFC配置，然后重新刷写程序。

## 固件下载
[firmware.bin](https://github.com/QiYueYiya/DoorLock/releases/download/DoorLock/firmware.bin)

## HomeAssistant配置文件
configuration.yaml
```
mqtt:
  binary_sensor:
    - unique_id: DoorSensor
      name: "房门"
      state_topic: "sensor/door/state"
      device_class: "door"
      qos: 2
      device:
        name: "门锁"
        model: "qiyueyi.mqtt.doorlock"
        manufacturer: "厂商名称"
        identifiers: "设备MAC地址"
  lock:
    - unique_id: DoorLock
      name: "门锁"
      state_topic: "DoorLock/state"
      command_topic: "DoorLock/cmd"
      payload_lock: "LOCK"
      payload_unlock: "UNLOCK"
      state_locked: "LOCK"
      state_unlocked: "UNLOCK"
      qos: 2
      device:
        name: "门锁"
        model: "qiyueyi.mqtt.doorlock"
        manufacturer: "厂商名称"
        identifiers: "设备MAC地址"
```

## 材料
5V电源、导线、霍尔传感器\*1个、微动开关\*1个、小磁铁\*1个、ESP8266nodemcu\*1个、MFRC522模块、SG90舵机\*1个、10K电阻\*2个。

## 接线
1. 图上的NPN三极管实际为霍尔传感器，连接时请注意霍尔传感器的引脚顺序。
2. 物理开关请使用微动开关或其他无锁开关。
3. 磁铁的磁极方向会影响霍尔传感器感应，请注意磁铁的磁极方向。

![image](.vscode/IMG_02.png)

## 实物图

<img width = '60%' src =".vscode/IMG_01.jpg"/>