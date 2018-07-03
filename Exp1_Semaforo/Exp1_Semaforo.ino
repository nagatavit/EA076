#include "TimerThree.h"

//nomeacao  dos pinout
#define GPIO_botao  12
#define LED_vermelho_car 5
#define LED_amarelo_car 6
#define LED_verde_car 7
#define LED_vermelho_pedestre  8
#define LED_verde_pedestre 9
#define base_LDR 700
#define tempo_min_debounce 1

// ENUM que define os estados do semaforo
volatile enum {DIA_G, DIA_Y, DIA_R, DIA_BR, NOITE_BY} estado;

//declaracao das variaveis de controle da maquina de estados
volatile unsigned char botao_atual = 1, mudanca_est = 0, count, pisca_LED = 0, i = 0, aux2 = 0, botao_last = HIGH, botao_leitura ; // variaveis de estado
volatile unsigned int luminosidade[10], ldr_medio = 0, aux = 0, contador_1, contador_2, tempo_debounce; // variaveis de medições
volatile unsigned const long int BASE_TEMPO_TIMER = 50; // base de tempo para calculo de delays

void setup() {
  // Definicao das GPIO para o sinal dos LEDs como saida e o botao como entrada
  pinMode(GPIO_botao, INPUT);
  pinMode(LED_verde_pedestre, OUTPUT);
  pinMode(LED_vermelho_pedestre, OUTPUT);
  pinMode(LED_verde_car, OUTPUT);
  pinMode(LED_amarelo_car, OUTPUT);
  pinMode(LED_vermelho_car, OUTPUT);

  // Contador para interrupcao: base de tempo de 50 ms
  Timer3.initialize(BASE_TEMPO_TIMER * 1000); //tempo em us
  Timer3.attachInterrupt(cronometro);

  //inicializacao do semaforo no estado de dia com passagem liberada para carros
  estado = DIA_G;
  mudanca_est = 1;
}


//funcao de incremento das variaveis de controle do contador (interrupcao Timer3)
void cronometro() {
  contador_1++;
  contador_2++;
  count++;
}

// Funções auxiliares
void zera_flags() {
  contador_1 = 0;
  contador_2 = 0;
  mudanca_est = 0;
}

void apaga_leds() {
  digitalWrite(LED_verde_car, LOW);
  digitalWrite(LED_amarelo_car, LOW);
  digitalWrite(LED_vermelho_car, LOW);
  digitalWrite(LED_verde_pedestre, LOW);
  digitalWrite(LED_vermelho_pedestre, LOW);
}

