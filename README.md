\# ĐỀ TÀI: VOICE CONTROL KHÓA ĐIỆN TỬ CỬA NHÀ

Môn học: Mạng cảm biến

Giảng viên hướng dẫn: Hồ Nhựt Minh

Sinh viên thực hiện: Trần Tiến Lộc

Mã số sinh viên: N23DCCI041

1. Cấu trúc thư mục dự án (Repository Structure)

```text
mangcambien-cuoiky-trantienloc/
├── Voice_Smart_Lock/
│   └── Voice_Smart_Lock.ino      # Mã nguồn chính (Arduino C++) chạy trên ESP32 NodeMCU
├── Edge_Impulse_Library/
│   └── tienloc_voice_control_inferencing.zip # Thư viện mô hình nén dạng C++/Arduino xuất từ Edge Impulse
└── README.md                     # Tài liệu hướng dẫn cài đặt, cấu trúc và vận hành hệ thống
 
2. Hướng dẫn cài đặt và Chạy chương trình

2.1 Các công cụ cần chuẩn bị
- Phần mềm Arduino IDE (Phiên bản gợi ý: 2.x.x trở lên).
- Thư viện lõi phần cứng: Đã cài đặt gói cấu hình board ESP32 by Espressif Systems trong mục Boards Manager của Arduino IDE.

2.2 Các bước thực hiện nạp code
- Cài đặt thư viện AI nhúng:
  + Mở phần mềm Arduino IDE => Vào thẻ Sketch => Include Library => Add .ZIP Library....
  + Tìm và chọn file tienloc_voice_control_inferencing.zip nằm trong thư mục Edge_Impulse_Library/ của kho lưu trữ này.

- Cài đặt các thư viện phụ thuộc (Dependencies):
  + Vào mục Library Manager (Trình quản lý thư viện) trên Arduino IDE, tìm kiếm và cài đặt chính xác 2 thư viện sau:Blynk (bởi Volodymyr Shymanskyy); LiquidCrystal_I2C (bởi Frank de Brabander)

- Cấu hình thông số mạng và Token mã nguồn:
  + Mở file Voice_Smart_Lock.ino bên trong thư mục Voice_Smart_Lock/.Chỉnh sửa các dòng đầu mã nguồn bằng thông tin Blynk cá nhân của bạn:
                    #define BLYNK_TEMPLATE_ID   "TMPL6hgHoa_q_"
                    #define BLYNK_TEMPLATE_NAME "Voice Smart Lock"
                    #define BLYNK_AUTH_TOKEN    "gTYSLl9XQ7oKTk10LOtKDxdofsT4Uy0k"

- Cấu hình thông số bộ định tuyến Wi-Fi thực địa tại vị trí đặt mạch:
                    char ssid[] = "Tên_WiFi_Của_Bạn";
                    char pass[] = "Mật_Khẩu_WiFi";

- Biên dịch và nạp chương trình:
  + Kết nối kit ESP32 với máy tính qua cáp Micro-USB.
  + Trên thanh công cụ Arduino IDE, chọn đúng cổng COM kết nối và tên mạch là ESP32 DevModule.
  + Nhấn nút Upload (Biểu tượng mũi tên chỉ sang phải ->) để hệ thống tiến hành biên dịch thuật toán và nạp code xuống phần cứng.
  
  2.3 Quy trình vận hành kiểm thử
  - Khởi động: Cấp nguồn cho mạch, màn hình LCD hiển thị Connecting WiFi.... Khi kết nối thành công với Cloud Blynk, còi kêu 1 tiếng bíp ngắn và LCD chuyển sang trạng thái chờ: He thong khoa / Dang lang nghe....

  - Điều khiển tại chỗ (Offline): Nói từ khóa "Tiến Lộc" vào micro INMP441 để kích hoạt trạng thái nhận lệnh (LCD hiển thị: Chao Tien Loc! / Moi ra lenh (5s)). Trong vòng 5 giây tiếp theo, nói "Mở khóa" để kích hoạt rơ-le co chốt Solenoid mở cửa.

  - Điều khiển từ xa (Online): Mở ứng dụng Blynk trên điện thoại, nhấn giữ nút Trợ lý giọng nói (chân V1) và nói "mở khóa". Lệnh sẽ được đồng bộ qua Wi-Fi xuống ESP32 để kích mở chốt vật lý tức thời.
