#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define PPR 1
#define FILTER_N 5

void UART_init(void) {
    uint16_t ubrr = 103;
    UBRR0H = ubrr >> 8;
    UBRR0L = ubrr;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void UART_send_char(char c) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

void UART_send_string(const char *s) {
    while (*s) UART_send_char(*s++);
}

char UART_recv_char(void) {
    while (!(UCSR0A & (1 << RXC0)));
    return UDR0;
}

void PWM_init(void) {
    DDRD |= (1 << PD6);
    TCCR0A = (1 << WGM00) | (1 << WGM01) | (1 << COM0A1);
    TCCR0B = (1 << CS01) | (1 << CS00);
    OCR0A = 0;
}

void TIMER1_init(void) {
    TCCR1B = (1 << WGM12) | (1 << CS12);
    OCR1A = 62500 - 1;
    TIMSK1 |= (1 << OCIE1A);
}

volatile uint16_t contagem_pulsos = 0;
volatile uint8_t sinal_rpm = 0;
volatile uint16_t pulsos_capturados = 0;

ISR(INT0_vect) {
    contagem_pulsos++;
}

ISR(TIMER1_COMPA_vect) {
    pulsos_capturados = contagem_pulsos;
    contagem_pulsos = 0;
    sinal_rpm = 1;
}

void SENSOR_init(void) {
    DDRD &= ~(1 << PD2);
    PORTD |= (1 << PD2);
    EICRA |= (1 << ISC01);
    EICRA &= ~(1 << ISC00);
    EIMSK |= (1 << INT0);
    sei();
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
    uint8_t indice_buffer = 0;

    char linha[32];
    char numero[4];
    uint8_t indice_num = 0;

    UART_send_string("Digite 0-100% ou letras A-K:\r\n");

    while (1) {
        // Tratamento UART RX
        if (UCSR0A & (1 << RXC0)) {
            char c = UART_recv_char();

            // 1) Ignorar sempre o CR / LF, caso não haja dígitos acumulados
            if ((c == '\n' || c == '\r') && indice_num == 0) {
                // nada a fazer
            }
            // 2) Se vier Enter e já houver dígitos, trata como número
            else if ((c == '\n' || c == '\r') && indice_num > 0) {
                numero[indice_num] = '\0';
                int ciclo = atoi(numero);
                if (ciclo < 0) {
                    UART_send_string("Erro: valor abaixo de 0%\r\n");
                }
                else if (ciclo > 100) {
                    UART_send_string("Erro: valor acima de 100%\r\n");
                }
                else {
                    duty_desejado = ciclo;
                    snprintf(linha, sizeof(linha), "PWM ajustado para %d%%\r\n", ciclo);
                    UART_send_string(linha);
                }
                // limpa o buffer de número
                indice_num = 0;
                memset(numero, 0, sizeof(numero));
            }
            // 3) Se for letra de A–K / a–k, ajusta duty
            else if ((c >= 'A' && c <= 'K') || (c >= 'a' && c <= 'k')) {
                uint8_t index = (c >= 'a') ? c - 'a' : c - 'A';
                duty_desejado = index * 10;
                snprintf(linha, sizeof(linha), "PWM ajustado para %u%%\r\n", duty_desejado);
                UART_send_string(linha);
                // limpa qualquer resto de número pendente
                indice_num = 0;
                memset(numero, 0, sizeof(numero));
            }
            // 4) Se for dígito, acumula
            else if (c >= '0' && c <= '9' && indice_num < sizeof(numero) - 1) {
                numero[indice_num++] = c;
            }
            // 5) Qualquer outro caractere inválido
            else {
                UART_send_string("Erro: comando inválido\r\n");
                indice_num = 0;
                memset(numero, 0, sizeof(numero));
            }
        }

        // Cálculo do RPM
        if (sinal_rpm) {
            sinal_rpm = 0;

            uint16_t rpm_atual = pulsos_capturados * (60 / PPR);
            buffer_rpm[indice_buffer++] = rpm_atual;
            if (indice_buffer >= FILTER_N) indice_buffer = 0;

            uint32_t soma = 0;
            for (uint8_t i = 0; i < FILTER_N; i++) soma += buffer_rpm[i];
            uint16_t rpm_filtrado = soma / FILTER_N;

            snprintf(linha, sizeof(linha), "RPM: %u\r\n", rpm_filtrado);
            UART_send_string(linha);

            snprintf(linha, sizeof(linha), "PWM: %u%%\r\n", duty_atual);
            UART_send_string(linha);
        }

        // Rampa PWM
        if (tempo_rampa_ms >= 200) {
            tempo_rampa_ms = 0;
            if (duty_atual < duty_desejado) duty_atual++;
            else if (duty_atual > duty_desejado) duty_atual--;
            OCR0A = (duty_atual * 255UL) / 100;
        }

        _delay_ms(10);
        tempo_rampa_ms += 10;
    }
}






    

    
