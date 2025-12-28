#include <Arduino.h>
#include <driver/i2s.h>        // ESP32 专用的 I2S 音频驱动库
#include <Wire.h>              // Arduino 标准 I2C 通信库
#include <Adafruit_GFX.h>      // Adafruit 图形库核心（画点、线、圆等）
#include <Adafruit_SSD1306.h>  // SSD1306 OLED 屏幕驱动库

// ==========================================
//               硬件引脚定义
// ==========================================

// --- 麦克风 INMP441 (I2S 输入) ---
// I2S 是一种专门传输音频数据的协议，比模拟信号更抗干扰
#define MIC_I2S_WS   4    // 字选择 (Word Select)，用于区分左右声道
#define MIC_I2S_SCK  5    // 串行时钟 (Serial Clock)
#define MIC_I2S_SD   6    // 串行数据 (Serial Data)，麦克风的数据从这进来

// --- 功放 MAX98357A (I2S 输出) ---
#define AMP_I2S_LRC  16   // 左右时钟 (Left/Right Clock)，同 WS
#define AMP_I2S_BCLK 15   // 位时钟 (Bit Clock)
#define AMP_I2S_DIN  7    // 数据输入 (Data In)，ESP32 把数据发给功放

// --- 音量控制按键 ---
// 注意：ESP32 的某些引脚（如34-39）是仅输入的，没有内部上拉电阻。
// 如果你的板子上没有外部上拉电阻，按键可能无法稳定工作。
#define PIN_VOL_UP   39   // 音量加
#define PIN_VOL_DOWN 40   // 音量减

// --- OLED 屏幕 (0.91寸 SSD1306) ---
// AI小智通常使用 ESP32-S3，SDA/SCL 可以任意映射
#define SCREEN_SDA   41   // I2C 数据线
#define SCREEN_SCL   42   // I2C 时钟线
#define SCREEN_WIDTH 128  // 屏幕宽度像素
#define SCREEN_HEIGHT 32  // 屏幕高度像素
#define OLED_RESET   -1   // 复位引脚，-1 表示通过软件复位，不占用IO
#define SCREEN_ADDRESS 0x3C // I2C 地址，通常是 0x3C，少部分是 0x3D

// 创建屏幕对象，后续所有屏幕操作都通过 display 变量调用
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==========================================
//               音频与系统参数
// ==========================================

// 采样率 16000Hz：人声范围通常在 300Hz-3400Hz，16k 足够清晰且占用资源少
#define SAMPLE_RATE     16000
// DMA 缓冲区设置：
// 缓冲区数量越多，抗卡顿能力越强，但延迟越高；
// 单个缓冲区越长，中断次数越少，但延迟越高。
// 8 * 128 是一个平衡延迟和稳定性的配置。
#define DMA_BUF_COUNT   8
#define DMA_BUF_LEN     128
// 总处理缓冲区大小
#define BUFFER_SIZE     1024

int16_t *audio_buffer; // 音频数据指针，指向一块动态分配的内存

// ==========================================
//               音量控制逻辑
// ==========================================

int volume_level = 5;         // 当前音量档位 (0-9)，默认开机是 5
float current_gain = 4.0;     // 当前数字增益倍数

// 增益表：人耳对音量的感知是非线性的，所以我们用查表法
// 0档静音，随着档位增加，放大的倍数越来越大
const float gain_table[10] = {
    0.0f,  0.5f,  1.0f,  2.0f,  3.0f, 
    4.0f,  6.0f,  8.0f,  12.0f, 16.0f
};

// ==========================================
//               硬件初始化函数
// ==========================================

// --- 初始化麦克风 (I2S1 端口) ---
void init_mic() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // 主机模式，接收数据(RX)
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,        // 16位深度，CD音质也是16位
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,         // 只取左声道数据
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,   // 标准 I2S 格式
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,            // 中断优先级
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false                                    // 不使用高精度时钟，省电
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = MIC_I2S_SCK,
        .ws_io_num = MIC_I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE, // 麦克风不需要输出引脚
        .data_in_num = MIC_I2S_SD          // 数据从这里读入
    };
    // 安装驱动并设置引脚
    i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_1, &pin_config);
}

