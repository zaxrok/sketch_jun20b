
/*
 * 0.1 Coblo test
 */

#include <SoftwareSerial.h>
#include <Thread.h>
#include <ThreadController.h>
#include <LED.h>
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

const int pinButton = 12; // 푸시버튼 연결 핀 번호
const int pinRGB_Red = 9;    // RGB LED의 Red 연결 핀 번호
const int pinRGB_Green = 10; // RGB LED의 Green 연결 핀 번호
const int pinRGB_Blue = 11;  // RGB LED의 Blue 연결 핀 번호
const int pinWhite = 3;
const int pinMic = A2;  // 마이크 연결 핀 번호

boolean isAvailable = false;
char    serialRead;
unsigned char prevc = 0;  // real data
byte    index = 0;        // buffer index
char    buffer[52];       // real buffer 
boolean isStart = false;
byte    dataLen = 0;
int     ledswitch = 0;
boolean ledEnabled = false;

SoftwareSerial bluetooth(1, 0);
ThreadController controll = ThreadController();
Thread worker1 = Thread();
Thread worker2 = Thread(); 
LED led = LED(pinRGB_Blue);


/*
 * ff, 55, len, idx, act, dev, typ, val
 *  0,  1,   2,   3,   4,   5,   6,   7
*/
void runModule(int device){
  int type = readBuffer(6);  
  int value = readBuffer(7);
      
  switch(device){
    case 1: // echo
      ledEnabled = true;
      break;
    case 2: // complete
      ledEnabled = false;
      break;
    default:
    break;
  }
}
// led blinking
void callback1(){
  if(ledEnabled){
    if(++ledswitch % 2){
      led.on();
    }else{
      led.off();
    }
  }else{
    led.off();
  }
}
// led on/off
void callback2(){ // for test
  ledEnabled = !ledEnabled;
}

void setup() {
  bluetooth.begin( 9600 );
  
  worker1.onRun(callback1);
  worker1.setInterval(500);

  worker2.onRun(callback2);
  worker2.setInterval(2000);

  controll.add(&worker1);
  controll.add(&worker2);
}

void loop() {
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
// 0~9, 0~9, 0~9
void color(int r, int g, int b){
  digitalWrite(pinRGB_Red, r*28);
  digitalWrite(pinRGB_Green, g*28);
  digitalWrite(pinRGB_Blue, b*28);

}
void ledoff(){
  digitalWrite(pinRGB_Red, LOW);   // 빨간색 켜기
  digitalWrite(pinRGB_Green, LOW); // 초록색 켜기
  digitalWrite(pinRGB_Blue, LOW);   // 파란색 끄기
}
 
