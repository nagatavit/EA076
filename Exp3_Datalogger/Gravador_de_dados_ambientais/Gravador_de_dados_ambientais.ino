#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <TimerThree.h>
#include <Wire.h>

// ----------------------------------
//              DEFINES
// ----------------------------------

#define clk_display 52
#define DIN_display 51
#define DC_display 22
#define CS_display 24
#define reset_display 26

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

// ----------------------------------
//         VARIAVEIS GLOBAIS
// ----------------------------------

// Variaveis do teclado
int linVect[4] = {keyPinL1, keyPinL2, keyPinL3, keyPinL4};
int colVect[3] = {keyPinC1, keyPinC2, keyPinC3};
char keyValue[4][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}, {10, 11, 12}};

// Maquinas de estado
enum {OCIOSO, SAMPLING, TRANSFERING, PRINT, MEMREAD} dataloggerState, dataloggerStateNext; // estado do programa
enum {MEASURE, STATUS, COLETANDO, WRITING, CONFIRM} displayState, displayStateNext, displayStateOld; // estado do display NOKIA 5110

// Variaveis para base de tempo
volatile char flagTimer0 = 0, flagTimer1 = 0;
unsigned const long int BASE_TEMPO_TIMER = 50; // base de tempo para calculo de delays (50 ms)
int count = 0;

volatile int usedBuffer = 0, buffer_data[BUFFER_SIZE], thermometerRead, memoryread[2048];

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
  display.display();
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
   Base de tempo usando interrupcao (Timer3)
*/

void cronometro() {
  count++;
  if ((count % 20) == 0) {
    flagTimer0 = 1;
  }
  if ((count % 20) == 0) {
    flagTimer1 = 1;
  }
}

// ----------------------------------
//         TECLADO MATRICIAL
// ----------------------------------

/*
   Varredura das colunas do teclado matricial, para cada linha
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
   Varredura por linhas e debounce do teclado matricial
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

  buttonStateReading = keyValue[_linha][_coluna]; // Retorna Valor do teclado a partir da posicao da matriz

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

// ----------------------------------
//         MEMORIA 24C16
// ----------------------------------

/*
   Retorna a quantidade de memoria usada
*/

unsigned int usedSpace() {
  unsigned int _i = 0;
  unsigned long int ret;
  unsigned char aux[2];

  // Dummy Write para realizar acesso aleatorio a memoria
  Wire.beginTransmission(0b1010111); // transmit to 24C16 (e paginacao 111)
  Wire.write(0xFE); // Endereco 2046
  Wire.endTransmission();

  Wire.requestFrom(0b1010111, 2); // Le posicoes 2046 e 2047 que indicam o espaco usado na memoria

  if (Wire.available()) {
    for (_i = 0; _i < 2; _i++)
      aux[_i] = Wire.read();    // receive a byte as character
  }


  ret = (256 * aux[0] + aux[1]);

  return (ret); // Expressao para o espaco total usado
}

/*
   Atualiza o contador do uso de memoria
*/

void updateUsedSpace() {
  unsigned int aux = 0;

  aux =  usedSpace();

  aux = aux + usedBuffer;

  Wire.beginTransmission(0b1010111); // transmit to 24C16 (e paginacao 111)
  Wire.write(0xFE);
  Wire.write(aux  >> 8);
  Wire.write(aux & 0xFF);
  Wire.endTransmission();
}

/*
   Realiza a transferencia de dados do buffer do arduino
   para memoria 24c16
*/

void transferBlockData() {
  int counter = 0, aux;
  unsigned int spaceTemp = usedSpace();
  byte teste;

  while (counter < usedBuffer) {
    aux = (spaceTemp & 0x700) >> 8;
    aux = 0b1010000 | aux;
    Wire.beginTransmission(aux);

    aux = spaceTemp & 0xFF;
    Wire.write(aux);

    while (((spaceTemp & 0xF) != 0xF) && counter < usedBuffer) { // Realiza escrita ate fim da pagina
      Wire.write(buffer_data[counter]);
      counter += 1;
      spaceTemp++;
    }
    if (((spaceTemp & 0xF) == 0xF) && counter < usedBuffer) { // Escrita do ultimo end. da pagina
      Wire.write(buffer_data[counter]);
      counter += 1;
      spaceTemp++;
    }

    teste = Wire.endTransmission();
  }
  counter = 0;
}

// ----------------------------------
//         DISPLAY NOKIA 5110
// ----------------------------------



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

  analogReference(INTERNAL1V1);
  dataloggerState = OCIOSO;
  displayState = MEASURE;

}

// ----------------------------------
//            LOOP PRINCIPAL
// ----------------------------------

