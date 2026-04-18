import serial
import serial.tools.list_ports
import time
from ctypes import cast, POINTER
from comtypes import CLSCTX_ALL
from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume

def get_volume_control():
    try:
        from pycaw.utils import AudioUtilities
        devices = AudioUtilities.GetSpeakers()
        interface = devices.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
        return cast(interface, POINTER(IAudioEndpointVolume))
    except:
        try:
            device_enumerator = AudioUtilities.GetDeviceEnumerator()
            devices = device_enumerator.GetDefaultAudioEndpoint(0, 1)
            interface = devices.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
            return cast(interface, POINTER(IAudioEndpointVolume))
        except Exception as e:
            print(f"❌ ไม่สามารถเข้าถึงระบบเสียงได้: {e}")
            return None

def select_port():
    ports = serial.tools.list_ports.comports()
    if not ports: return None
    print("\n=== รายชื่อพอร์ตที่พบ ===")
    for i, port in enumerate(ports):
        print(f"{i}: {port.device} - {port.description}")
    try:
        return ports[int(input("\n👉 เลือกหมายเลขพอร์ต: "))].device
    except: return None

# --- เริ่มทำงานหลัก ---
selected_com = select_port()
if selected_com:
    try:
        # 1. เปิด Serial Port
        ser = serial.Serial(selected_com, 115200, timeout=0.1)
        ser.setDTR(True)
        ser.setRTS(True)
        # time.sleep(0.5) # รอให้ ESP32 บูตเสร็จ
        
        while True:
            if ser.in_waiting > 0:
                # อ่านค่าและเช็คว่าเป็นคำว่า READY หรือไม่
                msg = ser.readline().decode('utf-8', errors='ignore').strip().upper()
                if msg == "READY":
                    print("🚀 ESP32 พร้อมแล้ว! (หลุดจากลูป Ready)")
                    break # เจอ READY แล้ว ให้หยุดรอและไปทำขั้นตอนถัดไป
            time.sleep(0.02)
        
        volume_control = get_volume_control()

        if volume_control:
            print(f"✅ เชื่อมต่อ {selected_com} สำเร็จ")
            
            # --- ฟังก์ชันส่งค่าเริ่มต้น (First Connection Feedback) ---
            initial_vol_float = volume_control.GetMasterVolumeLevelScalar()
            initial_vol_int = int(initial_vol_float * 100)
            
            first_msg = f"R{initial_vol_int}\n"
            ser.write(first_msg.encode('utf-8')) # ส่งค่าความดังปัจจุบันไปหา ESP32 ทันที 1 ครั้ง
            
            last_reported_vol = initial_vol_int
            print(f"🚀 ส่งค่าเริ่มต้นสำเร็จ: {first_msg.strip()}")
            print("--- ระบบ Two-way พร้อมใช้งาน (V=รับ / R=ส่งกลับ) ---")
            
            last_send_time = time.time()
            
            esp32_move = False

            while True:
                # ตรวจสอบการปรับเสียงจากคอม (ส่ง Feedback ตลอดการใช้งาน)
                # current_vol_float = volume_control.GetMasterVolumeLevelScalar()
                # current_vol_int = int(current_vol_float * 100)

                # if esp32_move == False:
                # if abs(current_vol_int - last_reported_vol) != 0:
                #     feedback_msg = f"R{current_vol_int}\n"
                #     ser.write(feedback_msg.encode('utf-8'))
                #     last_reported_vol = current_vol_int
                #     # print(f"🔄 Feedback: {feedback_msg.strip()}")
                #     print(feedback_msg.strip())
                
                # current_time = time.time()
                # if current_time - last_send_time >= 0.5:
                #     feedback_msg = f"R{current_vol_int}\n"
                #     ser.write(feedback_msg.encode('utf-8'))
                #     last_reported_vol = current_vol_int
                #     print(f"🔄 Feedback: {feedback_msg.strip()}")
                #     last_send_time = time.time()

                # ตรวจสอบการรับค่าจาก ESP32 (V0-V100)
                if ser.in_waiting > 0:
                    raw_data = ser.readline().decode('utf-8', errors='ignore').strip()
                    command = raw_data.upper()

                    if command.startswith('V'):
                        try:
                            vol_percent = int(command[1:])
                            vol_percent = max(0, min(100, vol_percent))
                            volume_control.SetMasterVolumeLevelScalar(vol_percent / 100.0, None)
                            last_reported_vol = vol_percent
                            current_vol_int = vol_percent
                            # print(f"{current_vol_int}:{last_reported_vol} 1")
                            print(f"==> {vol_percent}%")
                        except ValueError:
                            pass
                
                # ตรวจสอบการปรับเสียงจากคอม (ส่ง Feedback ตลอดการใช้งาน)
                current_vol_float = volume_control.GetMasterVolumeLevelScalar()
                current_vol_int = int(round(current_vol_float * 100))
                
                # print(f"{current_vol_int}:{last_reported_vol} 2")
                
                # ส่งไปหา esp32
                if current_vol_int != last_reported_vol:
                # elif abs(current_vol_int - last_reported_vol) != 0:
                    feedback_msg = f"R{current_vol_int}\n"
                    ser.write(feedback_msg.encode('utf-8'))
                    last_reported_vol = current_vol_int
                    # print(f"🔄 Feedback: {feedback_msg.strip()}")
                    # print(feedback_msg.strip())
                    print(f"<== {current_vol_int}%")
                time.sleep(0.1)
                    
    except Exception as e:
        print(f"❌ Error: {e}")
    except KeyboardInterrupt:
        print("\n👋 ปิดโปรแกรม")
        ser.close()