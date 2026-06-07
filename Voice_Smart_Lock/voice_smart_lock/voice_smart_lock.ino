/**
 * DỰ ÁN: KHÓA CỬA ĐIỆN TỬ ĐIỀU KHIỂN GIỌNG NÓI SONG HÀNH (HYBRID SMART LOCK)
 * MÔN HỌC: MẠNG CẢM BIẾN
 * SINH VIÊN THỰC HIỆN: TRẦN TIẾN LỘC
 * MSSV: N23DCCI041
 * Lớp: D23CQCI01-N
 * * Mô tả: Mã nguồn tích hợp suy diễn Edge AI (Edge Impulse INT8) từ Micro I2S tại cửa
 * và nhận chuỗi ký tự chuyển đổi giọng nói (Speech-to-Text) từ xa qua Blynk IoT V1.
 */

// 1. CẤU HÌNH THÔNG TIN KẾT NỐI BLYNK CLOUD
#define BLYNK_TEMPLATE_ID   "TMPL6hgHoa_q_"
#define BLYNK_TEMPLATE_NAME "Voice Smart Lock"
#define BLYNK_AUTH_TOKEN    "gTYSLl9XQ7oKTk10LOtKDxdofsT4Uy0k"

// 2. KHAI BÁO CÁC THƯ VIỆN HỆ THỐNG
#include <tienloc_voice_control_inferencing.h> // Thư viện Edge Impulse
#include <driver/i2s.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

// 3. ĐỊNH NGHĨA THÔNG TIN CẤU HÌNH MẠNG WI-FI
char ssid[] = "Oppo A15s";
char pass[] = "khongbiet";

// 4. ĐỊNH NGHĨA CẤU HÌNH CHÂN NGOẠI VI (PINS CONFIGURATION)
const int RELAY_PIN = 26;  // Chân kích Module Relay 5V (Cách ly quang)
const int BUZZER_PIN = 27; // Chân phát tín hiệu âm thanh Còi chíp

// Khởi tạo màn hình hiển thị LCD 1602 qua giao tiếp I2C (SDA=21, SCL=22)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// 5. ĐỊNH NGHĨA MÁY TRẠNG THÁI HỮU HẠN (FSM) CHO BẢO MẬT BIÊN
enum State { IDLE, WAITING_FOR_COMMAND };
State currentState = IDLE;
unsigned long wakeWordTime = 0;
const unsigned long TIMEOUT_INTERVAL = 5000; // Cửa sổ 5 giây chờ nhận lệnh thực thi

// 6. CẤU HÌNH THÔNG SỐ ĐỆM CHO THƯ VIỆN EDGE IMPULSE CONTINUOUS
static signed short sampleBuffer[2048];

// ============================================================================
// KÊNH ĐIỀU KHIỂN 1: NHẬN LỆNH GIỌNG NÓI TỪ XA QUA ĐIỆN THOẠI (BLYNK VOICE ASSISTANT)
// ============================================================================
// Bộ chuyển đổi giọng nói trên Smartphone dịch thành Text và gửi về chân ảo V1
BLYNK_WRITE(V1) {
    String commandFromApp = param.asString();
    commandFromApp.toLowerCase(); // Chuẩn hóa chuỗi về chữ thường để so khớp
    
    Serial.print("[Blynk Voice Remote]: "); Serial.println(commandFromApp);
    
    // So khớp từ khóa không dấu và có dấu cơ bản từ điện thoại gửi về
    if (commandFromApp == "mở khóa" || commandFromApp == "mo khoa" || commandFromApp == "open") {
        Serial.println("-> Kích hoạt mở khóa qua kênh truyền thông không dây.");
        openDoor();
    } 
    else if (commandFromApp == "khóa" || commandFromApp == "khoa" || commandFromApp == "close") {
        Serial.println("-> Kích hoạt đóng khóa nhanh qua kênh truyền thông không dây.");
        closeDoor();
    }
}