void loop() {
  int _keyPressed, aux, tempMeas;
  unsigned int usedData;

  _keyPressed =  poolingLines();

  if (_keyPressed == 1) {
    Wire.beginTransmission(0b1010111); // transmit to 24C16 (e paginacao 111)
    Wire.write(0xFE);
    Wire.write(0x00);
    Wire.write(0x00);
    Wire.endTransmission();
  }

  if (_keyPressed == 8) {
    Wire.beginTransmission(0b1010111); // transmit to 24C16 (e paginacao 111)
    Wire.write(0xFE);
    Wire.write(0xF0);
    Wire.write(0xF0);
    Wire.endTransmission();
  }

  // Maquina de estados do datalogger
  switch (dataloggerState) {
    case OCIOSO:
      // Le temperatura atual ((1100 mV / 10 mV) / 1023 bits) * 10 (decimo de grau)
      //    thermometerRead = analogRead(thermo) * 110 / 102.3;

      // Transicoes de estado
      if (_keyPressed == 4) // inicia coleta periodica
        dataloggerStateNext = SAMPLING;
      else if (_keyPressed == 6 ) // transfere dados para memoria && usedBuffer != 0
        dataloggerStateNext = TRANSFERING;
      /*      else if (_keyPressed == 7) // imprime buffer
              dataloggerStateNext = PRINT;
            else if (_keyPressed == 11) // le memoria
              dataloggerStateNext = MEMREAD; */
      break;

    case SAMPLING:

      if (flagTimer0) {
        if (usedBuffer < BUFFER_SIZE) {
          aux = analogRead(thermo) * 110 / 102.3;
          buffer_data[usedBuffer] = (aux & 0xFF00) >> 8;
          buffer_data[usedBuffer + 1] = aux & 0xFF;
          usedBuffer += 2;
          flagTimer0 = 0;
        }
      }

      // Transicao de estado
      if (_keyPressed == 5) // finaliza coleta periodica
        dataloggerStateNext = OCIOSO;
      break;

    case TRANSFERING:
      transferBlockData(); // Transfere dados do buffer para memoria
      delay(10);
      updateUsedSpace();
      usedBuffer = 0;

      // Transicao de estado
      dataloggerState = OCIOSO;
      dataloggerStateNext = OCIOSO;
      break;
      /*
         case PRINT:
           for (int j = 0; j < usedBuffer; j++)
             Serial.println(buffer_data[j]);
           dataloggerState = OCIOSO;
           dataloggerStateNext = OCIOSO;
           break;

        case MEMREAD:
           Wire.beginTransmission(0b1010000); // transmit to 24C16 (e paginacao 111)
           Wire.write(0x00);
           Wire.endTransmission();
           Wire.requestFrom(0b1010111, 2);*/
      /*
        if (Wire.available()) {
        for (int a = 0; a < asd; a++)
          memoryread[a] = Wire.read();    // receive a byte as character
        }

        for (int a = 0; a < asd; a++)
        Serial.println(memoryread[a]);

        break;*/
  }

  if (dataloggerState != dataloggerStateNext) {
    //   Serial.println("PRESSIONE CONFIRMAR OU CANCELAR");
    //  displayStateOld = displayState;
    //  displayState = CONFIRM;

    if (_keyPressed == 10) {
      dataloggerStateNext = dataloggerState;
      //    displayState = displayStateOld;
    } else if (_keyPressed == 12) {
      dataloggerState = dataloggerStateNext;
      //    displayState = displayStateOld;
    }

  }

  switch (displayState) {
    case MEASURE:
      tempMeas = analogRead(thermo) * 110 / 102.3;
      display.clearDisplay();
      display.println("MEASURING:");
      display.println("Current Temp: ");
      display.setCursor(22, 18);
      display.print(String(tempMeas / 10));
      display.print('.');
      display.print(String(tempMeas % 10));
      display.print((char)247);
      display.println("C");
      if (_keyPressed == 3)
        displayState = STATUS;
      break;

    case STATUS:
      usedData = usedSpace();
      display.clearDisplay();
      display.println("STATUS:");
      display.println("Used data: ");
      display.setCursor(20, 18);
      display.println(String(usedData));
      display.println("Data avbl:");
      display.setCursor(20, 38);
      display.println(String(usedBuffer));
      if (_keyPressed == 2)
        displayState = MEASURE;
      break;

    case COLETANDO:
      display.clearDisplay();
      display.println("Sampling data:");
      break;

    case WRITING:
      display.clearDisplay();
      display.println("Writing on");
      display.println("Memory");
      display.println("Please Wait");
      break;
    case CONFIRM:
      display.clearDisplay();
      display.println("CONFIRM?");
      display.println("# Sim");
      display.println("* Sim");
      break;
  }
  if (flagTimer1) {
    display.display();
    flagTimer1 = 0;
  }
}
