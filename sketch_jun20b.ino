
/*
 * 0.2 서박사 프로그램 테스트 
 * 0.1 Coblo test
 */

#include <SoftwareSerial.h>
#include <Thread.h>
#include <ThreadController.h>
#include <LED.h>
#include <Button.h>
#include <Wire.h>

/*
 * R: read sensor
 * C: calibration
 * T: check status(close/open)
 * B: blink on
 * N: blink off
 * O: led on
 * F: led off
 * 
 * 
 */

// protocol
#define GET 1
#define RUN 2
#define RESET 4
#define COBLO_DEVICE 0xde
#define FUNCTION_BLOCK 0x7a
#define ADD     0
#define START   1
#define FUNCTION 2
#define STOP  3
#define EXT 0
#define SENSOR_NUM 6
enum WIRETYPE {
  LEDON,
  LEDOFF,
  BLINKON,
  BLINKOFF,
  READSENSOR,
  CALIBRATION,
  CHECKSTATUS,
};

#define FORWARD_BLOCK 0x01
#define TURNLEFT_BLOCK 0x02
#define TURNRIGHT_BLOCK 0x03

const int pinStartButton = 2; // 푸시버튼 연결 핀 번호
const int pinStartLED = 9;
const int pinFunctButton = -1; //
const int pinFunctLED = 10;
const int pinStepsButton = -1;
const int pinStepsLED = 11;

boolean isAvailable = false;
char    serialRead;
unsigned char prevc = 0;  // real data
byte    index = 0;        // buffer index
char    buffer[52];       // real buffer 
boolean isStart = false;
byte    dataLen = 0;
int     ledswitch = 0;
boolean ledEnabled = false;
uint8_t sensor_address[SENSOR_NUM] = {10, 11, 12, 13, 14, 15};
uint8_t sensor_status[SENSOR_NUM] =  {0x4f, 0x4f, 0x4f, 0x4f, 0x4f, 0x4f};
uint8_t rgbCode[SENSOR_NUM][3] = {0};
SoftwareSerial bluetooth(1, 0);
ThreadController controll = ThreadController();
Thread worker1 = Thread();
Thread worker2 = Thread(); 
LED startLED = LED(pinStartLED);
LED functLED = LED(pinFunctLED);
LED stepsLED = LED(pinStepsLED);
Button startButton = Button(pinStartButton);
Button functButton = Button(pinFunctButton);
Button stepsButton = Button(pinStepsButton);

// ready sensor
boolean onlyOne = false;
unsigned long msLastTick;
uint8_t functionID = 0;
/*
 * ff, 55,  len,  idx,  act,  dev,  id,  task
 *  0,  1,   2,    3,     4,   5,   6,    7
*/
void runModule(int device){
  int id = readBuffer(6);
  int task = readBuffer(7);
      
  switch(device){
    case 1: // echo
      setLED(id, BLINKON);
      if(id == 14 || id == 15)
        functLED.on();
      break;
    case 2: // complete
      
      if(task == 0x7a){
        functionID = id;
      }else{
        setLED(id, BLINKOFF);
        setLED(id, LEDON);
      }
      break;
    case 3: // all complete
      setAllLED(LEDOFF);
      functLED.off();
      if(functionID != 0){
        setLED(functionID, BLINKOFF);
      }
      break;
    case 4: // function complete
      functLED.off();
      setLED(14, LEDOFF);
      setLED(15, LEDOFF);
      setLED(functionID, BLINKOFF);
      setLED(functionID, LEDON);
      break;
    default:
      break;
  }
}
// led blinking
void callback1(){
  if(ledEnabled){
    if(++ledswitch % 2){
      startLED.on();
    }else{
      startLED.off();
    }
  }else{
    startLED.off();
  }
}
// led on/off
void callback2(){ // for test
  ledEnabled = !ledEnabled;
}

void setup() {
  bluetooth.begin( 9600 );
  /*
  worker1.onRun(callback1);
  worker1.setInterval(500);

  worker2.onRun(callback2);
  worker2.setInterval(2000);

  controll.add(&worker1);
  //controll.add(&worker2);
  */
  Wire.begin();
  //Serial.begin(9600);
  msLastTick = millis();     
}

void loop() {
  unsigned long msTick = millis() - msLastTick;  
  if( msTick >= 1000 && !onlyOne){
    setSensor(READSENSOR);
    readSensor();
    startLED.on();
    stepsLED.on();
    onlyOne = true;
  }
  controll.run();
  readSerial();
  if(isAvailable){
    unsigned char c = serialRead & 0xff;
    if(c == 0x55 && isStart == false){
      if(prevc == 0xff){
        index = 1;
        isStart = true;
      }
    }else{  // isn't 0x55
      prevc = c;
      if(isStart){  // finded header code
        if(index == 2){ // is index length?
          dataLen = c;
        }else if(index > 2){
          dataLen--;
        }
        writeBuffer(index, c);    // real value
      }
    }

    index++;
    if(index > 51){ // checking max buffer
      index = 0;
      isStart = false;
    }

    if(isStart && dataLen == 0 && index > 3){
      isStart = false;    // reset
      parseData();
      index = 0;
    }
  }
  if(startButton.uniquePress()){   
    setSensor(CHECKSTATUS);
    readStatus(); 

    delay(500);
    setSensorByCheck();
    readSensor();    
    
    stopSignal();

    for(uint8_t i = 0; i < SENSOR_NUM; i++){
      if(sensor_status[i] == 0x43){
        uint8_t id = rgbCode[i][0];
        uint8_t sensor1 = (rgbCode[i][2] & 0x0f)+0x30;
        uint8_t sensor2 = (rgbCode[i][2]>>4 & 0x0f)+0x30; 
        uint8_t sensor3 = (rgbCode[i][1] & 0x0f) + 0x30;
        blockSignal(id, sensor1, sensor2, sensor3);
      }      
    }  
    
    startSignal();
  }
}
void parseData(){
  int idx = readBuffer(3);
  int action = readBuffer(4);
  int device = readBuffer(5);

  switch(action){
    case GET:     
    break;
    case RUN:
      runModule(device);
    break;
    case RESET:   
    break;
  }
}
void readSerial(){
  isAvailable = false;
  if(bluetooth.available() > 0){
    isAvailable = true;
    serialRead = bluetooth.read();
  }
}

