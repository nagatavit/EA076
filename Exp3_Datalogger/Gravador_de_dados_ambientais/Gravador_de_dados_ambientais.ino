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
#define thermo A0
#define BUFFER_SIZE 100

#define minDebounceTime 1

// Variaveis do teclado
int linVect[4] = {keyPinL1, keyPinL2, keyPinL3, keyPinL4};
int colVect[3] = {keyPinC1, keyPinC2, keyPinC3};
char keyValue[4][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}, {10, 11, 12}};
//enum {NULO, RESET, MEASURE, STATUS, INIT_MEAS, END_MEAS, TRANSFER, SETE, OITO, NOVE, CONFIRMA, ZERO, CANCELA} keypadState;
enum {OCIOSO, SAMPLING, TRANSFERING, PRINT, MEMREAD} dataloggerState, dataloggerStateNext;

//
char flagTimer = 0;
int count = 0, i = 0, buffer_data[BUFFER_SIZE], thermometerRead, memoryread[2048];
volatile unsigned const long int BASE_TEMPO_TIMER = 50; // base de tempo para calculo de delays (50 ms)
byte palavra;
Adafruit_PCD8544 display = Adafruit_PCD8544(DC_display, CS_display, reset_display); //criacao do objeto display

// ----------------------------------
//          INICIALIZACOES
// ----------------------------------

/*
   Inicializacao do display NOKIA 5110
*/

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

/*
   Inicializacao do teclado matricial
*/

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

// ----------------------------------
//         FUNCOES AUXILIARES
// ----------------------------------

/*
   Funcao de contagem por interrupcao
*/

void cronometro() {
  count++;
  if ((count % 20) == 0) {
    flagTimer = 1;
  }
}

/*
   Pooling de leitura do teclado matricial
*/

int poolingCollums(int _linha) {
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

/*
   Leitura e debounce do teclado matricial
*/

int poolingLines() {
  int _coluna = 3, _linha = 4;
  static int buttonStateReading = 0, buttonStateOld = 0, buttonStateNew = 0;
  static int debounceTime;

  for (_linha = 0; _linha < 4; _linha ++) {
    _coluna = poolingCollums(_linha);
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

  return (buttonStateNew);
}

/*
   Funcao de retorno da memoria usada no
*/

int usedSpace() {
  unsigned int aux[2], _i = 0;

  Wire.beginTransmission(0b1010111); // transmit to 24C16 (e paginacao 111)
  Wire.write(0xFE);
  Wire.endTransmission();

  Wire.requestFrom(0b1010111, 2);

  if (Wire.available()) {
    for (_i = 0; _i < 2; _i++)
      aux[_i] = Wire.read();    // receive a byte as character
  }

  //Serial.println(aux[0]);
  //Serial.println(aux[1]);

  return (2 * (256 * aux[0] + aux[1]));
}
void updateusedSpace() {
  unsigned int aux = usedSpace() + i;
  Wire.beginTransmission(0b1010111); // transmit to 24C16 (e paginacao 111)  
  Wire.write(0xFE);  
  Wire.write((aux & 0xFF00) >> 8);
  Wire.write(aux & 0xFF);
  Wire.endTransmission();
}
//function
void transferingData() {
  static int counter = 0;
  int spaceTemp = usedSpace();
  char newPage = 0;
  while (counter < i) {
    Wire.beginTransmission((0b1010000 | ((spaceTemp & 0x700) >> 8))); // transmit to 24C16 (e paginacao 111)
    Wire.write(spaceTemp & 0xFF);
    while (!((spaceTemp - 1) & 0xF) || newPage) {
      newPage = 0;
      Wire.write(buffer_data[counter]);
      counter += 1;
      spaceTemp++;
    }
    newPage = 1;
    Wire.endTransmission();
  }
  i = 0;
  counter = 0;
  updateusedSpace();
}

// ----------------------------------
//                SETUP
// ----------------------------------

void setup() {
  Wire.begin();
  Serial.begin(9600);
  initDisplay();
  initKeyboard();

  // Contador para interrupcao: base de tempo de 50 ms
  Timer3.initialize(BASE_TEMPO_TIMER * 1000); //time in us
  Timer3.attachInterrupt(cronometro);

  dataloggerState = OCIOSO;
  analogReference(INTERNAL1V1);
}

// ----------------------------------
//            LOOP PRINCIPAL
// ----------------------------------

void loop() {
  int _keyPressed, aux;
  unsigned int asd = usedSpace();

  _keyPressed =  poolingLines();

  if (_keyPressed == 1) {
    Wire.beginTransmission(0b1010111); // transmit to 24C16 (e paginacao 111)
    Wire.write(0xFE);
    Wire.write(0x00);
    Wire.write(0x00);
    Wire.endTransmission();
  }

  if (_keyPressed == 2) {
    Wire.beginTransmission(0b1010111); // transmit to 24C16 (e paginacao 111)
    Wire.write(0xFE);
    Wire.write(0xFF);
    Wire.write(0xFF);
    Wire.endTransmission();
  }

  //  Wire.beginTransmission(0b1010111); // transmit to 24C16 (e paginacao 111)
  //  Wire.write(0xFD);
  //  Wire.endTransmission();

  //Wire.requestFrom(0b1010111);
  /*
    if (flagTimer) {
      unsigned int teste = usedSpace();
      //Serial.println(teste,BIN);
      //Serial.println("--------------");

      flagTimer = 0;
    }*/

  // Maquina de estados do datalogger

  switch (dataloggerState) {
    case OCIOSO:
      Serial.println("OCIOSO");
      thermometerRead = analogRead(thermo) * 110 / 102.3;
      if (_keyPressed == 4)
        dataloggerStateNext = SAMPLING;
      else if (_keyPressed == 6 && i != 0)
        dataloggerStateNext = TRANSFERING;
      else if (_keyPressed == 7)
        dataloggerStateNext = PRINT;
      else if (_keyPressed == 11)
        dataloggerStateNext = MEMREAD;
      break;
    case SAMPLING:
      Serial.println("COLETANDO DADOS");
      if (flagTimer) {
        if (i < BUFFER_SIZE) {
          aux = analogRead(thermo) * 110 / 102.3;
          buffer_data[i] = (aux & 0xFF00) >> 8;
          buffer_data[i + 1] = aux & 0xFF;
          i += 2;
          flagTimer = 0;
        }
      }
      if (_keyPressed == 5)
        dataloggerStateNext = OCIOSO;
      break;
    case TRANSFERING:
      Serial.println("TRANSFERINDO DADOS");
      transferingData();
      break;
    case PRINT:
      for (int j = 0; j < i; j++)
        Serial.println(buffer_data[j]);
      dataloggerState = OCIOSO;
      dataloggerStateNext = OCIOSO;
      break;
    case MEMREAD:
      Wire.beginTransmission(0b1010000); // transmit to 24C16 (e paginacao 111)
      Wire.write(0x00);
      Wire.endTransmission();
      Wire.requestFrom(0b1010111, 2);
      if (Wire.available()) {
        
        for (int a = 0; a < asd; a++)
          memoryread[a] = Wire.read();    // receive a byte as character
      }

      for (int a = 0; a < asd; a++){
        Serial.println(memoryread[a]);
      }
      break;
  }

  if (dataloggerState != dataloggerStateNext) {
    Serial.println("PRESSIONE CONFIRMAR OU CANCELAR");
    if (_keyPressed == 10)
      dataloggerStateNext = dataloggerState;
    else if (_keyPressed == 12)
      dataloggerState = dataloggerStateNext;
  }
  Serial.print("STATE ");
  Serial.println(dataloggerState);
}
