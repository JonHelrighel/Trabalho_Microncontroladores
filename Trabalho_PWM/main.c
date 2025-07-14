#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define PPR 1        // Pulsos por revolução
#define FILTER_N 5   // Janela da média móvel

// --- UART 9600 8N1 ---
void UART_init(void) {
	uint16_t ubrr = 103;  // 16MHz / 16 / 9600 - 1
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

// --- PWM no Timer0 (D6 / OC0A) ---
void PWM_init(void) {
	DDRD  |= (1<<PD6); // OC0A como saída
	TCCR0A = (1<<WGM00)|(1<<WGM01)|(1<<COM0A1); // Fast PWM, não inversor
	TCCR0B = (1<<CS01)|(1<<CS00);              // Prescaler 64
	OCR0A  = 0; // Duty inicial 0%
}

// --- Timer1 para amostragem a cada 1 segundo ---
void TIMER1_init(void) {
	TCCR1B  = (1<<WGM12)|(1<<CS12);  // CTC, prescaler 256
	OCR1A   = 62500 - 1;             // 16MHz / 256 = 62500 → 1s
	TIMSK1 |= (1<<OCIE1A);           // Habilita interrupção Compare A
}

// --- Variáveis globais ---
volatile uint16_t contagem_pulsos = 0;
volatile uint8_t sinal_rpm = 0;
volatile uint16_t pulsos_capturados = 0;

// --- Interrupção do sensor (INT0 no PD2) ---
ISR(INT0_vect) {
	contagem_pulsos++;
}

// --- Timer1: a cada 1s captura a contagem ---
ISR(TIMER1_COMPA_vect) {
	pulsos_capturados = contagem_pulsos;
	contagem_pulsos = 0;
	sinal_rpm = 1;
}

// --- Inicializa INT0 no PD2 ---
void SENSOR_init(void) {
	DDRD  &= ~(1<<PD2);               // PD2 como entrada
	PORTD |=  (1<<PD2);               // pull-up interno
	EICRA |=  (1<<ISC01);             // INT0 = borda de descida
	EICRA &= ~(1<<ISC00);
	EIMSK |=  (1<<INT0);              // habilita INT0
	sei();                            // habilita interrupções globais
}

int main(void) {
	UART_init();
	PWM_init();
	TIMER1_init();
	SENSOR_init();
	
	uint8_t duty_atual = 0;
	uint8_t duty_desejado = 0;
	uint16_t tempo_rampa_ms = 0;

	uint16_t buffer_rpm[FILTER_N] = {0};
	uint8_t  indice_buffer = 0;

	char linha[32];
	char numero[4];
	uint8_t indice_num = 0;

	UART_send_string("Digite 0-100% e ENTER:\r\n");

	while (1) {
		// Leitura via UART (duty cycle)
if (UCSR0A & (1<<RXC0)) {
	char c = UART_recv_char();

	// Controle via letras A–K (ou a–k)
	if ((c >= 'A' && c <= 'K') || (c >= 'a' && c <= 'k')) {
		uint8_t duty_table[] = {0, 26, 51, 77, 102, 128, 153, 179, 204, 230, 255};
		uint8_t index = (c >= 'a') ? c - 'a' : c - 'A';
		if (index < sizeof(duty_table)) {
			duty_desejado = index * 10;
			snprintf(linha, sizeof(linha), "PWM ajustado para %u%%\r\n", index * 10);
			UART_send_string(linha);
		}
	}
	// Controle via número (ex: 75 + ENTER)
	else if (c >= '0' && c <= '9' && indice_num < sizeof(numero)-1) {
		numero[indice_num++] = c;
	}
	else if ((c == '\r' || c == '\n') && indice_num > 0) {
		numero[indice_num] = '\0';
		int ciclo = atoi(numero);
		if (ciclo < 0) ciclo = 0;
		if (ciclo > 100) ciclo = 100;
		duty_desejado = ciclo;
		indice_num = 0;
		memset(numero, 0, sizeof(numero));
		snprintf(linha, sizeof(linha), "PWM ajustado para %d%%\r\n", ciclo);
		UART_send_string(linha);
	}
}
		// Processa RPM a cada 1 segundo
		if (sinal_rpm) {
			sinal_rpm = 0;

			// Adiciona nova leitura convertida em RPM ao buffer
			uint16_t rpm_atual = pulsos_capturados * (60 / PPR);
			buffer_rpm[indice_buffer++] = rpm_atual;
			if (indice_buffer >= FILTER_N) indice_buffer = 0;

			// Média móvel
			uint32_t soma = 0;
			for (uint8_t i = 0; i < FILTER_N; i++) {
				soma += buffer_rpm[i];
			}
			uint16_t rpm_filtrado = soma / FILTER_N;

			// Envia via UART
			snprintf(linha, sizeof(linha), "RPM: %u\r\n", rpm_filtrado);
			UART_send_string(linha);
		}
		// Controle de rampa PWM (1% a cada 200ms)
if (tempo_rampa_ms >= 200) {
	tempo_rampa_ms = 0;

	if (duty_atual < duty_desejado) {
		duty_atual++;
	} else if (duty_atual > duty_desejado) {
		duty_atual--;
	}
	OCR0A = (duty_atual * 255UL) / 100;
}
_delay_ms(10);  // base de tempo
tempo_rampa_ms += 10;

	}
}

    

    
