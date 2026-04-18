#include <Adafruit_NeoPixel.h>
#include <SimpleFOC.h>
#ifdef __AVR__
#include <avr/power.h>  // Required for 16 MHz Adafruit Trinket
#endif

BLDCMotor motor = BLDCMotor(7);
BLDCDriver3PWM driver = BLDCDriver3PWM(0, 1, 2, 3);
MagneticSensorI2C sensor = MagneticSensorI2C(AS5600_I2C);

#define PIN_LED 8  //
#define PIN_LED_RGB 7
#define PIN_OUT_1 0
#define PIN_OUT_2 1
#define PIN_OUT_3 2
#define PIN_EN_MOTOR 3

#define NeoPixel_Brightness 50  //max255
#define NUMPIXELS 16            //Popular NeoPixel ring size
Adafruit_NeoPixel pixels(NUMPIXELS, PIN_LED_RGB, NEO_GRB + NEO_KHZ800);

float num_detents = 100;
float detent_strength = 0.2;
float damping = 0.01;
float voltage_limit = 3.0;

unsigned long wait_time_reset_esp = 1000;  //ระยะเวลาที่ต้องการรอ
unsigned long last_time_reset_esp = 0;     //ประกาศตัวแปรเป็น global เพื่อเก็บค่าไว้ไม่ให้ reset จากการวนloop
unsigned long wait_time_RGB_boot_esp = 25;
unsigned long last_time_RGB_boot_esp = 0;

float main_volume = 0;
int last_main_volume = 0;
int RGB_NUM = 0;
int last_RGB_NUM = 0;
int Count_RGB_boot_esp = 0;

bool motor_update = true;

void setup() {
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif

  pixels.begin();
  pixels.setBrightness(NeoPixel_Brightness);

  Serial.begin(115200);

  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 255, 255));
  }
  pixels.show();

  Wire.setClock(400000);
  sensor.init();
  motor.linkSensor(&sensor);
  driver.voltage_power_supply = 13;
  driver.init();
  driver.enable();  //
  motor.linkDriver(&driver);

  // playMario();  //
  // playHBD();
  // playStarWars();
  // playCoffinDance();
  // playStartupCinematic();
  playStartupSciFi();
  // playStartupTech();

  driver.disable();

  motor.voltage_limit = voltage_limit;
  motor.controller = MotionControlType::torque;
  motor.LPF_velocity.Tf = 0.01;
  motor.P_angle.P = 20;        // แรงดีดเข้าหาตำแหน่งเป้าหมาย
  motor.PID_velocity.P = 0.1;  // ความหนืดขณะเคลื่อนที่
  motor.PID_velocity.I = 5;    // ช่วยให้ไปถึงเป้าหมายได้แม่นขึ้น
  motor.velocity_limit = 35;   // จำกัดความเร็วไม่ให้หมุนเร็วเกินไปจนน่ากลัว
  motor.init();

  last_time_RGB_boot_esp = millis();
  int C_BG[3] = { random(0, 255), random(0, 255), random(0, 255) };
  int C_VL[3] = { 255 - C_BG[0], 255 - C_BG[1], 255 - C_BG[2] };
  while (!Serial) {
    if (Count_RGB_boot_esp == NUMPIXELS) {
      Count_RGB_boot_esp = 0;
      for (int i = 0; i < 3; i++) {
        C_BG[i] = random(0, 255);
        C_VL[i] = (255 - C_BG[i]);
      }
    }

    if ((millis() - last_time_RGB_boot_esp) > wait_time_RGB_boot_esp) {
      for (int t = 0; t < NUMPIXELS; t++) {
        pixels.setPixelColor(t, pixels.Color(C_BG[0], C_BG[1], C_BG[2]));
      }
      pixels.setPixelColor(Count_RGB_boot_esp, pixels.Color(C_VL[0], C_VL[1], C_VL[2]));
      pixels.show();
      Count_RGB_boot_esp += 1;
      last_time_RGB_boot_esp = millis();
    }
  }

  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(255, 255, 255));
  }
  pixels.show();

  driver.enable();
  motor.initFOC();

  Serial.println("READY");
  last_time_reset_esp = millis();
}

