#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "lib/ssd1306_i2c.h"
#include "lib/matriz_leds.h"
#include <stdint.h>

#define BUTTON_H
#define LED_H
#define I2C_SDA 14
#define I2C_SDL 15
#define BTN_A 5
#define BTN_B 6
#define LED_G 11
#define LED_B 12
#define MATRIX_PIN 7

extern Matriz_leds_config* numeros[];

// Controla o tempo de debounce dos botões
static uint32_t last_time = 0;

static volatile uint8_t number;

static volatile bool led_g_state = false;
static volatile bool led_b_state = false;

// Configuração para a matriz de LEDs
PIO pio;
uint sm;

// Buffer para manipular o display
uint8_t ssd[ssd1306_buffer_length];

// Define a área que será renderizada no display
struct render_area frame_area = {
    start_column : 0,
    end_column : ssd1306_width - 1,
    start_page : 0,
    end_page : ssd1306_n_pages - 1
};

// Protótipos das funções
static void gpio_irq_handler(uint gpio, uint32_t events);

static bool debounce_time(uint32_t *last_time);

void button_init(uint pin);

void led_init(uint8_t pin);
void blink(uint8_t pin);

extern void calculate_render_area_buffer_length(struct render_area *area);
extern void ssd1306_send_command(uint8_t cmd);
extern void ssd1306_send_command_list(uint8_t *ssd, int number);
extern void ssd1306_send_buffer(uint8_t ssd[], int buffer_length);
extern void ssd1306_init();
extern void ssd1306_scroll(bool set);
extern void render_on_display(uint8_t *ssd, struct render_area *area);
extern void ssd1306_set_pixel(uint8_t *ssd, int x, int y, bool set);
extern void ssd1306_draw_line(uint8_t *ssd, int x_0, int y_0, int x_1, int y_1, bool set);
extern void ssd1306_draw_char(uint8_t *ssd, int16_t x, int16_t y, uint8_t character);
extern void ssd1306_draw_string(uint8_t *ssd, int16_t x, int16_t y, char *string);
extern void ssd1306_command(ssd1306_t *ssd, uint8_t command);
extern void ssd1306_config(ssd1306_t *ssd);
extern void ssd1306_init_bm(ssd1306_t *ssd, uint8_t width, uint8_t height, bool external_vcc, uint8_t address, i2c_inst_t *i2c);
extern void ssd1306_send_data(ssd1306_t *ssd);
extern void ssd1306_draw_bitmap(ssd1306_t *ssd, const uint8_t *bitmap);

int main() {
    char input;

    stdio_init_all();

    // Inicializa os componentes
    button_init(BTN_A);
    button_init(BTN_B);
    led_init(LED_G);
    led_init(LED_B);

    // Configuração da Matriz
    pio = pio0;
    sm = configurar_matriz(pio, MATRIX_PIN);
   
    // Inicializa o display
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SDL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SDL);

    ssd1306_init();

    // Determina a área a ser renderizada no display
    calculate_render_area_buffer_length(&frame_area);

    // Zera o buffer do display
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    // Desliga a matriz de LEDs
    imprimir_desenho(*numeros[10], pio, sm);

    // Configura interrupções para os botões
    gpio_set_irq_enabled_with_callback(BTN_A, GPIO_IRQ_EDGE_FALL, true, gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BTN_B, GPIO_IRQ_EDGE_FALL, true, gpio_irq_handler);

    while (true) {
        scanf("%c", &input);
        printf("%c", input);

        // Limpa o display
        memset(ssd, 0, ssd1306_buffer_length);
        
        // Verifica se a entrada é um número
        if (input >= '0' && input <= '9'){
            imprimir_desenho(*numeros[input - '0'], pio, sm);
        } else {
            imprimir_desenho(*numeros[10], pio, sm);
        }

        // Exibe o caractere inserido no display
        ssd1306_draw_char(ssd, 10, 10, input);
        render_on_display(ssd, &frame_area);    
        sleep_ms(1000);
    }
}

// Configura um botão
void button_init(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
}

void led_init(uint8_t pin){
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
}

void blink(uint8_t pin) {
    gpio_put(pin, true);
    sleep_ms(100); // Mantém o LED aceso por 100 ms
    gpio_put(pin, false);
    sleep_ms(100); // Mantém o LED apagado por 100 ms
}

static void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    // Define um vetor para armazenar mensagens
    char *text[2] ={ "", "" };

    // Aplica o debounce
    if(debounce_time(&last_time)) {
        // Quando o botão A é pressionado
        if(gpio == BTN_A) {  
            led_g_state = !led_g_state;
            gpio_put(LED_G, led_g_state);

            if (led_g_state) {
                text[0] = "  LED Verde  ";
                text[1] = "   ligado  ";
                printf("LED verde ligado\n");
            } else {
                text[0] = "  LED Verde  ";
                text[1] = "  desligado  ";
                printf("LED verde desligado\n");
            }
        } else if (gpio == BTN_B) { // Quando o botão B é pressionado
            led_b_state = !led_b_state;
            gpio_put(LED_B, led_b_state);

            if (led_b_state) {
                text[0] = "  LED Azul  ";
                text[1] = "   ligado  ";
                printf("LED azul ligado\n");
            } else {
                text[0] = "  LED Azul  ";
                text[1] = "  desligado  ";
                printf("LED azul desligado\n");
            }
        }
    }

    // Exibe o texto no display
    int y = 0;
    for(uint8_t i = 0; i < count_of(text); i++) {
        ssd1306_draw_string(ssd, 5, y, text[i]);
        y += 8;
    }
    render_on_display(ssd, &frame_area);
}

static bool debounce_time(uint32_t *last_time) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    if(current_time - *last_time > 200000) {
        *last_time = current_time;
        return true;
    }
    return false;
}