// Loop principal
void loop() {

  // Pooling de leitura do botao com uso de debounce

  botao_leitura = digitalRead(GPIO_botao);
  if (botao_leitura != botao_last) {
    tempo_debounce = count;
  }

  if ((count - tempo_debounce) > tempo_min_debounce) {
    if (botao_atual != botao_last) {
      botao_atual = botao_leitura;
    }
  }

  botao_last = botao_leitura;

  /*
    Maquina de estados do semaforo:

    DIA_G: Estado DIURNO com verde para carros e vermelho para pedestres
    DIA_Y: Estado DIURNO com amarelo para carros e vermelho para pedestres
    DIA_R: Estado DIURNO com vermelho para carros e verde para pedestres
    DIA_BR: Estado DIURNO com vermelho para carros e vermelho piscante para pedestres
    NOITE_BR: Estado NOTURNO com amarelo piscante para carros e vermelho piscante para pedestres

  */
  switch (estado) {

    case DIA_G:
      if (mudanca_est) { // Setup inicial do estado
        apaga_leds();
        digitalWrite(LED_verde_car, HIGH);
        digitalWrite(LED_vermelho_pedestre, HIGH);
        zera_flags();
      } else if ( (!mudanca_est) && (botao_atual == LOW) ) { // Transição de estado através do push button
        estado = DIA_Y;
        mudanca_est = 1;
        contador_1 = 0;
      }
      break;

    case DIA_Y:
      if (mudanca_est && ((contador_1 * BASE_TEMPO_TIMER) >= 800) ) { // Setup inicial e espera tempo para mudança de estado
        apaga_leds();
        zera_flags();
        digitalWrite(LED_vermelho_pedestre, HIGH);
        digitalWrite(LED_amarelo_car, HIGH);
      } else if ( !(mudanca_est) && ((contador_1 * BASE_TEMPO_TIMER) >= 3000) ) { // Transição de estado após 3s
        estado = DIA_R;
        contador_1 = 0;
        mudanca_est = 1;
      }
      break;

    case DIA_R:
      if (mudanca_est) {  // Setup inicial do estado
        apaga_leds();
        digitalWrite(LED_vermelho_car, HIGH);
        digitalWrite(LED_verde_pedestre, HIGH);
        zera_flags();
      }

      if ( (contador_1 * BASE_TEMPO_TIMER) >= 3000) { // Transição de estado após 3s
        estado = DIA_BR;
        mudanca_est = 1;
      }
      break;

    case DIA_BR:
      if (mudanca_est) {  // Setup inicial do estado
        apaga_leds();
        digitalWrite(LED_vermelho_car, HIGH);
        digitalWrite(LED_vermelho_pedestre, HIGH);
        zera_flags();
      }
      if ( (contador_2 * BASE_TEMPO_TIMER) >= 800 ) { // Alterna entre ligado e desligado do led vermelho
        contador_2 = 0;
        pisca_LED = digitalRead(LED_vermelho_pedestre);
        pisca_LED ^= 0b1;
        digitalWrite(LED_vermelho_pedestre, pisca_LED);

      } else if ( (contador_1 * BASE_TEMPO_TIMER) >= 5000) { // Transição de estado após 5s
        estado = DIA_G;
        mudanca_est = 1;
      }
      break;

    case NOITE_BY:
      if (mudanca_est) {  //Setup inicial de estado (todos os leds apagados
        apaga_leds();
        zera_flags();
      }

      /* Alterna entre desligado e ligado para os leds vermelho para
       *  pedestres e amarelo para carros à cada 0.5 s
       */
      if ( (contador_1 * BASE_TEMPO_TIMER) >= 500) { 
        pisca_LED = digitalRead(LED_vermelho_pedestre);
        pisca_LED ^= 0b1;
        digitalWrite(LED_vermelho_pedestre, pisca_LED);
        digitalWrite(LED_amarelo_car, pisca_LED);
        contador_1 = 0;
      }
      break;
  }

  // Leitura periodica e calculo das médias da tensao do LDR
  if ( !((count * BASE_TEMPO_TIMER) % 100 ) ) {
    aux = analogRead(A0);
    /* Até 10 medições, a média é feita termo a termo
     *  Após 10 medições, média é feita retirando-se o 
     *  valor mais antigo e adicionando o valor atual
     */
    if (aux2 > 10) {  
      ldr_medio = ldr_medio - (luminosidade[i] + aux) / 10; 
    } else {
      ldr_medio += aux;
      ldr_medio = ldr_medio / 2;
    }
    aux2++;
    luminosidade[i] = aux;
    i = (i + 1) % 10;
  }

  /* Verificação periódica das médias do LDR para 
   *  caso haja transição, mudar de estado DIURNO 
   *  para NOTURNO (e vice versa)
   */
  if (!((count * BASE_TEMPO_TIMER) % 5000)) {
    if ((ldr_medio >= base_LDR) && (estado == NOITE_BY)) { // Transição modo NOTURNO -> DIURNO
      estado = DIA_G;
      mudanca_est = 1;
    } else if ((ldr_medio < base_LDR) && (estado != NOITE_BY)) { // Transição modo DIURNO -> NOTURNO
      estado = NOITE_BY;
      mudanca_est = 1;
    }
  }

}


