#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define PPR 1       // 1 pulso por revolucao (ajustado ao seu disco)
#define FILTER_N 5  // Numero de amostras na media movel

// --- UART 9600 8N1 ---
void UART_init(void) {
	uint16_t ubrr = 103;           // 16MHz / 16 / 9600 - 1
	UBRR0H = ubrr >> 8;
	UBRR0L = ubrr;
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
}

void UART_send_char(char c) {
    while (!(UCSR0A & (1<<UDRE0)));
    UDR0 = c;
}

void UART_send_string(const char *s) {
    while (*s) UART_send_char(*s++);
}

char UART_recv_char(void) {
    while (!(UCSR0A & (1<<RXC0)));
    return UDR0;
}
void PWM_init(void) {
    DDRD  |= (1<<PD6);  // OC0A (D6) como saída
    TCCR0A = (1<<WGM00)|(1<<WGM01)|(1<<COM0A1); // Fast PWM, não inversor
    TCCR0B = (1<<CS01)|(1<<CS00); // Prescaler 64
    OCR0A  = 0;  // Duty inicial 0%
}                             // 0% duty cycle

void TIMER1_init(void) {
    TCCR1B  = (1<<WGM12)|(1<<CS12); // CTC, prescaler 256
    OCR1A   = 62500 - 1;            // 16MHz/256 = 62500 ticks → 1s
    TIMSK1 |= (1<<OCIE1A);          // IRQ Compare A
}
volatile uint16_t contagem_pulsos  = 0; 
volatile uint8_t  sinal_rpm        = 0;
volatile uint16_t pulsos_capturados = 0;

ISR(INT0_vect) {
    contagem_pulsos++;
}

ISR(TIMER1_COMPA_vect) {
    pulsos_capturados = contagem_pulsos;
    contagem_pulsos   = 0;
    sinal_rpm         = 1;
}
void SENSOR_init(void) {
    DDRD  &= ~(1<<PD2);
    PORTD |=  (1<<PD2);              // pull‑up
    EICRA |=  (1<<ISC01)|(1<<ISC00); // INT0 = borda de subida
    EIMSK |=  (1<<INT0);
    sei();                           // habilita interrupções
}
int main(void) {
    UART_init();        
    PWM_init();
    TIMER1_init();
    SENSOR_init();

    // Buffer para média móvel de RPM
    uint16_t buffer_rpm[FILTER_N] = {0};
    uint8_t  indice_buffer = 0;

    char linha[32];
    char numero[4];
    uint8_t indice_num = 0;

    UART_send_string("Digite 0-100% e ENTER:\r\n");

    while (1) {

         if (UCSR0A & (1<<RXC0)) {
            char c = UART_recv_char();
            if (c >= '0' && c <= '9' && indice_num < sizeof(numero)-1) {
                numero[indice_num++] = c;
            }
            else if ((c=='\r' || c=='\n') && indice_num > 0) {
                numero[indice_num] = '\0';
                int ciclo = atoi(numero);
                if (ciclo < 0) ciclo = 0;
                if (ciclo > 100) ciclo = 100;
                OCR0A = (uint8_t)((ciclo * 255UL) / 100);
                indice_num = 0;
                memset(numero, 0, sizeof(numero));
                UART_send_string("OK\r\n");
            }
        }
    }
}



    

    