// --- 初始化功放 (I2S0 端口) ---
void init_speaker() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // 主机模式，发送数据(TX)
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUF_COUNT,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = AMP_I2S_BCLK,
        .ws_io_num = AMP_I2S_LRC,
        .data_out_num = AMP_I2S_DIN,       // 数据从这里发出去
        .data_in_num = I2S_PIN_NO_CHANGE   // 功放不需要输入引脚
    };
    // 安装驱动并设置引脚
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

// ==========================================
//               核心音频任务
// ==========================================
// 这是一个 FreeRTOS 任务，独立于 loop() 运行，专门负责搬运音频数据
void audio_pass_through_task(void *pvParameters) {
    size_t bytes_read = 0;
    size_t bytes_written = 0;
    int32_t processed_sample = 0; // 用 32位整数暂存计算结果，防止计算溢出

    Serial.println("[Audio] 实时透传任务启动...");

    while (true) {
        // 1. 从麦克风读取数据 (I2S_NUM_1)
        // portMAX_DELAY 表示如果没有数据，程序会在这里一直等，不会往下跑
        i2s_read(I2S_NUM_1, audio_buffer, BUFFER_SIZE * sizeof(int16_t), &bytes_read, portMAX_DELAY);

        // 获取当前的音量放大倍数
        float gain = current_gain; 

        // 2. 数字信号处理 (DSP) - 调整音量
        // bytes_read 是字节数，每个样本是 16位(2字节)，所以循环次数要除以 2
        for(int i = 0; i < bytes_read / 2; i++) {
            // 核心算法：原始声音 * 放大倍数
            processed_sample = (int32_t)(audio_buffer[i] * gain);

            // 3. 防止破音 (Clipping)
            // 16位音频的最大值是 32767，最小值是 -32768
            // 如果放大后超过这个范围，声音会通过“削顶”产生刺耳的爆音
            // 所以必须强制限制在这个范围内
            if (processed_sample > 32767) processed_sample = 32767;
            else if (processed_sample < -32768) processed_sample = -32768;

            // 将处理后的 32位数据转回 16位存回去
            audio_buffer[i] = (int16_t)processed_sample;
        }

        // 4. 将处理好的数据写入功放 (I2S_NUM_0)
        i2s_write(I2S_NUM_0, audio_buffer, bytes_read, &bytes_written, portMAX_DELAY);
    }
}

// ==========================================
//               辅助功能函数 (UI更新)
// ==========================================

// --- 刷新 OLED 屏幕显示 ---
void refresh_oled_display() {
    display.clearDisplay(); // 清除显存，准备画新的一帧
    
    // 1. 显示文字标题
    display.setTextSize(1);              // 字体大小 1
    display.setTextColor(SSD1306_WHITE); // 白色字
    display.setCursor(0, 0);             // 坐标原点在左上角 (0,0)
    display.print("Volume: ");
    display.print(volume_level);
    display.print(" / 9");

    // 2. 绘制音量进度条 (图形化)
    // drawRect 画空心框：x=0, y=12, 宽=128, 高=10
    display.drawRect(0, 12, 128, 10, SSD1306_WHITE);
    
    // 计算实心条的宽度
    // map函数将 0-9 的档位映射到 0-126 像素宽度
    int bar_width = map(volume_level, 0, 9, 0, 126);
    
    // fillRect 画实心矩形：用来表示当前进度
    if (bar_width > 0) {
        display.fillRect(2, 14, bar_width, 6, SSD1306_WHITE);
    }

    // 3. 显示增益倍数 (底部小字)
    display.setCursor(0, 24);
    display.print("Gain: ");
    display.print(current_gain, 1); // 保留1位小数
    display.print("x");

    // 非常重要！不调用 display() 屏幕是什么都不会显示的
    display.display(); 
}

