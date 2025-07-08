#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

int main(void) {
	DDRD |= (1 << PD6); // D6 (OC0A) como sa�da
	DDRB |= (1 << PB3); // D11 (OC2A) como sa�da (n�o usada agora)

	// Configura Timer0 - Fast PWM no OC0A
	TCCR0A = (1 << COM0A1) | (1 << WGM01) | (1 << WGM00);
	TCCR0B = (1 << CS01) | (1 << CS00); // Prescaler 64

	// Configura Timer2 (n�o usado ativamente)
	TCCR2A = (1 << COM2A1) | (1 << WGM21) | (1 << WGM20);
	TCCR2B = (1 << CS22);

	OCR0A = 0; // teste pra ver aaa
	OCR2A = 0;

	// PWM sobe de 0 at� 255 e fica l�
	for (int i = 0; i <= 255; i++) {
		OCR0A = i;
		_delay_ms(10);
	}

	// Mant�m PWM no m�ximo para sempre
	while (1) {
		// Nada aqui, OCR0A j� est� em 255
	}
}
