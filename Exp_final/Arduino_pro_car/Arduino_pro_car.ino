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

/*
 * Leitura do bluetooth e parsing:
 *  funcao le do buffer provindo do bluetooth
 *  e retorna um char, correspondente a funcao
 *  e escreve o valor recebido no parametro commandValue
 */

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

/*
 * Funcao de selecao de velocidade
 *  escreve um sinal PWM na saida 
 *  correspondente ao controle da 
 *  ponte H para velocidade do motor
 */

void speedSelect(unsigned char dir, unsigned int value) {
  if (value == 0 ) {
    digitalWrite(pinUpDown1, HIGH);
    digitalWrite(pinUpDown2, HIGH);
  } else {
    digitalWrite(pinUpDown1, dir);
    digitalWrite(pinUpDown2, 1 ^ dir);
  }
  analogWrite(motorPWM, value);
}

/*
 * Funcao de selecao de direcao
 *  escreve um sinal PWM na saida 
 *  correspondente ao controle da 
 *  ponte H para direcao do carro
 */

void directionSelect(unsigned char dir, unsigned int value) {
  if (value == 0) {
    digitalWrite(pinLeftRight1, HIGH);
    digitalWrite(pinLeftRight2, HIGH);
  } else {
    digitalWrite(pinLeftRight1, dir);
    digitalWrite(pinLeftRight2, 1 ^ dir);
  }
  analogWrite(directionPWM, value);
}

/*
 * Inicializacao das portas do arduino
 */

void setup() {
  Serial.begin(38400);
  
  pinMode(pinUpDown1, OUTPUT); // Sentido de rotacao do motor de tracao
  pinMode(pinUpDown2, OUTPUT);
  pinMode(pinLeftRight1, OUTPUT); // Sentido de rotacao da solenoide de direcao
  pinMode(pinLeftRight2, OUTPUT);
  pinMode(motorPWM, OUTPUT);    // Intensidade do motor de tracao
  pinMode(directionPWM, OUTPUT); // Intensidade da solenoide de direcao

  digitalWrite(pinUpDown1, HIGH); // Estado padrao do motor de tracao, para frente
  digitalWrite(pinUpDown2, LOW);
  analogWrite(motorPWM, 0);

  digitalWrite(pinLeftRight1, LOW); // Estado padrao da direcao, endireitado
  digitalWrite(pinLeftRight2, LOW);
  analogWrite(directionPWM, 0);

  //carControlState = OCIOSO;

}

/*
 * Loop principal:
 *  Selecao das funcoes lidas do bluetooth e
 * chamadas das funcoes de controle de velocidade
 * e direcao
 */

void loop() {
  static char opSelect = 0;  // Selecao de comando
  static long int pwmValue = 0; // Valor lido do bluetooth
  static Ultrasonic ultrasom(pino_trigger, pino_echo); // Objeto do sensor ultrasonico
  static long dist; // Distancia lida
  static char flagDist = 0; // Flag para recuo do carrinho

  dist = ultrasom.Ranging(CM);

  opSelect = bluetoothParsing(&pwmValue);

  if (dist < 30 ) {
    speedSelect(0, 127);
    flagDist = 1;
  } else if (dist >= 30 && flagDist ) {
    speedSelect(1, 0);
    flagDist = 0;
  }

  if (opSelect == 'F') { // Sentido Frente "Foward"
    speedSelect(1, pwmValue);
  } else if (opSelect == 'B') { // Sentido Tras "Backward"
    speedSelect(0, pwmValue);
  } else if (opSelect == 'L') { // Sentido Esquerda "Left"
    directionSelect(1, pwmValue);
  } else if (opSelect == 'R') { // Sentido Direita "Right"
    directionSelect(0, pwmValue);
  }
  opSelect = 0; // Zera leitura de caracter
  pwmValue = 0; // Zera leitura de valor

}