// --- 更新音量 ---
void update_volume(int change) {
    int old_level = volume_level;
    volume_level += change; // 增加或减少音量

    // 限制范围在 0-9 之间
    if (volume_level > 9) volume_level = 9;
    if (volume_level < 0) volume_level = 0;

    // 只有音量真正改变了，或者强制刷新(change==0)时，才更新状态
    if (volume_level != old_level || change == 0) {
        // 从查找表中获取对应的放大倍数
        current_gain = gain_table[volume_level];

        // 1. 串口打印调试信息
        Serial.print("音量等级: ");
        Serial.print(volume_level);
        Serial.print(" (增益: ");
        Serial.print(current_gain);
        Serial.println(")");

        // 2. 刷新屏幕显示 UI
        refresh_oled_display();
    }
}

// ==========================================
//               Arduino Setup (初始化)
// ==========================================
void setup() {
    Serial.begin(115200); // 开启串口调试
    delay(1000);

    // --- 1. 初始化 I2C 和 OLED 屏幕 ---
    // ESP32 允许自定义 I2C 引脚，这里设置为 SDA=41, SCL=42
    Wire.begin(SCREEN_SDA, SCREEN_SCL);

    // SSD1306_SWITCHCAPVCC 表示内部产生 3.3V->7V 的显示电压
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed (屏幕初始化失败)"));
        // 如果屏幕坏了，这里不写 while(1)，让音频功能依然能工作
    } else {
        Serial.println(F("OLED Init Success!"));
        display.clearDisplay();
        display.display(); // 刚开机清屏
    }

    // --- 2. 硬件初始化 ---
    pinMode(PIN_VOL_UP, INPUT_PULLUP);   // 设置按键为输入上拉模式
    pinMode(PIN_VOL_DOWN, INPUT_PULLUP);

    // 申请内存用于存放音频数据
    // ps_malloc 优先从 PSRAM (外部伪静态随机存储器) 申请，这部分内存很大(几MB)
    audio_buffer = (int16_t *)ps_malloc(BUFFER_SIZE * sizeof(int16_t));
    if (audio_buffer == NULL) {
        // 如果没有 PSRAM (比如普通 ESP32)，则回退到内部 RAM 申请
        Serial.println("PSRAM 内存分配失败！尝试使用内部 RAM...");
        audio_buffer = (int16_t *)malloc(BUFFER_SIZE * sizeof(int16_t));
        // 如果内部 RAM 也不够，那就只能卡死在这里了
        if (audio_buffer == NULL) while (1);
    }

    // 初始化音频驱动
    init_mic();
    init_speaker();
    
    // 初始化并显示一次默认音量
    update_volume(0); 

    // --- 3. 创建音频任务 ---
    // xTaskCreatePinnedToCore: 创建一个 FreeRTOS 任务
    // "AudioTask": 任务名字
    // 8192: 栈大小 (字节)，音频处理需要较大栈空间
    // NULL: 参数
    // 5: 优先级 (很高)，保证音频不卡顿
    // NULL: 任务句柄
    // 0: 运行在 CPU 核心 0 上 (Arduino 的 loop 默认在核心 1)
    xTaskCreatePinnedToCore(audio_pass_through_task, "AudioTask", 8192, NULL, 5, NULL, 0);

    Serial.println("系统就绪。");
}

// ==========================================
//               Arduino Loop (主循环)
// ==========================================
unsigned long last_debounce_time = 0;
const unsigned long debounce_delay = 200; // 防抖时间 200ms

void loop() {
    // 读取按键状态 (因为是 INPUT_PULLUP，所以按下是 LOW)
    bool btn_up_pressed = (digitalRead(PIN_VOL_UP) == LOW);
    bool btn_down_pressed = (digitalRead(PIN_VOL_DOWN) == LOW);

    // 简单的按键防抖逻辑
    if ((millis() - last_debounce_time) > debounce_delay) {
        if (btn_up_pressed) {
            update_volume(1); // 音量 +1
            last_debounce_time = millis(); // 记录当前时间，防止连击
        } 
        else if (btn_down_pressed) {
            update_volume(-1); // 音量 -1
            last_debounce_time = millis();
        }
    }
    // 延时一小会儿，让出 CPU 给其他低优先级任务（比如 WiFi 后台任务）
    vTaskDelay(pdMS_TO_TICKS(50));
}
