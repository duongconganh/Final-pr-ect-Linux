Cách thực thi mã nguồn:
 - Biên dịch với make
 - Chạy chương trình Gateway với địa chỉ port kèm theo để khởi tạo Socket, ví dụ: ./gateway 1234 .
 - Chạy Chương trình Sensor với địa chỉ Gateway adress cùng Port của Gatewysensor và địa chỉ port tương ứng với ID room của Sensor,
   các ID room từ 1 -> 10 tương ứng với các port từ 2001 -> 2010.
   VD: ./sensor 192.168.1.103 1234 1 or ./sensor 192.168.1.103 1234 45678.
    
Các yêu cầu đã và chưa thực hiện được:

- 1 : Khởi tạo 1 tiến trình con để đọc log và ghi và file: gateway.log.
- 2 : Tạo ra 3 thread thực hiện 3 khối công việc khác nhau bao gồm Storage Manager, Data Manager và Connection Manager.
- 3 : Tạo kết nối giữa các senser với gateway sensor thông qua Socket stream(TCP).
- 4 : Có thể ghi dữ liệu và data shared với một cấu trúc dữ liệu Buffer circle.
- 5 : Ghi các log như nóng và lạnh vào file log.
- 6 : Chưa thực hiên. Kết nối, tạo và ghi dữ liệu và database.
- 7 : Sử dụng FIFO để giao tiếp giữa tiến trình cha và con,đảm bảo an toàn dữ liệu giữa nhiều nguồn có thể ghi dữ liệu từ tiến trình cha với khóa mutex.
- 8 : Ghi các log theo dạng cấu trúc <sequence number> <timestamp> <log-event info message>  vào gateway.log.
- 9 : Chua hoàn thành ghi log của SQL.

Hạn chế của mã nguồn:
- Code chưa được clean, chưa chia các file hoàn chỉnh và sử dụng staticlib hay sharedlib.
- Chưa comment rõ ràng.
- Chưa thực hiện yêu cầu hết nối với cơ sở dữ liệu SQLite.
- Code vẫn còn nhiều hạn chế.
