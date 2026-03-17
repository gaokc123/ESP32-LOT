#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ESP32-SpeexDSP.h> // 引入 SpeexDSP 库

// ================= 用户配置区 =================
const char* ssid     = "Bone";
const char* password = "12345678";
const char* host     = "172.20.10.5";
const int   httpPort = 8000;
const char* url      = "/api/upload/";

// ================= 全局参数 =================
#define SAMPLE_RATE     16000
#define MAX_RECORD_SEC  15
size_t record_size = 0;
int current_rec_len = 0;

// 双缓冲区设计 (极其重要)
int16_t *raw_buffer = NULL;       // 原始环境音缓冲
int16_t *processed_buffer = NULL; // 降噪后的人声缓冲
bool play_processed_mode = true;  // 播放模式切换标志

// ================= 硬件引脚定义 =================
// 麦克风 & 扬声器 I2S 引脚 (保持不变)
#define MIC_I2S_WS   4
#define MIC_I2S_SCK  5
#define MIC_I2S_SD   6
#define AMP_I2S_LRC  16
#define AMP_I2S_BCLK 15
#define AMP_I2S_DIN  7

// 按键引脚
#define PIN_RECORD_BTN 39 // 录音键
#define PIN_PLAY_BTN   40 // 播放/切换键 (新增)

// OLED 屏幕
#define SCREEN_SDA   41
#define SCREEN_SCL   42
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// DSP 降噪对象
ESP32SpeexDSP dsp;

// 任务句柄
TaskHandle_t AudioTaskHandle = NULL;

// ================= UI 辅助函数 =================
void showStatus(String title, String subtitle = "") {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy16_t_gb2312); 
  u8g2.setCursor(0, 16); 
  u8g2.print(title);
  if(subtitle != "") {
    u8g2.setCursor(0, 40); 
    u8g2.print(subtitle);
  }
  u8g2.sendBuffer();
  Serial.println(title + " " + subtitle);
}

// ================= I2S 初始化 (同上个版本，略微折叠保持整洁) =================
void i2s_install() {
  i2s_config_t i2s_config_mic = { .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), .sample_rate = SAMPLE_RATE, .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, .communication_format = I2S_COMM_FORMAT_I2S, .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, .dma_buf_count = 8, .dma_buf_len = 64, .use_apll = false };
  i2s_driver_install(I2S_NUM_0, &i2s_config_mic, 0, NULL);
  i2s_pin_config_t pin_config_mic = { .bck_io_num = MIC_I2S_SCK, .ws_io_num = MIC_I2S_WS, .data_out_num = -1, .data_in_num = MIC_I2S_SD };
  i2s_set_pin(I2S_NUM_0, &pin_config_mic);

  i2s_config_t i2s_config_amp = { .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), .sample_rate = SAMPLE_RATE, .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, .communication_format = I2S_COMM_FORMAT_I2S, .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, .dma_buf_count = 8, .dma_buf_len = 64, .use_apll = false };
  i2s_driver_install(I2S_NUM_1, &i2s_config_amp, 0, NULL);
  i2s_pin_config_t pin_config_amp = { .bck_io_num = AMP_I2S_BCLK, .ws_io_num = AMP_I2S_LRC, .data_out_num = AMP_I2S_DIN, .data_in_num = -1 };
  i2s_set_pin(I2S_NUM_1, &pin_config_amp);
}


// WAV 头生成函数 (保持不变)
void createWavHeader(uint8_t *header, int waveDataSize) { /*...同上版本...*/ }