void loop() {
  motor.loopFOC();

  float current_angle = motor.shaft_angle * -1;
  float detent_torque = -detent_strength * sin(motor.shaft_angle * num_detents);
  float damping_torque = -damping * motor.shaft_velocity;

  if (motor_update == false) {
    main_volume = mapFloat(current_angle, 0.0, float(2.0 * _PI), 0.0, num_detents);
    main_volume = constrain(main_volume, 0.0, num_detents);

    if (current_angle < -0.15) {
      motor.move(-3);
    } else if (current_angle > 6.5) {
      motor.move(3);
    } else {
      motor.move(detent_torque + damping_torque);
    }
  } else if (motor_update == true) {
    motor.controller = MotionControlType::angle;
    float TAGET_rad = mapFloat(main_volume, 0.0, 100.0, 0.0, float(2.0 * _PI));
    TAGET_rad = TAGET_rad * -1;
    motor.move(TAGET_rad);
    // Serial.println(String(TAGET_rad) + ":" + String(motor.shaft_angle));
    if ((int)(motor.shaft_angle * 100) == (int)(TAGET_rad * 100)) {
      motor.controller = MotionControlType::torque;
      motor_update = false;
    }
    last_main_volume = main_volume;
  }

  // if (!Serial) {
  //   esp_restart();
  // }
  // if ((millis() - last_time_reset_esp) > wait_time_reset_esp) {
  //   esp_restart();
  // }

  // static uint32_t last_print = 0;

  if (Serial.available() > 0 && motor_update == false) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.startsWith("R")) {
      // ตัดตัว R ออกแล้วแปลงเป็นตัวเลข
      int pcVolume = input.substring(1).toInt();
      main_volume = pcVolume;
    }
    LED_RGB_UPDATE(main_volume);
    if (main_volume != last_main_volume) {
      motor_update = true;
      // motor.controller = MotionControlType::angle;
    }

    last_main_volume = main_volume;
    last_time_reset_esp = millis();
  }

  static uint32_t last_print = 0;
  if (millis() - last_print > 100 && motor_update == false && (int)round(main_volume) != last_main_volume) {

    // float current_detent = mapFloat(current_angle, 0.0, float(2.0 * _PI), 0.0, num_detents);
    // current_detent = constrain(current_detent, 0.0, num_detents);

    // main_volume = mapFloat(current_angle, 0.0, float(2.0 * _PI), 0.0, num_detents);
    // main_volume = constrain(main_volume, 0.0, num_detents);

    // Serial.print("Angle: ");
    // Serial.print(current_angle);
    // Serial.print(" | Detent Index: ");
    // Serial.println((int)round(current_detent));

    // Serial.println(String(main_volume) + ":" + String(last_main_volume));

    main_volume = (int)round(main_volume);
    Serial.println("V" + String((int)round(main_volume)));
    LED_RGB_UPDATE(main_volume);

    last_main_volume = main_volume;
    last_print = millis();
  }
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void LED_RGB_UPDATE(int vol) {  //0-100
  int rgb_vol = map(vol, 0, 100, NUMPIXELS - 1, 0);
  if (vol == 0) {
    for (int i = 0; i < NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.Color(255, 255, 255));
    }
  } else {
    for (int i = 0; i < NUMPIXELS; i++) {
      pixels.setPixelColor(i, pixels.Color(0, 255, 0));
    }
    for (int i = 0; i < rgb_vol; i++) {
      pixels.setPixelColor(i, pixels.Color(255, 255, 255));
    }
  }

  pixels.show();
}

// void MOTOR_BLDC_UPDATE(int vol) {  //0-100
//   // motor.controller = MotionControlType::angle;

//   while (true) {
//     motor.loopFOC();
//     float angle = motor.shaft_angle * -1;

//     float target = mapFloat(angle, 0.0, 6.3, 0.0, num_detents);
//     target = constrain(target, 0.0, num_detents);
//     // target = target.toInt();

//     if (round(target) < vol) {
//       motor.move(-6);
//     } else if (round(target) > vol) {
//       motor.move(6);
//     } else if (round(target) == vol) {
//       break;
//     }
//   }

