#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <TimerThree.h>

#define enable_bridge 9
#define motor_pos 10
#define motor_neg 11
#define clk_display 52
#define DIN_display 51
#define DC_display 6
#define CS_display 7
#define reset_display 8
#define interruptPin 21

String command[5] = {"VEL", "VENT", "EXAUST", "PARA", "RETVEL"}, reading = "", reading_aux = "", arg = "";
int i, velocity = 0, count = 0, speed_measured = 0, timer = 0;
char reading_end = 0, flag_vel = 0, aux = 0, state_new = 0, state_old = 0;
volatile const long int TIMER_BASE = 250; // base de tempo
Adafruit_PCD8544 display = Adafruit_PCD8544(DC_display, CS_display, reset_display); //criacao do objeto display

void initEncoder() {
  Timer3.initialize(TIMER_BASE * 1000); //tempo em us
  Timer3.attachInterrupt(cronometer);
 // attachInterrupt(digitalPinToInterrupt(interruptPin), readEncoder, RISING);
}

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

void initMotor() {
  pinMode(enable_bridge, OUTPUT);
  pinMode(motor_pos, OUTPUT);
  pinMode(motor_neg, OUTPUT);
  digitalWrite(motor_pos, HIGH);
  digitalWrite(motor_neg, LOW);
  // analogWrite(enable_bridge, 0);
}

void initBluetooth() {
  Serial3.begin(9600);
  Serial.begin(9600);
  Serial.println("Pronto :");
}

void cronometer() {
  speed_measured = count / 30;
  count = 0;
  timer++;
}
void readEncoder() {
  count++;
}

void bluetoothRead() {
  if (Serial3.available()) {
    aux = Serial3.read();
    if (aux == ' ') {
      reading = String(reading_aux);
      reading_aux = "";
      flag_vel = 1;
    } else if (aux != '*') {
      reading_aux = String(reading_aux + aux);
    } else {
      arg = "AUS";
      if (!flag_vel) {
        reading = String(reading_aux);
      } else {
        arg = String(reading_aux);
      }
      reading_aux = "";
      flag_vel = 0;
      reading_end = 1;
      Serial.println(reading);
      Serial.println(arg);
    }
  }
}

void compareString() {
  for (i = 0; i < 5; i++) {
    if (reading.equalsIgnoreCase(command[i]))
      break;
  }
  switch (i) {
      Serial.println(i);
    case 0:
      if (arg.equalsIgnoreCase("AUS")) {
        Serial.println("ERRO: PARÂMETRO AUSENTE");
        break;
      }

      velocity = arg.toInt();

      if (velocity < 0 || velocity > 100)
        Serial.println("ERRO: PARÂMETRO INCORRETO");

      analogWrite(enable_bridge, (velocity * 255 / 100));
      digitalWrite(motor_pos, HIGH);
      digitalWrite(motor_neg, LOW);
      Serial.print("OK VEL ");
      Serial.println(arg);
      break;

    case 1:
      /*if (!(motor_pos == HIGH || motor_neg == LOW)) {
        digitalWrite(motor_pos, HIGH);
        digitalWrite(motor_neg, HIGH);
        }*/
      state_new = 0;
      Serial.println("OK VENT");
      break;
    case 2:
      /*      if (!(motor_pos == LOW || motor_neg =+ HIGH)) {
              digitalWrite(motor_pos, HIGH);
              digitalWrite(motor_neg, HIGH);
            }*/
      state_new = 1;
      Serial.println("OK EXAUST");
      break;
    case 3:
      state_new = 2;
      Serial.println("OK PARA");
      break;
    case 4:
      break;
    case 5:
      Serial.println("ERRO: COMANDO INEXISTENTE");
      break;

  }
  reading_end = 0;
  attDisplay();
}

void attDisplay() {
  display.clearDisplay();

  display.setCursor(4, 0);
  display.print("VEL. %: ");
  display.println(arg);

  if (state_new == 0) {
    display.setCursor(4, 10);
    display.println("VENTILADOR");
  } else if (state_new == 1) {
    display.setCursor(4, 10);
    display.println("EXAUSTOR");
  } else {
    display.setCursor(4, 10);
    display.println("PARADO");
  }
  
  display.setCursor(4, 20);
  display.println("VEL. ATUAL: ");
  display.setCursor(15, 30);
  display.print(arg);
  display.println(" RPM");

  display.display();
}

void setup() {
  initBluetooth();
  initDisplay();
  initMotor();
  initEncoder();
  //pinMode(51,OUTPUT);
  //pinMode(52,OUTPUT);
  //Adafruit_PCD8544 display = Adafruit_PCD8544(5, 4, 3);
  //display.begin();

}

void loop() {
  bluetoothRead();
  if (reading_end)
    compareString();
  if ((timer % 4)==0) {
    //display.print(speed_measure);
    //display.display();
    attDisplay;
  }

  if (state_new != state_old && speed_measured == 0) {
    if (state_new == 0) {
      digitalWrite(motor_pos, HIGH);
      digitalWrite(motor_neg, LOW);
    } else if (state_new == 1) {
      digitalWrite(motor_pos, LOW);
      digitalWrite(motor_neg, HIGH);
    } else {
      digitalWrite(motor_pos, HIGH);
      digitalWrite(motor_neg, HIGH);
    }
    state_old = state_new;
  }
}
