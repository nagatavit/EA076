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
#define targetSpdMin 15

String command[5] = {"VEL", "VENT", "EXAUST", "PARA", "RETVEL"}, reading = "", reading_aux = "", arg = "";
int i, speed_target = 0, count = 0, speed_measured = 0, timer = 0;
char reading_end = 0, aux = 0, state_new = 0, state_old = 0;
volatile const long int TIMER_BASE = 250; // base de tempo
Adafruit_PCD8544 display = Adafruit_PCD8544(DC_display, CS_display, reset_display); //criacao do objeto display

void initEncoder() {
  Timer3.initialize(TIMER_BASE * 1000); //tempo em us
  Timer3.attachInterrupt(cronometer);
  pinMode(interruptPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(interruptPin), readEncoder, FALLING);
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
  analogWrite(enable_bridge, initialSpeed);
}

void initBluetooth() {
  Serial3.begin(9600);
  Serial.begin(9600);
}

void cronometer() {
  // velocidade = ((encoder / pás) / tempo) * 60 segundos = ((encoder / 2) / 0.25) * 60 segundos
  speed_measured = count * 120;
  count = 0;
  timer++;
}

void readEncoder() {
  count++;
}

void bluetoothRead() {
  static char _flagVel = 0;

  if (Serial3.available()) {
    aux = Serial3.read();
    if (aux == ' ') {
      reading = String(reading_aux);
      reading_aux = "";
      _flagVel = 1;

    } else if (aux != '*') {
      reading_aux = String(reading_aux + aux);

    } else {
      arg = "AUS";
      if (!_flagVel) {
        reading = String(reading_aux);
      } else {
        arg = String(reading_aux);
      }

      reading_aux = "";
      _flagVel = 0;
      reading_end = 1;
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

      if (arg.toInt() < 0 || arg.toInt() > 100) {
        Serial.println("ERRO: PARÂMETRO INCORRETO");
        break;
      }

      speed_target = arg.toInt();

      analogWrite(enable_bridge, (convSpd(speed_target)) * 255 / 100 ); //
      Serial.println(convSpd(speed_target));

      if (state_old == 2) {
        state_new = 0;
      }
      Serial.print("OK VEL ");
      Serial.println(arg);
      break;

    case 1:
      state_new = 0;
      stopBridge();
      Serial.println("OK VENT");
      break;
    case 2:
      state_new = 1;
      stopBridge();
      Serial.println("OK EXAUST");
      break;
    case 3:
      state_new = 2;
      stopBridge();
      Serial.println("OK PARA");
      break;
    case 4:
      Serial.print("VEL: ");
      Serial.print(String(speed_measured));
      Serial.println(" RPM");
      break;
    case 5:
      Serial.println("ERRO: COMANDO INEXISTENTE");
      break;

  }
  reading_end = 0;
  attDisplay();
}

int convSpd(int _target) {
  return (_target * (100 - targetSpdMin) / 100 + targetSpdMin);
}

void stopBridge() {
  if (state_new != state_old) {
    digitalWrite(motor_pos, HIGH);
    digitalWrite(motor_neg, HIGH);
  }
}

void attDisplay() {
  display.clearDisplay();

  display.setCursor(4, 0);
  display.print("VEL. %: ");
  display.println(String(speed_target));

  if (state_old == 0) {
    display.setCursor(4, 10);
    display.println("VENTILADOR");
  } else if (state_old == 1) {
    display.setCursor(4, 10);
    display.println("EXAUSTOR");
  } else {
    display.setCursor(4, 10);
    display.println("PARADO");
  }

  display.setCursor(4, 20);
  display.println("VEL. ATUAL: ");
  display.setCursor(15, 30);
  display.print(String(speed_measured));
  display.println(" RPM");

  display.display();
}

void setup() {
  initBluetooth();
  initDisplay();
  initMotor();
  initEncoder();
  attDisplay();
}

void loop() {
  bluetoothRead();

  if (reading_end)
    compareString();

  if ((timer % 6) == 0)
    attDisplay();

  if (state_new != state_old && speed_measured == 0) {
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
