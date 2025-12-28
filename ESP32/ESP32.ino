#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= 用户配置区 (必须修改!) =================
const char* ssid     = "Bone";          // WiFi 名称
const char* password = "12345678";      // WiFi 密码
// 电脑 IP 地址 (Django 后端服务器地址)
// 注意：这个 IP 地址必须是你电脑在局域网内的 IP，不能是 127.0.0.1
// 可以在电脑终端输入 ipconfig 查看
const char* host     = "172.20.10.5"; 
const int   httpPort = 8000;            // Django 默认端口
const char* url      = "/api/upload/";  // 后端上传接口地址

// ================= 全局参数 =================
#define SAMPLE_RATE     16000           // 采样率 16kHz (人声清晰度足够)
#define MAX_RECORD_SEC  10              // 最大录音时长 (秒)
size_t record_size = 0;                 // 录音缓冲区大小 (字节)，在 setup 中计算
int16_t *rec_buffer = NULL;             // 指向录音数据的指针
int current_rec_len = 0;                // 当前实际录制的长度

// ================= 硬件引脚定义 =================
// 麦克风 (INMP441) I2S 引脚
#define MIC_I2S_WS   4
#define MIC_I2S_SCK  5
#define MIC_I2S_SD   6

// 扬声器 (MAX98357A) I2S 引脚
#define AMP_I2S_LRC  16
#define AMP_I2S_BCLK 15
#define AMP_I2S_DIN  7

// 录音按键引脚 (GPIO 39)
// 注意：有些开发板 GPIO 0 是 Boot 键，这里改用 39
#define PIN_RECORD_BTN 39  

// OLED 屏幕 I2C 引脚
#define SCREEN_SDA   41
#define SCREEN_SCL   42
// OLED 对象初始化
Adafruit_SSD1306 display(128, 32, &Wire, -1);

// ================= 状态标志 =================
TaskHandle_t RecordTaskHandle = NULL;   // RTOS 任务句柄，用于任务间通信

// ================= UI 辅助函数 =================
// 在 OLED 屏幕上显示状态信息
// title: 主标题 (大字)
// subtitle: 副标题 (小字)
void showStatus(String title, String subtitle = "") {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println(title);
  
  if(subtitle != "") {
    display.setCursor(0, 16);
    display.println(subtitle);
  }
  display.display();
  Serial.println(title + " " + subtitle); // 同时打印到串口方便调试
}

// ================= I2S 初始化 =================
// 配置 I2S 接口，分别用于麦克风输入 (I2S_NUM_0) 和扬声器输出 (I2S_NUM_1)
void i2s_install() {
  // --- 配置麦克风 (I2S0) ---
  i2s_config_t i2s_config_mic = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // 主机模式，接收数据
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,        // 16位采样精度
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,         // 单声道 (左声道)
    .communication_format = I2S_COMM_FORMAT_I2S,         // 标准 I2S 格式
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,                                  // DMA 缓冲区数量
    .dma_buf_len = 64,                                   // 每个缓冲区长度
    .use_apll = false
  };
  i2s_driver_install(I2S_NUM_0, &i2s_config_mic, 0, NULL);
  
  i2s_pin_config_t pin_config_mic = {
    .bck_io_num = MIC_I2S_SCK,
    .ws_io_num = MIC_I2S_WS,
    .data_out_num = -1,            // 麦克风不需要输出引脚
    .data_in_num = MIC_I2S_SD      // 麦克风数据输入引脚
  };
  i2s_set_pin(I2S_NUM_0, &pin_config_mic);

  // --- 配置扬声器 (I2S1) ---
  i2s_config_t i2s_config_amp = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // 主机模式，发送数据
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };
  i2s_driver_install(I2S_NUM_1, &i2s_config_amp, 0, NULL);
  
  i2s_pin_config_t pin_config_amp = {
    .bck_io_num = AMP_I2S_BCLK,
    .ws_io_num = AMP_I2S_LRC,
    .data_out_num = AMP_I2S_DIN,   // 扬声器数据输出引脚
    .data_in_num = -1              // 扬声器不需要输入引脚
  };
  i2s_set_pin(I2S_NUM_1, &pin_config_amp);
}