void writeBuffer(int idx, unsigned char c){
  buffer[idx] = c;
}
unsigned char readBuffer(int idx){
  return buffer[idx];
}
void writeHead(){
  writeSerial(0xff);
  writeSerial(0x55);
}
void writeEnd(){
  bluetooth.println();
}
void writeSerial(unsigned char c){
  bluetooth.write(c);
}
void callOK(){
  writeHead();
  writeEnd();
}


void stopSignal(){
  writeHead(); 
  writeSerial(COBLO_DEVICE);  // device
  writeSerial(EXT);     // ext
  writeSerial(STOP);
  writeSerial(0);  
  writeSerial(0); 
  writeSerial(0); 
  writeSerial(0); 
  writeSerial(0);
  writeEnd();
  delay(200);
}
void startSignal(){
  writeHead(); 
  writeSerial(COBLO_DEVICE); 
  writeSerial(EXT);
  writeSerial(START);  
  writeSerial(0); 
  writeSerial(0); 
  writeSerial(0); 
  writeSerial(0); 
  writeSerial(0);
  writeEnd();
  delay(200);
}
void blockSignal(int id, uint8_t sen1, uint8_t sen2, uint8_t sen3){
  writeHead();
  writeSerial(COBLO_DEVICE); 
  writeSerial(EXT);
  writeSerial(ADD);  
  writeSerial(id); 
  writeSerial(sen1); 
  writeSerial(sen2); 
  writeSerial(sen3);
  writeEnd();
  delay(5);
}

void setLED(int id, int type){
  uint8_t value;
  if(type == LEDON){
    value = 0x4f; // 'O'
  }
  else if(type == LEDOFF){
    value = 0x46;   // 'F'
  }
  else if(type == BLINKON){
    value = 0x42;    // 'B'
  }
  else if(type == BLINKOFF){
    value = 0x4e;    // 'N'
  }
  Wire.beginTransmission(id);    
  Wire.write(value);
  Wire.endTransmission();
  delay(10);
}

void setAllLED(int type){
  uint8_t value;
  if(type == LEDON){
    value = 0x4f; // 'O'
  }
  else if(type == LEDOFF){
    value = 0x46;   // 'F'
  }
  else if(type == BLINKON){
    value = 0x42;    // 'B'
  }
  else if(type == BLINKOFF){
    value = 0x4e;    // 'N'
  }
  for(uint8_t i = 0; i < SENSOR_NUM; i++){
    Wire.beginTransmission(sensor_address[i]);
    Wire.write(value);
    Wire.endTransmission();
    delay(10);
  }
}
void setSensorByCheck(){
  for(uint8_t i = 0; i < SENSOR_NUM; i++){
    if(sensor_status[i] == 0x43){
      Wire.beginTransmission(sensor_address[i]);
      Wire.write(0x52);
      Wire.endTransmission();
      delay(10);
    }    
  }
}
void setSensor(int type){
  uint8_t value;
  if(type == READSENSOR){
    value = 0x52; // 'R'
  }
  else if(type == CALIBRATION){
    value = 0x43;   // 'C'
  }
  else if(type == CHECKSTATUS){
    value = 0x54;    // 'T'
  }
  for(uint8_t i = 0; i < SENSOR_NUM; i++){
    Wire.beginTransmission(sensor_address[i]);
    Wire.write(value);
    Wire.endTransmission();
    delay(10);
  }
}

// 항상 상태 점검후 실행 한다 
// 블록이 없는 홀은 bypass
void readSensor(){
  uint8_t bytes = 3;
  for(uint8_t i = 0; i < SENSOR_NUM; i++){
    if(sensor_status[i] == 0x43){
      Wire.requestFrom(sensor_address[i], bytes); // blocked function
      if(Wire.available() >= bytes) {
          rgbCode[i][0] = Wire.read();
          rgbCode[i][1] = Wire.read();
          rgbCode[i][2] = Wire.read();      
          uint8_t id = rgbCode[i][0];
          uint8_t sensor1 = (rgbCode[i][2] & 0x0f)+0x30;
          uint8_t sensor2 = (rgbCode[i][2]>>4 & 0x0f)+0x30; 
          uint8_t sensor3 = (rgbCode[i][1] & 0x0f) + 0x30;
          Serial.print("id:");
          Serial.print(id);
          Serial.print(",");
          Serial.print(sensor1);
          Serial.print(",");
          Serial.print(sensor2);
          Serial.print(",");
          Serial.print(sensor3);
          Serial.println("");
        }
      }
   }
}

void readStatus(){
  uint8_t bytes = 2;
  for(uint8_t i = 0; i < SENSOR_NUM; i++){
    Wire.requestFrom(sensor_address[i], bytes); // blocked function
    if(Wire.available() >= bytes) {
        uint8_t id = Wire.read();
        sensor_status[i] = Wire.read();        
        Serial.print("id:");
        Serial.print(id);
        Serial.print(",");
        Serial.print(sensor_status[i]);
        Serial.println("");
      }
  }
}



