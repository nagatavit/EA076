#include <SPI.h>
#include <TimerThree.h>
#include <String.h>

/*definicao dos pinos de leitura do joystick*/
#define VR_x A0
#define VR_y A1

/*declaracao das variaveis globais*/
int val_x, val_y, aux_x, aux_y, flag = 0;
unsigned char x_pos, y_pos;
String dir_x, dir_y;

/*funcao de inicializacao
 *da porta serial do bluetooth
*/
void initBluetooth() {
  Serial3.begin(38400);
}

/*funcao de configuracao dos pinos 
*do joystick como entrada
*/
void initJoystick() {
  pinMode(VR_x, INPUT);
  pinMode(VR_y, INPUT);
}

/*funcao de inicializacao do Timer3
*para gerar interrupcao periodica a cada 200ms
*/
void initCronometer() {
  Timer3.initialize(200000);
  Timer3.attachInterrupt(cronometer);
}

/*funcao de tratamento de interrupcao do Timer3*/
void cronometer() {
  flag = 1;
}

/*funcao de leitura do joystick e conversao de escala
*para o envio do PWM para o motor entre 0 e 255
*e envio destacando direcao e sentido como F(Forward), 
B(Backward), R(Right) e L(Left)*/
void read_joystick() {
  val_x = analogRead(VR_x);
  val_y = analogRead(VR_y);
  if (val_x > 600) {
    x_pos = (val_x - 512) / 2;
    dir_x = String(x_pos);
    dir_x =  String('F' + dir_x + '*');
  }
  if (val_x < 424) {
    x_pos = -(val_x / 2) + 255;
    dir_x = String(x_pos);
    dir_x = String('B' + dir_x + '*');
  }
  if (val_y > 600) {
    y_pos = (val_y - 512) / 2;
    dir_y = String(y_pos);
    dir_y =  String('R' + dir_y + '*');
  }
  if (val_y < 424) {
    y_pos = -(val_y / 2) + 255;
    dir_y = String(y_pos);
    dir_y = String('L' + dir_y + '*');
  }
}
/*funcao de inicializacao dos peifericos como
*bluetooth, joystick e Timer3
*/
void setup() {
  initBluetooth();
  initJoystick();
  initCronometer();
}

void loop() {
  read_joystick();
  if (flag) {
    Serial3.print(dir_x);
    Serial3.print(dir_y);    
    flag = 0;
  }
}
