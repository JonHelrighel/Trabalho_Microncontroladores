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

// Inicializa UART
void UART_init(void) {
    uint16_t ubrr = 103;                   // Baud rate 9600 para 16 MHz
    UBRR0H = ubrr >> 8;
    UBRR0L = ubrr;
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);  // Habilita RX e TX
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);// 8 bits, sem paridade
}

// Envia um caractere via UART
void UART_send_char(char c) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

// Envia uma string via UART
void UART_send_string(const char *s) {
    while (*s) UART_send_char(*s++);
}

// Recebe caractere via UART
char UART_recv_char(void) {
    while (!(UCSR0A & (1 << RXC0)));
    return UDR0;
}

// Inicializa PWM no pino PD6 (OC0A)
void PWM_init(void) {
    DDRD |= (1 << PD6);                    // PD6 como saída
    TCCR0A = (1 << WGM00) | (1 << WGM01) | (1 << COM0A1); // Fast PWM
    TCCR0B = (1 << CS01) | (1 << CS00);    // Prescaler 64
    OCR0A = 0;                             // Duty inicial 0%
}

// Inicializa TIMER1 em modo CTC com interrupção a cada 1s
void TIMER1_init(void) {
    TCCR1B = (1 << WGM12) | (1 << CS12);   // CTC, prescaler 256
    OCR1A = 62500 - 1;                     // 1 segundo (16MHz / 256)
    TIMSK1 |= (1 << OCIE1A);              // Habilita interrupção OCR1A
}

volatile uint16_t contagem_pulsos = 0;     // Contador de pulsos
volatile uint8_t sinal_rpm = 0;            // Flag de cálculo de RPM
volatile uint16_t pulsos_capturados = 0;   // Armazena pulsos de 1s

// Interrupção de pulso do sensor (INT0)
ISR(INT0_vect) {
    contagem_pulsos++;
}

// Interrupção de tempo do TIMER1 (1s)
ISR(TIMER1_COMPA_vect) {
    pulsos_capturados = contagem_pulsos;
    contagem_pulsos = 0;
    sinal_rpm = 1;
}

// Inicializa entrada do sensor com pull-up e interrupção
void SENSOR_init(void) {
    DDRD &= ~(1 << PD2);                  // PD2 como entrada
    PORTD |= (1 << PD2);                  // Pull-up ativado
    EICRA |= (1 << ISC01);                // Interrupção na borda de descida
    EICRA &= ~(1 << ISC00);
    EIMSK |= (1 << INT0);                 // Habilita INT0
    sei();                                // Habilita interrupções globais
}

int main(void) {
    UART_init();
    PWM_init();
    TIMER1_init();
    SENSOR_init();

    uint8_t duty_atual = 0;
    uint8_t duty_desejado = 0;
    uint16_t tempo_rampa_ms = 0;

    uint16_t buffer_rpm[FILTER_N] = {0};  // Buffer circular para média móvel
    uint8_t indice_buffer = 0;

    char linha[32];
    char numero[4];
    uint8_t indice_num = 0;

    UART_send_string("Digite 0-100% ou letras A-K:\r\n");

    while (1) {
        // Leitura UART (não bloqueante)
        if (UCSR0A & (1 << RXC0)) {
            char c = UART_recv_char();

            // Ignora ENTER se nenhum dígito foi digitado
            if ((c == '\n' || c == '\r') && indice_num == 0) {
                // faz nada
            }
            // Se ENTER e dígitos acumulados, converte número
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
                indice_num = 0;
                memset(numero, 0, sizeof(numero));
            }
            // Se letra A–K ou a–k, calcula duty correspondente
            else if ((c >= 'A' && c <= 'K') || (c >= 'a' && c <= 'k')) {
                uint8_t index = (c >= 'a') ? c - 'a' : c - 'A';
                duty_desejado = index * 10;
                snprintf(linha, sizeof(linha), "PWM ajustado para %u%%\r\n", duty_desejado);
                UART_send_string(linha);
                indice_num = 0;
                memset(numero, 0, sizeof(numero));
            }
            // Se for número, acumula
            else if (c >= '0' && c <= '9' && indice_num < sizeof(numero) - 1) {
                numero[indice_num++] = c;
            }
            // Qualquer outro caractere é inválido
            else {
                UART_send_string("Erro: comando inválido\r\n");
                indice_num = 0;
                memset(numero, 0, sizeof(numero));
            }
        }

        // Cálculo do RPM a cada 1 segundo
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

        // Atualização do PWM suavemente (rampa)
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







    

    
