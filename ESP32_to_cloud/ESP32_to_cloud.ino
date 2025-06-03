#include <Arduino.h>
#include <Tressi-lab4_inferencing.h> // Edge Impulse 模型头文件
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ====================== 配置区域 ======================
// 引脚定义
#define RED_LED    D2
#define GREEN_LED  D1
#define BLUE_LED   D0
#define BUTTON_PIN D3

// WiFi 信息（替换成你自己的）
const char* ssid = "Tressi";
const char* password = "00000000";

// 云端 API 地址（替换成你自己的服务器）
const char* serverUrl = "http://10.18.133.92:8000/predict"; // ✅ 正确地址

// 本地推理置信度阈值
#define CONFIDENCE_THRESHOLD 80.0

// =====================================================

// 传感器对象
Adafruit_MPU6050 mpu;

// 数据采样参数
#define SAMPLE_RATE_MS 10
#define CAPTURE_DURATION_MS 1000

// 特征数据数组
float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

// 状态变量
bool capturing = false;
bool buttonPressed = false;
unsigned long last_sample_time = 0;
unsigned long capture_start_time = 0;
int sample_count = 0;

// 给模型读取数据的接口
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, features + offset, length * sizeof(float));
  return 0;
}

// 控制 LED 灯亮灭
void lightLED(const char* label) {
  if (strcmp(label, "O") == 0) {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BLUE_LED, LOW);
  } else if (strcmp(label, "V") == 0) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(BLUE_LED, LOW);
  } else if (strcmp(label, "Z") == 0) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BLUE_LED, HIGH);
  } else {
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BLUE_LED, LOW);
  }
}

// 云端推理发送函数
void sendRawDataToServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<1024> doc;
  JsonArray data = doc.createNestedArray("data");
  for (int i = 0; i < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i++) {
    data.add(features[i]);
  }

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  int httpResponseCode = http.POST(jsonPayload);
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Server response: " + response);

    DynamicJsonDocument responseDoc(256);
    DeserializationError error = deserializeJson(responseDoc, response);

    if (!error) {
      const char* gesture = responseDoc["gesture"];
      float confidence = responseDoc["confidence"];

      Serial.println("Server Inference Result:");
      Serial.print("Gesture: ");
      Serial.println(gesture);
      Serial.print("Confidence: ");
      Serial.print(confidence);
      Serial.println("%");

      lightLED(gesture);
      delay(1000);
      lightLED("off");
    } else {
      Serial.print("Failed to parse server response: ");
      Serial.println(error.c_str());
    }

  } else {
    Serial.printf("Error sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

// 初始化
void setup() {
  Serial.begin(115200);

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // 初始化 MPU6050
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found!");
    while (1) delay(10);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // 连接 WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  Serial.println("Setup complete. Press button to recognize gesture.");
}

// 数据采集 + 推理函数
void capture_accelerometer_data() {
  if (millis() - last_sample_time >= SAMPLE_RATE_MS) {
    last_sample_time = millis();

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    if (sample_count < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE / 3) {
      int idx = sample_count * 3;
      features[idx] = a.acceleration.x;
      features[idx + 1] = a.acceleration.y;
      features[idx + 2] = a.acceleration.z;
      sample_count++;
    }

    if (millis() - capture_start_time >= CAPTURE_DURATION_MS) {
      capturing = false;

      ei_impulse_result_t result = { 0 };
      signal_t signal;
      signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
      signal.get_data = &raw_feature_get_data;

      EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
      if (res != EI_IMPULSE_OK) {
        Serial.println("Failed to run classifier.");
        return;
      }

      float max_val = 0;
      int max_idx = -1;
      for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > max_val) {
          max_val = result.classification[i].value;
          max_idx = i;
        }
      }

      if (max_idx >= 0) {
        const char* label = ei_classifier_inferencing_categories[max_idx];
        float confidence = max_val * 100;

        Serial.print("Detected gesture: ");
        Serial.print(label);
        Serial.print(" (");
        Serial.print(confidence);
        Serial.println("%)");

        if (confidence < CONFIDENCE_THRESHOLD) {
          Serial.println("Low confidence - sending raw data to server...");
          sendRawDataToServer();
        } else {
          lightLED(label);
          delay(1000);
          lightLED("off");
        }
      }
    }
  }
}

// 主循环
void loop() {
  if (digitalRead(BUTTON_PIN) == LOW && !buttonPressed) {
    buttonPressed = true;
    capturing = true;
    sample_count = 0;
    capture_start_time = millis();
    last_sample_time = millis();
    Serial.println("Button pressed! Start capturing...");
  }

  if (digitalRead(BUTTON_PIN) == HIGH) {
    buttonPressed = false;
  }

  if (capturing) {
    capture_accelerometer_data();
  }
}

// ESP32 cloud fallback logic sketch