// ================= 上传函数 (支持双文件传参) =================
// data_buffer: 要上传的音频数组指针
// data_len: 音频数据的实际长度(字节)
// file_type: 文件标识，传入 "raw" 或 "processed"
void performUpload(int16_t *data_buffer, int data_len, String file_type) {
  // 1. 检查 WiFi 连接
  if (WiFi.status() != WL_CONNECTED) {
    showStatus("发生错误", "WiFi已断开!");
    return;
  }
  
  // 2. 连接服务器
  showStatus("连接服务器...", host);
  WiFiClient client;
  if (!client.connect(host, httpPort)) {
    showStatus("发生错误", "服务器连接失败");
    return;
  }
  
  // 3. 准备上传
  showStatus("正在上传...", file_type + ".wav");

  // 构建 HTTP Multipart 表单数据
  String boundary = "ESP32Boundary";
  String bodyHead = "--" + boundary + "\r\n";
  // 将传入的 file_type 拼接到文件名中
  bodyHead += "Content-Disposition: form-data; name=\"audio_file\"; filename=\"" + file_type + ".wav\"\r\n";
  bodyHead += "Content-Type: audio/wav\r\n\r\n";
  String bodyTail = "\r\n--" + boundary + "--\r\n";

  // 计算总内容长度 = 头部 + WAV头(44字节) + 音频数据 + 尾部
  int wavHeaderSize = 44;
  int totalContentLen = bodyHead.length() + wavHeaderSize + data_len + bodyTail.length();

  // 4. 发送 HTTP 请求头
  client.println("POST " + String(url) + " HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.print("Content-Length: "); client.println(totalContentLen);
  client.println(); // 空行表示请求头结束

  // 5. 发送数据体
  client.print(bodyHead); // 发送表单头
  
  uint8_t wavHeader[44];
  createWavHeader(wavHeader, data_len); // 生成 WAV 头
  client.write(wavHeader, 44); // 发送 WAV 头

  // 分块发送音频数据 (防止一次发太大导致网络堵塞)
  int chunkSize = 2048;
  int sent = 0;
  uint8_t *byteBuffer = (uint8_t *)data_buffer; 
  while (sent < data_len) {
    int toSend = (data_len - sent) > chunkSize ? chunkSize : (data_len - sent);
    client.write(byteBuffer + sent, toSend);
    sent += toSend;
  }
  client.print(bodyTail); // 发送表单尾部

  // 6. 等待服务器响应
  long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) { // 10秒超时
      showStatus("发生错误", file_type + " 上传超时");
      client.stop();
      return;
    }
  }
  
  client.stop(); // 断开连接
  Serial.println("[Upload] " + file_type + ".wav sent successfully.");
}


// ================= 主逻辑任务 (RTOS 任务) =================
void AudioTask(void *pvParameters) {
  uint32_t command = 0;

  while (true) {
    showStatus("系统就绪", "39录音 | 40播放");
    
    // 阻塞等待按键任务发来的指令 (1=录音, 2=播放)
    xTaskNotifyWait(0, 0xFFFFFFFF, &command, portMAX_DELAY);
    
    if (command == 1) {
      // ===== 阶段 1: 录音 =====
      Serial.println("[Audio] 开始录音");
      current_rec_len = 0;
      showStatus("正在录音...", "再次按 39 停止");
      
      size_t bytes_read;
      int chunk = 1024;
      
      while((current_rec_len + chunk < record_size)) {
         // 检查是否有停止信号
         if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
            Serial.println("[Audio] 收到停止信号");
            break; 
         }
         // 读取麦克风数据到【原始缓冲区】
         i2s_read(I2S_NUM_0, (uint8_t*)raw_buffer + current_rec_len, chunk, &bytes_read, portMAX_DELAY);
         current_rec_len += bytes_read;
      }
      
      // ===== 阶段 2: 降噪处理 =====
      showStatus("执行自适应降噪", "计算中...");
      
      // 先将原始数据完整拷贝一份到【处理缓冲区】
      memcpy(processed_buffer, raw_buffer, current_rec_len);
      
      int frame_size = 256; // Speex 要求的帧大小
      int num_frames = (current_rec_len / 2) / frame_size; // 计算共有多少帧 (16bit=2bytes)
      
      for(int i = 0; i < num_frames; i++) {
          // 调用 SpeexDSP 库的麦克风预处理函数，进行原地降噪滤波
          dsp.preprocessMicAudio(&processed_buffer[i * frame_size]); 
      }
      
      // ===== 阶段 3: 双文件上传 =====
      // 连续调用两次 performUpload 函数，将两个缓冲区的数据分别传给 Django
      performUpload(raw_buffer, current_rec_len, "raw");
      performUpload(processed_buffer, current_rec_len, "processed");
      
      showStatus("处理且上传完成", "请按 40 试听对比");

    } 
    else if (command == 2) {
      // ===== 阶段 4: 对比播放 =====
      if (current_rec_len == 0) {
         showStatus("无录音数据", "请先按 39 录音");
         vTaskDelay(pdMS_TO_TICKS(1000));
         continue; // 跳过本次循环
      }

      // 每次按下 40 键，切换一次播放模式标志位
      play_processed_mode = !play_processed_mode;
      
      // 根据标志位选择要播放的指针数组
      int16_t *play_ptr = play_processed_mode ? processed_buffer : raw_buffer;
      
      if(play_processed_mode) {
         showStatus("正在播放:", "【降噪后】纯人声");
      } else {
         showStatus("正在播放:", "【降噪前】原始音");
      }

      size_t bytes_written;
      int sent = 0;
      int chunk = 1024;
      
      // 将选定的音频数据写入扬声器的 I2S 接口
      while(sent < current_rec_len) {
         int toSend = (current_rec_len - sent) > chunk ? chunk : (current_rec_len - sent);
         i2s_write(I2S_NUM_1, (uint8_t*)play_ptr + sent, toSend, &bytes_written, portMAX_DELAY);
         sent += toSend;
      }
      
      showStatus("播放完毕", "系统就绪");
      vTaskDelay(pdMS_TO_TICKS(1000)); // 防止按键误触导致连播
    }
    
    // 清除可能堆积的误触信号，确保下次状态干净
    xTaskNotifyStateClear(NULL);
  }
}