//   // motor.controller = MotionControlType::torque;
// }

void playTone(float frequency, int duration) {
  if (frequency <= 0) {
    driver.setPwm(0, 0, 0);
    delay(duration);
    return;
  }

  long startTime = millis();
  float phase = 0;
  float volume = 10.0;
  // float volume = voltage_limit;

  // คำนวณความเร็วในการเปลี่ยนเฟสตามความถี่ที่ต้องการ
  // สูตร: phase_increment = 2 * PI * frequency * delta_t
  float phase_increment = (2.0 * _PI * frequency) * 0.0001;  // 0.0001 มาจาก delay 100us

  while (millis() - startTime < duration) {
    // จ่ายแรงดันแบบ 3 เฟส ห่างกันเฟสละ 120 องศา
    float v_a = _sin(phase) * volume;
    float v_b = _sin(phase + 2.0944) * volume;  // 2.0944 คือ 2*PI/3
    float v_c = _sin(phase + 4.1888) * volume;  // 4.1888 คือ 4*PI/3

    driver.setPwm(v_a, v_b, v_c);

    phase += phase_increment;
    if (phase > _2PI) phase -= _2PI;

    delayMicroseconds(100);
  }
  driver.setPwm(0, 0, 0);
}

void playMario() {
  // ท่อนเปิดที่เป็นเอกลักษณ์
  playTone(660, 100);
  delay(150);
  playTone(660, 100);
  delay(300);
  playTone(660, 100);
  delay(300);
  playTone(510, 100);
  delay(100);
  playTone(660, 100);
  delay(300);
  playTone(770, 100);
  delay(550);
  playTone(380, 100);
  // delay(575);
}

void playHBD() {
  playTone(262, 200);
  delay(25);  // C4
  playTone(262, 200);
  delay(25);
  playTone(294, 400);
  delay(25);  // D4
  playTone(262, 400);
  delay(25);  // C4
  playTone(349, 400);
  delay(25);  // F4
  playTone(330, 800);
  // delay(500);  // E4
}

void playStarWars() {
  playTone(440, 500);
  delay(25);  // A4
  playTone(440, 500);
  delay(25);
  playTone(440, 500);
  delay(25);
  playTone(349, 350);
  delay(25);  // F4
  playTone(523, 150);
  delay(25);  // C5
  playTone(440, 500);
  delay(25);  // A4
  playTone(349, 350);
  delay(25);  // F4
  playTone(523, 150);
  delay(25);  // C5
  playTone(440, 650);
  // delay(500);
}

void playCoffinDance() {
  // ท่อนฮุค: ลา ลา ลา ลา...
  playTone(440, 150);
  delay(25);  // A4
  playTone(440, 150);
  delay(25);
  playTone(440, 150);
  delay(25);
  playTone(440, 150);
  delay(25);

  playTone(659, 250);
  delay(25);  // E5
  playTone(587, 250);
  delay(25);  // D5
  playTone(523, 250);
  delay(25);  // C5
  playTone(494, 250);
  delay(25);  // B4

  playTone(392, 150);
  delay(25);  // G4
  playTone(392, 150);
  delay(25);
  playTone(440, 300);
  // delay(200);  // A4
}

void playStartupCinematic() {
  playTone(392, 150);
  delay(50);  // ซอล
  playTone(523, 150);
  delay(50);  // โด
  playTone(659, 150);
  delay(50);           // มี
  playTone(784, 400);  // ซอล (สูง)
}

void playStartupSciFi() {
  playTone(523, 100);
  delay(20);  // โด
  playTone(659, 100);
  delay(20);  // มี
  playTone(784, 100);
  delay(20);            // ซอล
  playTone(1047, 300);  // โด (สูง)
}

void playStartupTech() {
  // กวาดความถี่จากต่ำไปแหลมอย่างรวดเร็ว
  for (int i = 200; i < 1200; i += 20) {
    playTone(i, 10);
  }
  delay(50);
  // ตบท้ายด้วยโน้ตสูงสองตัวสั้นๆ
  playTone(880, 100);
  delay(20);
  playTone(1047, 200);
}