// 创建 WAV 文件头
// PCM 音频数据是“裸数据”，需要加上这个 44 字节的头，播放器才能识别
void createWavHeader(uint8_t *header, int waveDataSize) {
  int sampleRate = SAMPLE_RATE;
  int byteRate = sampleRate * 2; // 16bit = 2bytes
  int totalDataLen = waveDataSize + 36;
  
  // RIFF Chunk
  header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
  header[4] = (byte)(totalDataLen & 0xFF);
  header[5] = (byte)((totalDataLen >> 8) & 0xFF);
  header[6] = (byte)((totalDataLen >> 16) & 0xFF);
  header[7] = (byte)((totalDataLen >> 24) & 0xFF);
  header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E';
  
  // fmt Chunk
  header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' ';
  header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0; // Chunk size: 16
  header[20] = 1; header[21] = 0; // Format code: 1 (PCM)
  header[22] = 1; header[23] = 0; // Channels: 1 (Mono)
  header[24] = (byte)(sampleRate & 0xFF);
  header[25] = (byte)((sampleRate >> 8) & 0xFF);
  header[26] = (byte)((sampleRate >> 16) & 0xFF);
  header[27] = (byte)((sampleRate >> 24) & 0xFF);
  header[28] = (byte)(byteRate & 0xFF);
  header[29] = (byte)((byteRate >> 8) & 0xFF);
  header[30] = (byte)((byteRate >> 16) & 0xFF);
  header[31] = (byte)((byteRate >> 24) & 0xFF);
  header[32] = 2; header[33] = 0; // Block align: 2 bytes
  header[34] = 16; header[35] = 0; // Bits per sample: 16
  
  // data Chunk
  header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a';
  header[40] = (byte)(waveDataSize & 0xFF);
  header[41] = (byte)((waveDataSize >> 8) & 0xFF);
  header[42] = (byte)((waveDataSize >> 16) & 0xFF);
  header[43] = (byte)((waveDataSize >> 24) & 0xFF);
}

// ================= 上传函数 =================
// 将录制好的音频数据通过 HTTP POST 上传到服务器
void performUpload() {
  // 1. 检查 WiFi 连接
  if (WiFi.status() != WL_CONNECTED) {
    showStatus("Error:", "WiFi Lost!");
    return;
  }
  
  // 2. 连接服务器
  showStatus("Connecting...", host);
  WiFiClient client;
  if (!client.connect(host, httpPort)) {
    showStatus("Error:", "Connect Fail"); // 连接失败
    return;
  }
  
  // 3. 准备上传
  showStatus("Uploading...", String(current_rec_len/1024) + " KB");

  // 构建 HTTP Multipart 表单数据
  String boundary = "ESP32Boundary"; // 分隔符
  String bodyHead = "--" + boundary + "\r\n";
  bodyHead += "Content-Disposition: form-data; name=\"audio_file\"; filename=\"s3_audio.wav\"\r\n";
  bodyHead += "Content-Type: audio/wav\r\n\r\n";
  String bodyTail = "\r\n--" + boundary + "--\r\n";

  // 计算总内容长度 = 头部 + WAV头(44字节) + 音频数据 + 尾部
  int wavHeaderSize = 44;
  int totalContentLen = bodyHead.length() + wavHeaderSize + current_rec_len + bodyTail.length();

  // 4. 发送 HTTP 请求头
  client.println("POST " + String(url) + " HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.print("Content-Length: "); client.println(totalContentLen);
  client.println(); // 空行表示请求头结束

  // 5. 发送数据体
  client.print(bodyHead); // 发送表单头
  
  uint8_t wavHeader[44];
  createWavHeader(wavHeader, current_rec_len); // 生成 WAV 头
  client.write(wavHeader, 44); // 发送 WAV 头

  // 分块发送音频数据 (防止一次发太大内存溢出)
  int chunkSize = 2048;
  int sent = 0;
  uint8_t *byteBuffer = (uint8_t *)rec_buffer; 
  while (sent < current_rec_len) {
    int toSend = (current_rec_len - sent) > chunkSize ? chunkSize : (current_rec_len - sent);
    client.write(byteBuffer + sent, toSend);
    sent += toSend;
  }
  client.print(bodyTail); // 发送表单尾部

  // 6. 等待服务器响应
  long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) { // 10秒超时
      showStatus("Error:", "Timeout");
      client.stop();
      return;
    }
  }
  
  client.stop(); // 断开连接
  showStatus("Success!", "Sent OK"); // 上传成功提示
  delay(1500); // 停留一下，让用户看到成功提示
}

