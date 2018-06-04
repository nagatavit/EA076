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
enum {OCIOSO, SAMPLING, TRANSFERING, CLEARMEM, MEMREAD, PRINT} dataloggerState, dataloggerStateNext; // estado do programa
enum {MEASURE, STATUS, CONFIRM, SERIALREAD} displayState, displayStateOld; // estado do display NOKIA 5110
String nextStateName[5] = {"Clear MEM.", "Init. Sampl.", "End. Sampl.", "Transfer Data", "Serial Trsf."};
String statusName[2] = {"Idle", "Splng"};
volatile int statusIdx, nextStateIdx; // Variaveis para indicar escolhas no display

// Variaveis para base de tempo
volatile char flagTimer0 = 0, flagTimer1 = 0;
unsigned const long int BASE_TEMPO_TIMER = 50; // base de tempo para calculo de delays (50 ms)
int count = 0;

volatile int usedBuffer = 0, buffer_data[BUFFER_SIZE], thermometerRead, memoryread[2048];

Adafruit_PCD8544 display = Adafruit_PCD8544(DC_display, CS_display, reset_display); //criacao do objeto display
//criacao do objeto display

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
   Retorna coluna em nivel baixo para cada linha varrida
*/

int poolingCollums(int _linha) {
  int _coluna;

  digitalWrite(linVect[_linha], LOW); // Seta uma linha em nivel baixo
  for (int i = 0; i < 3; i++) {
    _coluna = digitalRead(colVect[i]);
    if (_coluna == 0) { // Caso uma coluna seja esteja em baixo, botao foi apertado
      digitalWrite(linVect[_linha], HIGH);
      return (i);
    }
  }
  digitalWrite(linVect[_linha], HIGH);
  return 3; // caso contrario, retorna coluna fora do range
}

/*
   Varredura por linhas e debounce do teclado matricial
*/

