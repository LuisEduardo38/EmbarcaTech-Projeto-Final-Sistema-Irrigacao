#include <stdio.h>
#include <time.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"

#include "Bibliotecas/font.h"
#include "Bibliotecas/ssd1306.h"
#include "hardware/i2c.h"

#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "ws2818b.pio.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

const uint8_t led_red_pino = 13;
const uint8_t led_blue_pino = 12;
const uint8_t led_green_pino = 11;

const uint8_t btn_a = 5;
const uint8_t btn_b = 6;
const uint8_t btn_j = 22;

const uint8_t matriz_pino = 7;
const uint8_t numero_led = 25;

volatile uint32_t ultimo_tempo = 0;
volatile bool segunda_tela = true;
volatile bool limpar_tela = false;

volatile bool trava_temperatura = true;
volatile bool trava_tempo = true;

int umida [] = {1, 1, 1, 1, 1,
                1, 1, 1, 1, 1,
                1, 1, 1, 1, 1,
                1, 1, 1, 1, 1,
                1, 1, 1, 1, 1};

void iniciar_pinos();
uint8_t sensor_temperatura();
void gpio_irq_handler(uint gpio, uint32_t events);
uint8_t manipulacao_matriz(uint8_t numero, PIO pio, int sm);
uint8_t sensor_reservatorio_agua(uint8_t reservatorio_atual);
void imprimir_numero(uint8_t intesidade ,int *numero, PIO pio, int sm);
void sensor_umidade_solo(uint8_t *umidade_atual, uint8_t *reservatorio_atual, uint8_t *temperatura_atual);

