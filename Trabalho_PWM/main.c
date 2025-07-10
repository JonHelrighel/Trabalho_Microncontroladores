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
volatile uint16_t pulse_count     = 0; //Pq voce ta definindo tag em ingles doidao???????????
volatile uint8_t  rpm_flag        = 0;
volatile uint16_t pulses_captured = 0;


