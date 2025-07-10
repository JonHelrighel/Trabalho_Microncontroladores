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


