# Discussion Report: Cloud-Edge Inference Design

## 1. Is server's confidence always higher?
Not always, but server confidence tends to be higher on ambiguous gestures due to larger, more accurate models.

## 2. Data Flow Sketch

[User Gesture] → [MPU6050] → [ESP32 Sampling]
→ [Local Model Inference] → [Confidence < 80?]
→ If YES → [Send to Server → Return Gesture]
→ If NO  → [Actuate LED]

## 3. Pros & Cons of Edge-first + Cloud Fallback

| Aspect               | Pros                                | Cons                                        |
|----------------------|-------------------------------------|---------------------------------------------|
| Connectivity         | Works mostly offline                | Still fails without WiFi during fallback    |
| Latency              | Real-time local feedback            | Cloud roundtrip causes delay                |
| Prediction Consistency | Reduces cloud dependence            | Potential label mismatch (edge vs cloud)    |
| Privacy              | Less data sent to server            | Raw data still exposed when sent            |

## 4. Mitigation Strategy
Use quantization-aware training to improve local model accuracy, and periodically OTA-update edge models based on server training improvements. 