int poolingLines() {
  int _coluna = 3, _linha = 4; // indices das linhas e colunas varridas
  static int buttonStateReading = 0, buttonStateOld = 0, buttonStateNew = 0; // Estados do teclado
  static int debounceTime; // Tempo de debounce

  for (_linha = 0; _linha < 4; _linha ++) { // varre as linhas
    _coluna = poolingCollums(_linha); // retorna coluna lida
    if (_coluna < 3) // para varredura no primeiro botao pressionado encontrado
      break;
  }

  buttonStateReading = keyValue[_linha][_coluna]; // Retorna Valor do teclado a partir da posicao da matriz

  // Debounce do botao pressionado
  if (buttonStateReading != buttonStateOld) { // marca tempo da ultima alteracao no botao
    debounceTime = count;
  }

  if ((count - debounceTime) > minDebounceTime) { // Se tempo menor que tempo minimo definido
    if (buttonStateNew != buttonStateOld) {
      buttonStateNew = buttonStateReading;  // Atualiza botao
    }
  }

  buttonStateOld = buttonStateReading; // Marca ultimo estado do botao

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
  unsigned char aux[2];

  // Dummy Write para realizar acesso aleatorio a memoria
  Wire.beginTransmission(0b1010111); // transmit to 24C16 (e paginacao 111)
  Wire.write(0xFE);                  // Endereco 2046
  Wire.endTransmission();

  Wire.requestFrom(0b1010111, 2); // Requisita espaco usado (armazenado em x[2046] e x[2047])

  if (Wire.available()) {
    for (_i = 0; _i < 2; _i++)
      aux[_i] = Wire.read();  // Le posicoes 2046 e 2047 que indicam o espaco usado na memoria
  }

  return ((256 * aux[0] + aux[1])); // Expressao para o espaco total usado
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
   para memoria 24C16
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
//                SETUP
// ----------------------------------

void setup() {
  Wire.begin();
  Serial.begin(9600);
  initDisplay();
  initKeyboard();
  Wire.begin();//inicializa o Wire
  Serial.begin(9600); //inicializa a porta serial
  initDisplay(); //inicializa o display
  initKeyboard(); //inicializa o teclado matricial

  // Contador para interrupcao: base de tempo de 50 ms
  Timer3.initialize(BASE_TEMPO_TIMER * 1000); //time in us
  Timer3.attachInterrupt(cronometro);

  analogReference(INTERNAL1V1); // Referencia entrada analogica em 1.1V

  // Estados iniciais das maquinas
  dataloggerState = OCIOSO;
  displayState = MEASURE;

}

// ----------------------------------
//            LOOP PRINCIPAL
// ----------------------------------

void loop() {
  int _keyPressed, aux, tempMeas, auxPrintSerial;
  unsigned int usedData, usedMem;
  unsigned char serialPrintAux[2];
  static char flagConfirm = 1;

  _keyPressed =  poolingLines(); // Valor pressionado do teclado (ja com debounce)

  // Maquina de estados do display
  switch (displayState) {
    case MEASURE: // Le temperatura, mas nao grava no buffer nem memoria
      // Le temperatura atual ((1100 mV / 10 mV) / 1023 bits) * 10 (decimo de grau)
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
      if (_keyPressed == 3) // Transicao de display
        displayState = STATUS;
      break;
    case STATUS: // Mostra estado atual da memoria e medidas
      usedData = usedSpace(); // Le espaco usado na memoria
      display.clearDisplay();
      display.print("STATUS:");
      display.println(statusName[statusIdx]);
      display.println("Used data: ");
      display.setCursor(20, 18);
      display.println(String(usedData));
      display.println("Data avbl:");
      display.setCursor(20, 38);
      display.println(String(usedBuffer));
      if (_keyPressed == 2) // Transicao de display
        displayState = MEASURE;
      break;
    case CONFIRM: // Confirma Transicao de estado
      display.clearDisplay();
      display.println(nextStateName[nextStateIdx]);
      display.println("CONFIRM?");
      display.println("# Yes");
      display.println("* No");
      if (_keyPressed == 10) { // Cancela (*)
        dataloggerStateNext = dataloggerState; // Mantem estado atual
        displayState = displayStateOld; // Volta display antigo
        flagConfirm = 1;
      } else if (_keyPressed == 12) { // Confirma (#)
        dataloggerState = dataloggerStateNext; // Muda estado atual
        displayState = displayStateOld; // Volta display antigo
        flagConfirm = 1;
      }
      break;
  }

  if (flagTimer1) { // Atualiza display a cada 1 s
    display.display();
    flagTimer1 = 0;
  }

  // Maquina de estados do datalogger
  switch (dataloggerState) {
    case OCIOSO: // Estado em idle (padrao)
      statusIdx = 0;

      // Transicoes de estado
      if (_keyPressed == 4) {   // inicia coleta periodica
        dataloggerStateNext = SAMPLING;
        nextStateIdx = 1;
      }
      else if (_keyPressed == 6 ) {// transfere dados para memoria && usedBuffer != 0
        dataloggerStateNext = TRANSFERING;
        nextStateIdx = 3;
      } else if (_keyPressed == 1 ) {
        dataloggerStateNext = CLEARMEM;
        nextStateIdx = 0;
      } else if (_keyPressed == 7 ) {
        dataloggerStateNext = MEMREAD;
        nextStateIdx = 4;
      }
      break;
    case SAMPLING: // Coletando amostras periodicas
      statusIdx = 1;
      if (flagTimer0) { // Realiza leitura a cada 1 segundo
        if (usedBuffer < BUFFER_SIZE) {
          aux = analogRead(thermo) * 110 / 102.3;
          buffer_data[usedBuffer] = (aux & 0xFF00) >> 8;
          buffer_data[usedBuffer + 1] = aux & 0xFF;
          usedBuffer += 2;
          flagTimer0 = 0;
        }
      }

      // Transicao de estado
      if (_keyPressed == 5) {// finaliza coleta periodica
        dataloggerStateNext = OCIOSO;
        nextStateIdx = 2;
      }
      break;
    case TRANSFERING: // Transferindo dados para memoria
      transferBlockData(); // Transfere dados do buffer para memoria
      delay(10); // Tempo para transmissao de todos os dados (necessario para ler memoria corretamente)
      updateUsedSpace(); //Atualiza espaco usado
      usedBuffer = 0; // Zera contador do buffer interno

      // Transicao de estado
      dataloggerState = OCIOSO;
      dataloggerStateNext = OCIOSO;
      break;
    case CLEARMEM: // Apaga contador da memoria
      Wire.beginTransmission(0b1010111); // transmit to 24C16 (e paginacao 111)
      Wire.write(0xFE); // Endereco 2046
      Wire.write(0x00); // Apaga end 2046 e 2047
      Wire.write(0x00);
      Wire.endTransmission();
      dataloggerState = OCIOSO;
      dataloggerStateNext = OCIOSO;
      break;
    case MEMREAD: // LE da memoria e imprime no serial
      display.clearDisplay(); //imprime mensagem no LCD de impressao no monitor serial
      display.println("Printing");
      display.println("on Serial");
      display.display();
      usedMem = usedSpace(); // Ve quantidade de memoria usada
      Wire.beginTransmission(0b1010000); // Dummy write para comeco da memoria
      Wire.endTransmission();

      Wire.requestFrom(0b1010000, usedMem); // Pede todos os dados coletados

      for (int _i = 0; _i < usedMem; _i++) {
        for (int _j = 0; _j < 2; _j++) { //para cada dois bytes coletados, imprime valor
          if (Wire.available()) {
            serialPrintAux[_j] = Wire.read();
          }
        }
        auxPrintSerial = (serialPrintAux[0] << 8) + serialPrintAux[1];

        Serial.print(auxPrintSerial/10); // Impressao da temperatura
        Serial.print(auxPrintSerial / 10); // Impressao da temperatura
        Serial.print('.');
        Serial.println(auxPrintSerial%10);
        Serial.println(auxPrintSerial % 10);
      }
      dataloggerState = OCIOSO; // Volta para estado ocioso
      dataloggerStateNext = OCIOSO;
      displayState = MEASURE;
      break;
  }

  if (dataloggerState != dataloggerStateNext && flagConfirm) {
    displayStateOld = displayState;
    displayState = CONFIRM;
    flagConfirm = 0;
  }


}
