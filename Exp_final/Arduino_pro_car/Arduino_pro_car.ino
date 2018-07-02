//#include <SoftwareSerial.h>
#include <Ultrasonic.h>

#define pinUpDown1 8
#define pinUpDown2 7
#define motorPWM 9

#define pinLeftRight1 11
#define pinLeftRight2 12
#define directionPWM 10

#define pinTrigger 6
#define pinEcho 7

#define pino_trigger 3
#define pino_echo 4

//static enum {OCIOSO, SPEED, DIRECTION} carControlState;
static unsigned char fowardNotBackward, leftNotRight;
int aux;


char bluetoothParsing(long int * commandValue) {
  static String _auxStr = "";
  char _commandChar = 0, _auxChar;
  //  int _commandValue = 0;

  if (Serial.available()) {
    _auxChar = Serial.read();

    if (_auxChar != '*') {
      _auxStr = String(_auxStr + _auxChar);
    } else {
      _commandChar = _auxStr.charAt(0);
      _auxStr = _auxStr.substring(1);
      *commandValue = _auxStr.toInt();
      _auxStr = "";
    }
  }
  return (_commandChar);
}

void speedSelect(unsigned char dir, unsigned int value) {
  if (value == 0 ) {
    digitalWrite(pinUpDown1, HIGH);
    digitalWrite(pinUpDown2, HIGH);
  } else {
    digitalWrite(pinUpDown1, dir);
    digitalWrite(pinUpDown2, 1 ^ dir);
  }
  fowardNotBackward = dir;
  analogWrite(motorPWM, value);
  // Serial.write(value);
}

void directionSelect(unsigned char dir, unsigned int value) {
  if (value == 0) {
    digitalWrite(pinLeftRight1, HIGH);
    digitalWrite(pinLeftRight2, HIGH);
  } else {
    digitalWrite(pinLeftRight1, dir);
    digitalWrite(pinLeftRight2, 1 ^ dir);
  }
  analogWrite(directionPWM, value);
  //  Serial.write(value);
}

void setup() {
  Serial.begin(38400);

  //  fowardNotBackward = 1;

  pinMode(pinUpDown1, OUTPUT);
  pinMode(pinUpDown2, OUTPUT);
  pinMode(pinLeftRight1, OUTPUT);
  pinMode(pinLeftRight2, OUTPUT);
  pinMode(motorPWM, OUTPUT);
  pinMode(directionPWM, OUTPUT);

  digitalWrite(pinUpDown1, HIGH);
  digitalWrite(pinUpDown2, LOW);
  analogWrite(motorPWM, 0);

  digitalWrite(pinLeftRight1, LOW);
  digitalWrite(pinLeftRight2, LOW);
  analogWrite(directionPWM, 0);

  //carControlState = OCIOSO;

}

void loop() {
  static char opSelect = 0;
  static long int pwmValue = 0;
  static Ultrasonic ultrasom(pino_trigger, pino_echo);
  static long dist;
  static char flagDist = 0;

  dist = ultrasom.Ranging(CM);

  opSelect = bluetoothParsing(&pwmValue);

  if (dist < 30 && fowardNotBackward) {
    speedSelect(0, 127);
    flagDist = 1;
  } else if (dist >= 20 && flagDist ) {
    speedSelect(1, 0);
    flagDist = 0;
  }

  if (opSelect == 'F') {
    speedSelect(1, pwmValue);
  } else if (opSelect == 'B') {
    speedSelect(0, pwmValue);
  } else if (opSelect == 'L') {
    directionSelect(1, pwmValue);
  } else if (opSelect == 'R') {
    directionSelect(0, pwmValue);
  }
  opSelect = 0;
  pwmValue = 0;

  //  if (Serial.available())
  //    BTSerial.write(Serial.read());
  //    aux = Serial.read();

}