// ================= 按键扫描任务 =================
void ButtonTask(void *pvParameters) {
  vTaskDelay(pdMS_TO_TICKS(1000));
  bool lastState39 = digitalRead(PIN_RECORD_BTN);
  bool lastState40 = digitalRead(PIN_PLAY_BTN);
  
  while (true) {
    bool state39 = digitalRead(PIN_RECORD_BTN);
    bool state40 = digitalRead(PIN_PLAY_BTN);
    
    // 监测 39 号引脚 (录音)
    if (lastState39 == HIGH && state39 == LOW) {
      vTaskDelay(pdMS_TO_TICKS(50));
      if(digitalRead(PIN_RECORD_BTN) == LOW) {
         Serial.println("[Btn] 39 Pressed! -> Record");
         xTaskNotify(AudioTaskHandle, 1, eSetValueWithOverwrite); // 发送命令 1
         while(digitalRead(PIN_RECORD_BTN) == LOW) vTaskDelay(50);
      }
    }
    
    // 监测 40 号引脚 (播放/切换)
    if (lastState40 == HIGH && state40 == LOW) {
      vTaskDelay(pdMS_TO_TICKS(50));
      if(digitalRead(PIN_PLAY_BTN) == LOW) {
         Serial.println("[Btn] 40 Pressed! -> Play");
         xTaskNotify(AudioTaskHandle, 2, eSetValueWithOverwrite); // 发送命令 2
         while(digitalRead(PIN_PLAY_BTN) == LOW) vTaskDelay(50);
      }
    }
    
    lastState39 = state39;
    lastState40 = state40;
    vTaskDelay(50);
  }
}

// ================= Setup 初始化 =================
void setup() {
  Serial.begin(115200);
  
  Wire.begin(SCREEN_SDA, SCREEN_SCL);
  u8g2.begin();
  u8g2.enableUTF8Print(); 
  showStatus("系统启动中...");

  // 1. 初始化双缓冲区 (内存消耗翻倍，必须检查 PSRAM)
  if(psramFound()){
    record_size = SAMPLE_RATE * 2 * MAX_RECORD_SEC;
    raw_buffer = (int16_t *)ps_malloc(record_size);
    processed_buffer = (int16_t *)ps_malloc(record_size);
    Serial.println("PSRAM OK - Double Buffering Active");
  } else {
    // 若无外部 RAM，为防止溢出，单次最大只能录制 3 秒左右
    record_size = SAMPLE_RATE * 2 * 3; 
    raw_buffer = (int16_t *)malloc(record_size);
    processed_buffer = (int16_t *)malloc(record_size);
    Serial.println("No PSRAM - Memory Limited");
  }
  
  if (raw_buffer == NULL || processed_buffer == NULL) {
    showStatus("内存错误!");
    while(1);
  }

  // 2. 初始化按键和 I2S
  pinMode(PIN_RECORD_BTN, INPUT_PULLUP);
  pinMode(PIN_PLAY_BTN, INPUT_PULLUP); // 新增 40 号按键
  i2s_install();

  // 3. 初始化 SpeexDSP 降噪引擎
  // 帧大小设为 256，采样率 16000
  dsp.beginMicPreprocess(256, SAMPLE_RATE);
  dsp.enableMicNoiseSuppression(true);
  dsp.setMicNoiseSuppressionLevel(-30); // 降噪深度(负值越大压制越狠，如 -30dB)

  // 4. 连接 WiFi (测试本地降噪时可先注释掉)
  // showStatus("正在连接 WiFi...", ssid);
  // WiFi.begin(ssid, password);
  // while (WiFi.status() != WL_CONNECTED) { delay(500); }

  // 5. 创建 RTOS 任务
  xTaskCreatePinnedToCore(AudioTask, "AudioTask", 10240, NULL, 3, &AudioTaskHandle, 1);
  xTaskCreatePinnedToCore(ButtonTask, "BtnTask", 2048, NULL, 2, NULL, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}