#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <IRremote.h>
#include <DHT.h>
#include <Keypad.h>
#include "SafeState.h"
#include <Adafruit_Sensor.h>
#include <DHT_U.h>

// Pin and Servo Position Definitions
#define SERVO_PIN 12
#define SERVO_LOCK_POS 0
#define SERVO_UNLOCK_POS 90
#define DHTPIN 16
#define DHTTYPE DHT22
#define PIN_RECEIVER 24

// Global Variables
bool showTemAndHum = false;
bool firstTimePrintInlock = false;
bool firstTimePrintInUnlock = false;
int currentRemoteLedState = 0;
String pass = "";
bool isTyping = false;
int length = 0;

// Servo, Display, IR Receiver, DHT, Keypad
Servo lockServo;
LiquidCrystal_I2C lcd(0x27, 20, 4);
IRrecv receiver(PIN_RECEIVER);

DHT_Unified dht(DHTPIN, DHTTYPE);
SafeState safeState;

const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 3;
char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}};
byte rowPins[KEYPAD_ROWS] = {9, 8, 7, 6};
byte colPins[KEYPAD_COLS] = {5, 4, 3};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);

// Custom Characters for Display
uint8_t dot[8] = {0b00000, 0b00000, 0b01010, 0b11111, 0b11111, 0b01110, 0b00100, 0b00000};
// USER KEY
String userKey = "0000";
// Utility Functions
void lock()
{
  lockServo.write(SERVO_LOCK_POS);
  safeState.lock();
  firstTimePrintInlock = true;
  firstTimePrintInUnlock = false;
}

void unlock()
{
  lockServo.write(SERVO_UNLOCK_POS);
  firstTimePrintInlock = false;
  firstTimePrintInUnlock = true;
}

void showBackMessage()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("____Welcome back____");
  delay(500);
  lcd.setCursor(6, 3);
  String message = "CT060202";
  for (char c : message)
  {
    lcd.print(c);
    delay(100);
  }
  delay(1000);
}

String inputSecretCode(int i)
{
  lcd.setCursor(8, i);
  lcd.print("[____]");
  lcd.setCursor(9, i);

  String result;

  while (result.length() < 4)
  {
    char key = keypad.getKey();
    if (key >= '0' && key <= '9')
    {
      lcd.print('*');
      result += key;
    }
  }
  return result;
}

bool setNewCode()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter new code:");
  String newCode = inputSecretCode(1);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Confirm new code");
  String confirmCode = inputSecretCode(1);

  if (newCode.equals(confirmCode))
  {
    safeState.setCode(newCode);
    return true;
  }
  else
  {
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("Code mismatch");
    lcd.setCursor(0, 1);
    lcd.print("Safe not locked!");
    delay(2000);
    return false;
  }
}
void showWaitScreen()
{
  lcd.clear();                 // Xóa màn hình
  lcd.setCursor(0, 0);         // Đặt con trỏ tại vị trí bắt đầu
  lcd.print("Please Wait..."); // Hiển thị thông điệp "Please Wait..."

  // Bạn có thể hiển thị một biểu tượng chờ hoặc hoạt ảnh nếu muốn, ví dụ:
  lcd.createChar(0, dot); // Tạo ký tự dot (dấu chấm)
  for (int i = 0; i < 4; i++)
  {
    lcd.setCursor(i, 1); // Đặt con trỏ tại vị trí dòng 1, cột từ 0 đến 3
    lcd.write(byte(0));  // In dấu chấm
    delay(500);          // Đợi 500ms
    lcd.setCursor(i, 1); // Đặt con trỏ lại
    lcd.print(" ");      // Xóa dấu chấm
  }

  // Sau khi hoàn thành, bạn có thể xóa màn hình hoặc chuyển sang một thông điệp khác
  lcd.clear(); // Xóa màn hình sau khi hiển thị
}

void safeUnlockedLogic()
{
  if (firstTimePrintInUnlock)
  {
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print("Door Unlocked!");
    lcd.setCursor(0, 1);
    lcd.print("PRESS # to lock");
    lcd.setCursor(0, 2);
    lcd.print("PRESS * to CP");
    firstTimePrintInUnlock = false;
  }

  // Run until one of the conditions is met

  char key = keypad.getKey();

  // Check for keypad key press
  if (key == '#')
  {
    lcd.clear();
    lock();
    showWaitScreen();
  }

  if (key == '*')
  {
    bool setNewPassSuccess = setNewCode();
    if (setNewPassSuccess)
    {
      lcd.clear();
      lock();
      showWaitScreen();
    }
    else
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Failed to change pass");
      firstTimePrintInUnlock = true;
      delay(1000); // Wait for 1 second before exiting the loop
    }
  }
}

