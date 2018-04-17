/*  EA076 - Exp2 - Ceiling fan 
 *   Hugo Ralf
 *   Vitor Nagata 
 *   
 * Ceiling fan controled by PWM, using H bridge to control a 5.9V DC motor
 * with commands sent via bluetooth throug a android bluetooth terminal,
 * an Encoder using PHCT203, readings and target speed print on an NOKIA 5110 LCD
 * 
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <TimerThree.h>

#define DC_display 6
#define CS_display 7
#define reset_display 8
#define enable_bridge 9
#define motor_pos 10
#define motor_neg 11
#define clk_display 52
#define DIN_display 51
#define interruptPin 21
#define initialSpeed 0
#define targetSpdMin 27

volatile const long int TIMER_BASE = 250; // base de tempo

String command[5] = {"VEL", "VENT", "EXAUST", "PARA", "RETVEL"};   // Comandos pré-definidos
String reading = "", reading_aux = "", arg = ""; // Variáveis de leitura dos comandos recebidos

int speed_target = 0, count = 0, speed_measured = 0, timer = 0;
 
char reading_end = 0, aux = 0, state_new = 0, state_old = 0; // Variáveis de estado

Adafruit_PCD8544 display = Adafruit_PCD8544(DC_display, CS_display, reset_display); // Criacao do objeto display

/*
 * Inicialização para medição de velocidade
 *  uso do Timer3 para contagem de tempo
 *  interrupção feita pela chave optoeletronica
 */

void initEncoder() { 
  Timer3.initialize(TIMER_BASE * 1000); //tempo em us
  Timer3.attachInterrupt(cronometer);
  pinMode(interruptPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(interruptPin), readEncoder, FALLING);
}

/*
 *  Inicialização dos pinos usado para o display 
 *  comunicação via SPI por Hardware
 *  configuração inicial do display
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
 * Inicialização dos pinos da ponte H 
 * para controle do motor DC e ajuste de velocidade
 * inicial no PWM
 */

void initMotor() {
  pinMode(enable_bridge, OUTPUT);
  pinMode(motor_pos, OUTPUT);
  pinMode(motor_neg, OUTPUT);
  digitalWrite(motor_pos, HIGH);
  digitalWrite(motor_neg, LOW);
  analogWrite(enable_bridge, initialSpeed);
}

/*
 * Inicialização do Serial para Bluetooth
 * e comunicação com o terminal
 */

void initBluetooth() {
  Serial3.begin(9600);
  Serial.begin(9600);
}


/* Rotina de interrupção do Timer3:
 *   - Adiciona contador timer, utilizado 
 * para rotinas regulares no loop
 *   - Cálculo da velocidade a partir das 
 * medições da chave optoeletrônica (e zera contador):
 * 
 * velocidade = ((encoder / pás) / tempo) * 60 segundos 
 * velocidade = ((encoder / 2) / 0.25) * 60 segundos
 */

void cronometer() {
  timer++;
  speed_measured = count * 120;
  count = 0;
}

/*
 * Interrupção da chave optoeletrônica
 * (soma contador)
 */
 
void readEncoder() {
  count++;
}

/*
 * Leitura e tratamento dos dados do buffer 
 * circular no Serial3 vindo do aparelho bluetooth.
 */

void bluetoothRead() {
  static char _flagVel = 0;

  if (Serial3.available()) {  // Se houver Caracters disponiveis
    aux = Serial3.read();     // Lê char disponível
    if (aux == ' ') {       // Caso especial: leitura de um espaço
      reading = String(reading_aux);  // Comando lido será a string antes do espaço
      reading_aux = "";
      _flagVel = 1;

    } else if (aux != '*') {    // Concatena chars até encontrar um "*"
      reading_aux = String(reading_aux + aux);

    } else {      // Lê um "*", fim de leitura
      arg = "AUS";
      
      if (!_flagVel) {  // Caso não se tenha lido um espaço
        reading = String(reading_aux);  // Comando é a string inteira
      } else {                    // Se leu um espaço
        arg = String(reading_aux); // A string é o argumento do comando
      }

      reading_aux = ""; // Limpa string lida e flags auxiliares
      _flagVel = 0;
      reading_end = 1; // Comando lido até o final
    }
  }
}

/*
 * Compara comando recebido com comandos pré-definidos
 * e executa funções de cada estado referente a um comando
 */