// ================= 主逻辑任务 (RTOS 任务) =================
// 负责：待机 -> 录音 -> 处理 -> 回放 -> 上传 的全流程
void AudioTask(void *pvParameters) {
  // 1. 启动时先清空所有通知，防止误触发
  ulTaskNotifyTake(pdTRUE, 0);

  while (true) {
    // === 阶段 0: 待机 ===
    showStatus("Ready", "Press to Start");
    Serial.println("[Audio] Waiting for button...");
    
    // 阻塞等待开始信号 (RTOS 特性：没信号时挂起，不占 CPU)
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    
    // === 阶段 1: 录音中 ===
    Serial.println("[Audio] Start Recording");
    current_rec_len = 0;
    showStatus("Recording...", "Press to Stop");
    
    size_t bytes_read;
    int chunk = 1024;
    
    // 循环录音
    // 退出条件：收到停止信号(ulTaskNotifyTake > 0) 或者 内存存满了
    while((current_rec_len + chunk < record_size)) {
       // 检查是否有停止信号 (非阻塞检查)
       // 如果 ButtonTask 再次发来信号，说明用户第二次按下了按键，意图停止
       if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
          Serial.println("[Audio] Stop signal received");
          break; // 跳出循环，结束录音
       }

       // 从麦克风读取数据存入 rec_buffer
       i2s_read(I2S_NUM_0, (uint8_t*)rec_buffer + current_rec_len, chunk, &bytes_read, portMAX_DELAY);
       current_rec_len += bytes_read;
    }
    
    // === 阶段 2: 处理 ===
    showStatus("Processing...", String(current_rec_len) + " bytes");
    vTaskDelay(pdMS_TO_TICKS(500)); // 稍作延时

    // === 阶段 3: 本地回放 ===
    // 让你听到刚才录了什么，确认录音正常
    showStatus("Playing...", "Listen...");
    size_t bytes_written;
    int sent = 0;
    while(sent < current_rec_len) {
       int toSend = (current_rec_len - sent) > chunk ? chunk : (current_rec_len - sent);
       // 将数据写入扬声器 I2S 接口
       i2s_write(I2S_NUM_1, (uint8_t*)rec_buffer + sent, toSend, &bytes_written, portMAX_DELAY);
       sent += toSend;
    }
    
    // === 阶段 4: 上传 ===
    performUpload();
    
    // === 阶段 5: 复位 ===
    // 关键：上传完后，再次清空可能积累的误触信号，确保回到 Ready 状态是干净的
    ulTaskNotifyTake(pdTRUE, 0);
    Serial.println("[Audio] Cycle done, resetting...");
  }
}

// ================= 按键扫描任务 (RTOS 任务) =================
// 负责：实时检测按键，消除抖动，向主任务发送信号
void ButtonTask(void *pvParameters) {
  // 1. 启动延时：给系统一点时间稳定，防止上电抖动
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  // 2. 状态校验：如果上电时按键就是按下的，等待它松开，防止开机误触
  while(digitalRead(PIN_RECORD_BTN) == LOW) {
     vTaskDelay(pdMS_TO_TICKS(100));
  }

  // 读取初始状态
  bool lastState = digitalRead(PIN_RECORD_BTN);
  
  while (true) {
    bool currentState = digitalRead(PIN_RECORD_BTN);
    
    // 检测按下瞬间 (状态由 HIGH 变为 LOW)
    // 假设按键接了上拉电阻，按下为 LOW
    if (lastState == HIGH && currentState == LOW) {
      vTaskDelay(pdMS_TO_TICKS(50)); // 消抖延时
      if(digitalRead(PIN_RECORD_BTN) == LOW) {
         Serial.println("[Btn] Pressed!");
         
         // 发送信号给 AudioTask
         // 如果 AudioTask 在睡觉(待机)，它会醒来开始录音
         // 如果 AudioTask 在录音，它会收到信号停止录音
         xTaskNotifyGive(RecordTaskHandle);
         
         // 等待松开，防止一次按下被识别为多次
         while(digitalRead(PIN_RECORD_BTN) == LOW) {
            vTaskDelay(pdMS_TO_TICKS(50));
         }
      }
    }
    
    lastState = currentState;
    vTaskDelay(pdMS_TO_TICKS(50)); // 扫描频率 50ms
  }
}

// ================= Setup 初始化 =================
void setup() {
  Serial.begin(115200);
  
  // 1. 初始化 OLED 屏幕
  Wire.begin(SCREEN_SDA, SCREEN_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  showStatus("Booting...");

  // 2. 内存分配 (PSRAM 检测)
  // 如果有 PSRAM (外部RAM)，可以录制更长时间
  if(psramFound()){
    record_size = SAMPLE_RATE * 2 * MAX_RECORD_SEC;
    rec_buffer = (int16_t *)ps_malloc(record_size); // 申请 PSRAM
    Serial.println("PSRAM OK");
  } else {
    // 如果没有 PSRAM，只能录短一点，防止内存溢出
    record_size = SAMPLE_RATE * 2 * 4; 
    rec_buffer = (int16_t *)malloc(record_size);    // 申请内部 RAM
    Serial.println("No PSRAM");
  }
  
  // 内存申请失败处理
  if (rec_buffer == NULL) {
    showStatus("Mem Error!");
    while(1);
  }

  // 3. 初始化按键和 I2S
  pinMode(PIN_RECORD_BTN, INPUT_PULLUP);
  i2s_install();

  // 4. 连接 WiFi
  showStatus("Connecting WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // 5. 创建 RTOS 任务
  // xTaskCreatePinnedToCore(任务函数, 任务名, 栈大小, 参数, 优先级, 句柄, 核心ID)
  // AudioTask 优先级高 (3)，确保录音不卡顿，运行在核心 1
  xTaskCreatePinnedToCore(AudioTask, "AudioTask", 10240, NULL, 3, &RecordTaskHandle, 1);
  // ButtonTask 优先级中 (2)，运行在核心 1
  xTaskCreatePinnedToCore(ButtonTask, "BtnTask", 2048, NULL, 2, NULL, 1);
}

// ================= Loop =================
void loop() {
  // 主循环空闲，因为任务都由 RTOS 调度管理
  vTaskDelay(pdMS_TO_TICKS(1000));
}