int main(){
    stdio_init_all();
    iniciar_pinos();
    srand(time(NULL));

    gpio_set_irq_enabled_with_callback(btn_a, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(btn_b, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(btn_j, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2818b_program);
    ws2818b_program_init(pio, sm, offset, matriz_pino, 800000);

    uint8_t umidade_solo = 40;
    uint8_t reservatorio_agua = 100;
    uint8_t temperatura_ambiente = 0;

    uint8_t icones = 0;

    char umidade [2];
    char resertorio [2];
    char temperatura [2];

    while(true){
        if(trava_temperatura == true){
            temperatura_ambiente = sensor_temperatura();
            trava_temperatura = false;
        }

        reservatorio_agua = sensor_reservatorio_agua(reservatorio_agua);

        sensor_umidade_solo(&umidade_solo, &reservatorio_agua, &temperatura_ambiente);

        icones = manipulacao_matriz(icones, pio, sm);

        sprintf(umidade, "%d", umidade_solo);
        sprintf(resertorio, "%d", reservatorio_agua);
        sprintf(temperatura, "%d", temperatura_ambiente);
        
        if(limpar_tela == true){
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            limpar_tela = false;
        }

        if(segunda_tela == true){
            ssd1306_draw_string(&ssd, "SISTEMA DE", 26, 1);
            ssd1306_draw_string(&ssd, "IRRIGACAO", 26, 10);
            ssd1306_draw_string(&ssd, "RESERTORIO", 27, 25);
            ssd1306_draw_string(&ssd, "  :100", 48, 35);
            ssd1306_draw_string(&ssd, resertorio, 38, 35);
            ssd1306_draw_string(&ssd, "UMIDADE SOLO", 14, 45);
            ssd1306_draw_string(&ssd, "  :100", 48, 55);
            ssd1306_draw_string(&ssd, umidade, 38, 55);
        }
        else{
            ssd1306_draw_string(&ssd, "SISTEMA DE", 26, 1);
            ssd1306_draw_string(&ssd, "IRRIGACAO", 26, 10);
            ssd1306_draw_string(&ssd, "TEMPERATURA", 20, 30);
            ssd1306_draw_string(&ssd, "C", 75, 45);
            ssd1306_draw_string(&ssd, temperatura, 55, 45);
        }

        pwm_set_gpio_level(led_red_pino, 100 * temperatura_ambiente);
        pwm_set_gpio_level(led_blue_pino, 100 * reservatorio_agua);
        pwm_set_gpio_level(led_green_pino, 100 * umidade_solo);
        ssd1306_send_data(&ssd);
        
        sleep_ms(500);
    }
}

void iniciar_pinos(){
    gpio_set_function(led_red_pino, GPIO_FUNC_PWM);
    gpio_set_function(led_blue_pino, GPIO_FUNC_PWM);
    gpio_set_function(led_green_pino, GPIO_FUNC_PWM);
    uint slice_numero_blue, slice_numero_green, slice_numero_red;
    slice_numero_blue = pwm_gpio_to_slice_num(led_red_pino);
    slice_numero_blue = pwm_gpio_to_slice_num(led_blue_pino);
    slice_numero_green = pwm_gpio_to_slice_num(led_green_pino);
    pwm_set_clkdiv(slice_numero_red, 4.0);
    pwm_set_clkdiv(slice_numero_blue, 4.0);
    pwm_set_clkdiv(slice_numero_green, 4.0);
    pwm_set_wrap(slice_numero_red, 4000);
    pwm_set_wrap(slice_numero_blue, 10000);
    pwm_set_wrap(slice_numero_green, 10000);
    pwm_set_gpio_level(led_red_pino, 100);
    pwm_set_gpio_level(led_blue_pino, 100);
    pwm_set_gpio_level(led_green_pino, 100);
    pwm_set_enabled(slice_numero_red, true);
    pwm_set_enabled(slice_numero_blue, true);
    pwm_set_enabled(slice_numero_green, true);

    gpio_init(btn_a);
    gpio_init(btn_b);
    gpio_init(btn_j);
    gpio_set_dir(btn_a, GPIO_IN);
    gpio_set_dir(btn_b, GPIO_IN);
    gpio_set_dir(btn_j, GPIO_IN);
    gpio_pull_up(btn_a);
    gpio_pull_up(btn_b);
    gpio_pull_up(btn_j);
}

void gpio_irq_handler(uint gpio, uint32_t events){
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    if(tempo_atual - ultimo_tempo > 200){
        ultimo_tempo = tempo_atual;
        if(gpio == 5){
            printf("BTN_A\n");
            trava_temperatura = true;
        }
        else if(gpio == 6){
            printf("BTN_B\n");

            limpar_tela = true;
            segunda_tela = !segunda_tela;
        }
        else if(gpio == 22){
            printf("BTN_J\n");
            reset_usb_boot(0, 0);
        }
    }
}

uint8_t sensor_temperatura(){
    uint8_t sensor;

    sensor = 15 + rand() % (40 - 15 + 1);

    return sensor;
}

uint8_t sensor_reservatorio_agua(uint8_t reservatorio_atual){
    static bool trava_reservatorio = false;

    if((reservatorio_atual < 80) && (trava_reservatorio == false)){
        reservatorio_atual = reservatorio_atual + 7;
        if(reservatorio_atual > 80){
            trava_reservatorio = true;
        }
    }
    else if((reservatorio_atual < 20) && (trava_reservatorio == true)){
        trava_reservatorio = false;
    }

    return reservatorio_atual;
}

void sensor_umidade_solo(uint8_t *umidade_atual, uint8_t *reservatorio_atual, uint8_t *temperatura_atual){
    static bool trava = false;
    if((*umidade_atual < 60) && (trava == false)){
        *umidade_atual = *umidade_atual + 1;
        *reservatorio_atual = *reservatorio_atual - 2;
        if(*umidade_atual >= 60){
            trava = true;
        }
    }
    else{
        if(*temperatura_atual > 35){
            *umidade_atual = *umidade_atual - 4;
            *reservatorio_atual = *reservatorio_atual - 6;
        }
        else if(*temperatura_atual > 30){
            *umidade_atual = *umidade_atual - 3;
            *reservatorio_atual = *reservatorio_atual - 5;
        }
        else if(*temperatura_atual > 25){
            *umidade_atual = *umidade_atual - 2;
            *reservatorio_atual = *reservatorio_atual - 3;
        }
        else{
            *umidade_atual = *umidade_atual - 1;
            *reservatorio_atual = *reservatorio_atual - 2;
        }
        
        if(*umidade_atual < 20){
            trava = false;
        }
    }
}

uint8_t manipulacao_matriz(uint8_t numero, PIO pio, int sm){
    if(numero <= 3){
        imprimir_numero(numero, umida, pio, sm);
        numero++;
    }
    else if(numero <= 6){
        imprimir_numero(numero, umida, pio, sm);
        numero++;
    }
    else if(numero <= 9){
        imprimir_numero(numero, umida, pio, sm);
        numero++;
    }
    else{
        numero = 0;
    }

    return numero;
}

void imprimir_numero(uint8_t intensidade,int *numero, PIO pio, int sm){
    uint8_t red, blue, green;
    if((intensidade >= 0) && (intensidade <= 2)){
        red = 0;
        blue = 100;
        green = 0;
    }
    else if((intensidade >= 3) && (intensidade <= 5)){
        red = 0;
        blue = 0;
        green = 100;
    }
    else if((intensidade >= 6) && (intensidade <= 8)){
        red = 100;
        blue = 0;
        green = 0;
    }

    for(uint8_t i = 0; i < numero_led; i++){
        uint8_t RED = (uint8_t)(red * numero[i]);
        uint8_t BLUE = (uint8_t)(blue * numero[i]);
        uint8_t GREEN = (uint8_t)(green * numero[i]);

        pio_sm_put_blocking(pio, sm, GREEN);
        pio_sm_put_blocking(pio, sm, RED);
        pio_sm_put_blocking(pio, sm, BLUE);
    }
}