void showOnLockScreen()
{
  // Biến để xác định lần đầu in nội dung
  if (firstTimePrintInlock)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Locked!");
    firstTimePrintInlock = false;
  }

  // Điều kiện kiểm tra xem có hiển thị nhiệt độ và độ ẩm hay không
  if (showTemAndHum)
  {
    sensors_event_t event;

    // Lấy nhiệt độ từ cảm biến DHT
    dht.temperature().getEvent(&event);
    float currentTemperature = event.temperature;

    // Lấy độ ẩm từ cảm biến DHT
    dht.humidity().getEvent(&event);
    float currentHumidity = event.relative_humidity;
    lcd.setCursor(0, 1);
    lcd.print("Temp: ");
    if (!isnan(currentTemperature))
    {
      lcd.print(currentTemperature);
      lcd.print(" *C ");
    }
    else
    {
      lcd.print("N/A");
    }

    lcd.setCursor(0, 2);
    lcd.print("Humidity: ");
    if (!isnan(currentHumidity))
    {
      lcd.print(currentHumidity);
      lcd.print(" % ");
    }
    else
    {
      lcd.print("N/A");
    }
  }
}

void safeLockedLogic()
{
  showOnLockScreen();
  if (keypad.getKey())
  {
    String userCode = inputSecretCode(0);
    bool unlockedSuccessfully = safeState.unlock(userCode);
    showWaitScreen();

    // Nếu mã đúng, mở khóa
    if (unlockedSuccessfully)
    {
      showBackMessage(); // Hiển thị thông điệp chào mừng
      unlock();          // Mở khóa cửa
    }
    else
    {
      lcd.clear();
      lcd.setCursor(0, 0); // Đặt con trỏ ở dòng 1 để hiển thị thông báo "Access Denied!"
      lcd.print("Access Denied!");
      delay(1000);
      firstTimePrintInlock = true;
    }
  }
}

void setup()
{
  lcd.begin(16, 2);
  Serial.begin(115200);

  // Initialize Devices and Pins
  lockServo.attach(SERVO_PIN);
  pinMode(22, OUTPUT);

  // Initialize DHT Sensor
  dht.begin();

  // Initialize Safe State
  safeState.setCode(userKey);
  lock();

  showBackMessage();
  lcd.clear();
  receiver.enableIRIn();
}

char returnStringNumber(int number)
{
  switch (number)
  {
  case 104:
    return '0';
  case 48:
    return '1';
  case 24:
    return '2';
  case 122:
    return '3';
  case 16:
    return '4';
  case 56:
    return '5';
  case 90:
    return '6';
  case 66:
    return '7';
  case 74:
    return '8';
  case 82:
    return '9';
  default:
    return '*';
  }
}

void handleRemote()
{
  if (receiver.decode()) // Make sure you're using IrReceiver, not 'receiver'
  {
    switch (receiver.decodedIRData.command) // Use 'receiver' for access to the IR data
    {
    case 176:
      currentRemoteLedState = !currentRemoteLedState;
      digitalWrite(22, currentRemoteLedState);
      break;
    case 168:
      showTemAndHum = !showTemAndHum;
      firstTimePrintInlock = true;
      break;
    case 162:
      if (!safeState.locked())
      {
        lcd.clear();
        lock();
        showWaitScreen();
      }
      else
      {
        lcd.setCursor(8, 0);
        lcd.print("[____]");
        lcd.setCursor(9, 0);

        pass = "";
        isTyping = true;
        length = 0;
      }
      break;
    default:
      if (!isTyping)
        break;
      char key = returnStringNumber(receiver.decodedIRData.command);
      length++;
      if (key >= '0' && key <= '9')
      {
        lcd.print('*');
        pass += key;
      }
      if (length == 4)
      {
        bool unlockedSuccessfully = safeState.unlock(pass);
        showWaitScreen();

        // Nếu mã đúng, mở khóa
        if (unlockedSuccessfully)
        {
          showBackMessage(); // Hiển thị thông điệp chào mừng
          unlock();          // Mở khóa cửa
        }
        else
        {
          lcd.clear();
          lcd.setCursor(0, 0); // Đặt con trỏ ở dòng 1 để hiển thị thông báo "Access Denied!"
          lcd.print("Access Denied!");
          delay(1000);
          firstTimePrintInlock = true;
        }
      }
      break;
    }

    receiver.resume(); // Always call resume to reset the receiver for the next signal
  }
}

void loop()
{

  if (safeState.locked())
    // logic lúc đóng
    safeLockedLogic();
  else
    // logic lúc mở
    safeUnlockedLogic();
  handleRemote();
}
