#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <TimerThree.h>
#include <Wire.h>

#define clk_display 52
#define DIN_display 51
#define DC_display 47
#define CS_display 45
#define reset_display 43

#define keyPinL1 9
#define keyPinL2 8
#define keyPinL3 7
#define keyPinL4 6
#define keyPinC1 5
#define keyPinC2 4
#define keyPinC3 3

#define minDebounceTime 1

// Variaveis do teclado
int linVect[4] = {keyPinL1, keyPinL2, keyPinL3, keyPinL4};
int colVect[3] = {keyPinC1, keyPinC2, keyPinC3};
char keyValue[4][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}, {10, 11, 12}};
enum {NULO, RESET, MEASURE, STATUS, INIT_MEAS, END_MEAS, TRANSFER, SETE, OITO, NOVE, CONFIRMA, ZERO, CANCELA} keypadState;

//
int count = 0, debounceTime;
int buttonStateReading = 0, buttonStateOld = 0, buttonStateNew = 0;
volatile unsigned const long int BASE_TEMPO_TIMER = 50; // base de tempo para calculo de delays (50 ms)
byte palavra;
Adafruit_PCD8544 display = Adafruit_PCD8544(DC_display, CS_display, reset_display); //criacao do objeto display

void initDisplay() {
  pinMode(clk_display, OUTPUT);
  pinMode(DIN_display, OUTPUT);
  pinMode(DC_display, OUTPUT);
  pinMode(CS_display, OUTPUT);
  pinMode(reset_display, OUTPUT);

  display.begin();
  display.setContrast(50);
  display.clearDisplay();   // limpa buffer e tela
  display.setTextSize(1); //tamanho das letras na tela
}

void cronometro() {
  count++;
}

void initKeyboard() {
  pinMode(keyPinC1, INPUT);
  pinMode(keyPinC2, INPUT);
  pinMode(keyPinC3, INPUT);
  pinMode(keyPinL1, OUTPUT);
  pinMode(keyPinL2, OUTPUT);
  pinMode(keyPinL3, OUTPUT);
  pinMode(keyPinL4, OUTPUT);

  digitalWrite(keyPinL1, HIGH);
  digitalWrite(keyPinL2, HIGH);
  digitalWrite(keyPinL3, HIGH);
  digitalWrite(keyPinL4, HIGH);
}

int poolingKey(int _linha) {
  int _coluna;

  digitalWrite(linVect[_linha], LOW);
  for (int i = 0; i < 3; i++) {
    _coluna = digitalRead(colVect[i]);
    if (_coluna == 0) {
      digitalWrite(linVect[_linha], HIGH);
      return (i);
    }
  }
  digitalWrite(linVect[_linha], HIGH);
  return 3;
}

void setup() {
  Wire.begin();
  Serial.begin(9600);
  initDisplay();
  initKeyboard();

  // Contador para interrupcao: base de tempo de 50 ms
  Timer3.initialize(BASE_TEMPO_TIMER * 1000); //tempo em us
  Timer3.attachInterrupt(cronometro);

  keypadState = NULO;
}

void loop() {
  int _coluna = 3, _linha = 4;

  // Leitura e debouncing do teclado matricial

  for (_linha = 0; _linha < 4; _linha ++) {
    _coluna = poolingKey(_linha);
    if (_coluna < 3)
      break;
  }

  buttonStateReading = keyValue[_linha][_coluna];

  if (buttonStateReading != buttonStateOld) {
    debounceTime = count;
  }

  if ((count - debounceTime) > minDebounceTime) {
    if (buttonStateNew != buttonStateOld) {
      buttonStateNew = buttonStateReading;
    }
  }

  buttonStateOld = buttonStateReading;

  switch (buttonStateNew) {
    case 1
        keypadState = RESET;
      break;
    case 2
        keypadState = MEASURE;
      break;
    case 3
        keypadState = STATUS;
      break;
    case 4
        keypadState = INIT_MEAS;
      break;
    case 5
        keypadState = END_MEAS;
      break;
    case 6
        keypadState = TRANSFER;
      break;
    case 10
        keypadState = CONFIRMA;
      break;
    case 12
        keypadState = CANCELA;
      break;
  }

  switch (keypadState) {
    case NULO


        break;

    case

        break;
  }


  delay(1000);

  Wire.beginTransmission(0b0001010000); // transmit to 24C16
  Wire.write("x is ");        // sends five bytes
  Wire.write(x);              // sends one byte
  Wire.endTransmission();    // stop transmitting

  x++;
  delay(500);

}