// ============================================================================
// HÀM KHỞI TẠO CẤU HÌNH HỆ THỐNG (SETUP)
// ============================================================================
void setup() {
    // Khởi tạo cổng Serial để Debug và nạp chương trình
    Serial.begin(115200);
    
    // Cấu hình linh kiện chấp hành đầu ra
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW); // Mặc định trạng thái ban đầu là Khóa Đóng
    
    // Khởi động màn hình hiển thị LCD
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Connecting WiFi");
    lcd.setCursor(0, 1);
    lcd.print("Please wait...");

    // Kết nối Wi-Fi cục bộ và liên kết đồng bộ Blynk Cloud Server
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    
    // Khởi tạo phần cứng Micro số qua Bus I2S
    initI2S();
    
    // Đưa hệ thống về trạng thái sẵn sàng lắng nghe ban đầu
    showIdleScreen();
}

// ============================================================================
// VÒNG LẶP CHƯƠNG TRÌNH CHÍNH (LOOP)
// ============================================================================
void loop() {
    // Duy trì liên tục luồng truyền nhận gói tin TCP/IP với máy chủ Cloud Blynk
    Blynk.run(); 

    // Kiểm tra thời gian Timeout của lớp bảo mật thứ hai (Wake-word) tại chỗ
    if (currentState == WAITING_FOR_COMMAND && (millis() - wakeWordTime > TIMEOUT_INTERVAL)) {
        currentState = IDLE;
        showIdleScreen();
    }

    // ============================================================================
    // KÊNH ĐIỀU KHIỂN 2: XỬ LÝ SÓNG ÂM TẠI BIÊN QUA MICRO I2S VÀ MẠNG CNN
    // ============================================================================
    ei_impulse_result_t result = { 0 };
    
    // Gọi hàm trích xuất tính toán cửa sổ trượt liên tục từ thư viện Edge Impulse
    EI_IMPULSE_ERROR r = run_classifier_continuous(&result, false);
    if (r != EI_IMPULSE_OK) {
        return; // Nếu bộ đệm lỗi hoặc chưa đủ mẫu, bỏ qua chu kỳ này để nạp tiếp
    }

    // Tiến hành phân loại nhãn và kích hoạt logic điều khiển phần cứng
    handleLocalInferenceResult(result);
}

// ============================================================================
// HÀM XỬ LÝ LOGIC SUY DIỄN TINYML TẠI CHỖ
// ============================================================================
void handleLocalInferenceResult(ei_impulse_result_t result) {
    float max_score = 0;
    String d_label = "";
    
    // Duyệt qua ma trận kết quả phân loại để tìm nhãn có độ tự tin cao nhất
    for (size_t ix = 0; ix < EI_IMPULSE_LABEL_COUNT; ix++) {
        if (result.classification[ix].value > max_score) {
            max_score = result.classification[ix].value;
            d_label = result.classification[ix].label;
        }
    }

    // Cài đặt bộ lọc ngưỡng tin cậy bảo mật (Threshold > 85%) để chống nhiễu kích nhầm
    if (max_score < 0.85) return; 

    // --- MÁY TRẠNG THÁI LOGIC ĐIỀU KHIỂN ---
    if (d_label == "cảnh báo") {
        // ONLINE: Đẩy sự kiện cảnh báo đột nhập khẩn cấp ngay lập tức về Smartphone
        Blynk.logEvent("canh_bao_nguy_hiem", "Cảnh báo: Phát hiện khẩu lệnh CẢNH BÁO tại cảm biến cửa!");
        triggerAlert();
    } 
    else if (currentState == IDLE) {
        if (d_label == "tiến lộc") { // Kích hoạt đúng từ khóa lớp 1 (Wake-word)
            currentState = WAITING_FOR_COMMAND;
            wakeWordTime = millis(); // Bắt đầu tính mốc thời gian timeout 5s
            
            lcd.clear();
            lcd.print("Chao Tien Loc!");
            lcd.setCursor(0, 1);
            lcd.print("Moi ra lenh (5s)");
            
            // ONLINE: Cập nhật trạng thái an ninh lên Dashboard di động
            Blynk.virtualWrite(V2, "Cửa: Chờ nhận lệnh..."); 
            beep(1);
        }
    } 
    else if (currentState == WAITING_FOR_COMMAND) {
        if (d_label == "mở khóa") { // Đúng lệnh lớp 2
            openDoor();
            currentState = IDLE;
            showIdleScreen();
        } 
        else if (d_label == "khóa") {
            closeDoor();
            currentState = IDLE;
            showIdleScreen();
        }
    }
}