void compareString() {
  int i;
  for (i = 0; i < 5; i++) { // Verifica se a string lida coincide com algum comando
    if (reading.equalsIgnoreCase(command[i])) // String que coinicidir é armazenado em i
      break;
  }

  switch (i) {
      
    case 0: // Comando Lido == VEL
    
      if (arg.equalsIgnoreCase("AUS")) {    // Nao foi lido um argumento
        Serial.println("ERRO: PARÂMETRO AUSENTE");
        break;
      }

      if (arg.toInt() < 0 || arg.toInt() > 100) { // argumento fora da faixa de valores
        Serial.println("ERRO: PARÂMETRO INCORRETO");
        break;
      }

      speed_target = arg.toInt(); // conversão da string para velocidade alvo

      analogWrite(enable_bridge, (convSpd(speed_target)) * 255 / 100 ); // Mudança no PWM para ajuste de velocidade
      
      if (state_old == 2) { // Caso estado anterior seja parado, mudar para ventilador
        state_new = 0;
      }
      Serial.print("OK VEL ");
      Serial.print(arg);
      Serial.println(" %");
      break;

    case 1: // Comando Lido == VENT
      state_new = 0;
      stopBridge(); // para motor (só se mudar de estado)
      Serial.println("OK VENT");
      break;
    case 2: // Comando Lido == EXAUST
      state_new = 1;
      stopBridge();
      Serial.println("OK EXAUST");
      break;
    case 3: // Comando Lido == PARA
      state_new = 2;
      stopBridge();
      Serial.println("OK PARA");
      break;
    case 4: // Comando Lido == RETVEL
      Serial.print("VEL: ");
      Serial.print(String(speed_measured));
      Serial.println(" RPM");
      break;
    case 5: // Comando Lido não corresponde a nenhum comando pré-definido
      Serial.println("ERRO: COMANDO INEXISTENTE");
      break;

  }
  reading_end = 0;
  attDisplay();
}

/*
 * Rotina auxiliar para conversão de escala
 * para que a porcentagem do ventilador esteja
 * na região operável do motor pelo PWM
 */

int convSpd(int _target) { 
  
  if(_target == 0)
    return(0);
  else
    return (_target * (100 - targetSpdMin) / 100 + targetSpdMin);
    
}

/*
 * Rotina auxliar para transição de estados (freia motor)
 */

void stopBridge() { 
  if (state_new != state_old) { // Só realiza a parada se houver mudança de estados
    digitalWrite(motor_pos, HIGH);
    digitalWrite(motor_neg, HIGH);
  }
}

/*
 * Rotina de impressão do display
 */

void attDisplay() {
  display.clearDisplay();

  display.setCursor(4, 0 ); // Impressão da velocidade alvo em porcentagem 
  display.print("VEL.: ");
  display.print(String(speed_target));
  display.println(" %");

  if (state_old == 0) { // Impressão do estado atual do ventilador
    display.setCursor(4, 10);
    display.println("VENTILADOR");
  } else if (state_old == 1) {
    display.setCursor(4, 10);
    display.println("EXAUSTOR");
  } else {
    display.setCursor(4, 10);
    display.println("PARADO");
  }

  display.setCursor(4, 20); // Impressão da velocidade medida pelo encoder em RPM
  display.println("VEL. ATUAL: ");
  display.setCursor(15, 30);
  display.print(String(speed_measured));
  display.println(" RPM");

  display.display();
}

void setup() {  // Inicialização geral
  initBluetooth();
  initDisplay();
  initMotor();
  initEncoder();
  attDisplay();
}

void loop() {
  bluetoothRead(); 

  if (reading_end)  // Leitura da string após receber "*"
    compareString();

  if ((timer % 4) == 0) // Taxa de atualiação do display
    attDisplay();

  if (state_new != state_old && speed_measured == 0) {  // Mudança de estado após motor parado (transição)
    if (state_new == 0) {
      digitalWrite(motor_pos, HIGH);
      digitalWrite(motor_neg, LOW);
    } else if (state_new == 1) {
      digitalWrite(motor_pos, LOW);
      digitalWrite(motor_neg, HIGH);
    } else if (state_new == 2) {
      speed_target = 0;
      analogWrite(enable_bridge, 0);
    }
    state_old = state_new;
  }
}