// ============================================================================
// CÁC HÀM TRỢ NĂNG ĐIỀU KHIỂN CHẤP HÀNH VÀ NGOẠI VI (HELPER FUNCTIONS)
// ============================================================================

// Hàm khởi động và cấu hình đường truyền DMA cho Micro I2S INMP441
void initI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // Chế độ Master nhận dữ liệu
        .sample_rate = 16000,                                 // Chuẩn tần số lấy mẫu giọng nói 16kHz
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,         // Độ phân giải mã PCM 16-bit
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,          // Kênh đơn (Mono) nối chân L/R xuống GND
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,   // Chuẩn giao tiếp Philips I2S
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,             // Mức ưu tiên ngắt tầng 1
        .dma_buf_count = 8,                                   // Số lượng khối đệm DMA liên hoàn
        .dma_buf_len = 64,                                    // Độ dài mỗi khối đệm dữ liệu
        .use_apll = false
    };
    
    // Định nghĩa cấu hình phần cứng sơ đồ chân GPIO kết nối thực tế
    i2s_pin_config_t pin_config = {
        .bck_io_num = 33,   // Chân SCK nối GPIO 33
        .ws_io_num = 25,    // Chân WS nối GPIO 25
        .data_out_num = -1, // Không dùng chân TX truyền dữ liệu ra
        .data_in_num = 32   // Chân SD nối GPIO 32
    };
    
    // Thực thi cài đặt driver của Espressif Systems vào nhân vi xử lý
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

// Hàm thực thi mở cửa vật lý
void openDoor() {
    lcd.clear();
    lcd.print("CUA DANG MO!");
    
    // ONLINE: Đồng bộ trạng thái thời gian thực lên giao diện Cloud
    Blynk.virtualWrite(V2, "Trạng thái: CỬA MỞ"); 
    
    digitalWrite(RELAY_PIN, HIGH); // Xuất mức cao đóng Relay cấp nguồn cho Solenoid co chốt
    beep(2);                       // Phát tín hiệu bíp bíp phản hồi thành công
    
    // Duy trì mở cửa tự động an toàn trong 5 giây, sau đó tự động kích hoạt đóng chốt
    delay(5000); 
    closeDoor();
}

// Hàm thực thi đóng khóa vật lý
void closeDoor() {
    digitalWrite(RELAY_PIN, LOW); // Ngắt điện, khóa Solenoid tự đẩy lò xo lao chốt ra đóng cửa
    
    lcd.clear();
    lcd.print("CUA DA KHOA!");
    
    // ONLINE: Đồng bộ trạng thái an toàn lên giao diện Cloud
    Blynk.virtualWrite(V2, "Trạng thái: CỬA KHÓA"); 
    
    beep(1);
    delay(1500); // Tạo trễ chống rung màn hình trước khi chuyển giao diện
}

// Hàm xử lý chuỗi còi hú báo động ngắt quãng liên tục
void triggerAlert() {
    lcd.clear();
    lcd.print("!!! CANH BAO !!!");
    for(int i = 0; i < 5; i++) {
        digitalWrite(BUZZER_PIN, HIGH); delay(100);
        digitalWrite(BUZZER_PIN, LOW);  delay(100);
    }
}

// Hàm hiển thị màn hình nền chờ đợi mặc định
void showIdleScreen() {
    lcd.clear();
    lcd.print("He thong khoa");
    lcd.setCursor(0, 1);
    lcd.print("Dang lang nghe...");
    
    // ONLINE: Đồng bộ trạng thái sẵn sàng lên điện thoại
    Blynk.virtualWrite(V2, "Hệ thống sẵn sàng");
}

// Hàm phát tín hiệu xung âm thanh ngắn qua còi báo
void beep(int times) {
    for(int i = 0; i < times; i++) {
        digitalWrite(BUZZER_PIN, HIGH); delay(150);
        digitalWrite(BUZZER_PIN, LOW);  delay(100);
    }
}