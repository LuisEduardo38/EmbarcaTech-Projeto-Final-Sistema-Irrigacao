//Bibliotecas para funcionamento em geral do código
#include <stdio.h>
#include <time.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"

//Bibliotecas para funcionamento do display1306
#include "Bibliotecas/font.h"
#include "Bibliotecas/ssd1306.h"
#include "hardware/i2c.h"

//Bibliotecas para funcionamento da matriz de led WS2818B
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "ws2818b.pio.h"

//Parâmetro para configurações do display1306
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

//Declaração das variáveis com os pinos dos leds
const uint8_t led_red_pino = 13;
const uint8_t led_blue_pino = 12;
const uint8_t led_green_pino = 11;
bool estado_blue = false;
bool estado_green = false;

//Declaração das variáveis com os pinos dos botões
const uint8_t btn_a = 5;
const uint8_t btn_b = 6;
const uint8_t btn_j = 22;

//Declaração das variáveis com os pinos da matriz juntamento com a quantidade de leds
const uint8_t matriz_pino = 7;
const uint8_t numero_led = 25;

//Declaração das variáveis globais
//Controle de telas do display 1306
volatile bool primeira_tela = true;
volatile bool segunda_tela = false;
volatile bool limpar_tela = false;

//Diversos
volatile uint32_t ultimo_tempo = 0;
struct repeating_timer timer_temperatura;

//Declaração de variáveis do tipo bool para controle de acionanto dos atuadores
bool trava_reservatorio = false;
bool trava_irrigacao = false;

//Declaração dos frames para uso na matriz de leds
int face01 [] = {1, 1, 1, 1, 1,
                 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0,
                 0, 0, 0, 0, 0};

int face02 [] =  {1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1,
                  0, 0, 0, 0, 0,
                  0, 0, 0, 0, 0,
                  0, 0, 0, 0, 0};


int face03 [] =  {1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1,
                  0, 0, 0, 0, 0,
                  0, 0, 0, 0, 0};

int face04 [] =  {1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1,
                  0, 0, 0, 0, 0};

int face05 [] =  {1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1};

/*
//Configurações para o sensor de temperatura LM35
//Declaração de variáveis
const uint8_t sensor_temperatura_pino = 26;
const uint16_t amd_ref = 4096;
const float voltagem_ref = 3.3;
const float escola_lm35 = 0.01;

//Inicializando a biblioteca e pinos
adc_init();
adc_gpio_init(sensor_temperatura_pino);
adc_select_input(0);

//Processando os dados coletado
uint16_t resultado = adc_read();//Lendo os dados coletados via ADC
float voltagem = (resultado / adc_ref) * voltagem_ref;//Convertendo para vontagem
float temperaturaC = voltagem / escola_lm35;//Colocando na escala do sensor

//Configurações para o sensor de nível de água
//Declaração de variáveis
const uint8_t sensor_agua_pino = 27;
const uint16_t adc_ref = 4096;
const float voltagem_ref = 3.3;
const float convercao_adc = voltagem_red / (adc_ref - 1);

//Inicializando a biblioteca e pinos
adc_init();
adc_gpio_init(sensor_agua_pino);
adc_select_input(1);

//Processando os dados coletado
uint16_t resultado = adc_read();//Lendo os dados coletados via ADC
float voltagem = resultado * convercao_adc;//Convertendo para vontagem
float nivel_agua = voltagem * 10;//A cada 1V será 10cm de água

//Configurações para o sensor de umidade do solo
//Declaração de variáveis
//const uint8_t sensor_umidade_pino = 27;
Const uint8_t sensor_umidade_pino = 20;

//Inicializando a biblioteca e pinos
//adc_init();
//adc_gpio_init(sensor_umidade_pino);
//adc_select_input(0);
gpio_init(sensor_umidade_pino);
gpio_set_dir(sensor_umidade_pino, GPIO_IN);

//Processando os dados coletado
//uint16_t umidade = adc_read();//Lendo os dados coletados via ADC
int umidade = gpio_get(sensor_umidade_pino);
*/

//Protótipos das funções do código
void iniciar_pinos();
void gpio_irq_handler(uint gpio, uint32_t events);
uint8_t sensor_reservatorio_agua(uint8_t reservatorio_atual);
bool alterna_temperatura(struct repeating_timer *temperatura);
void imprimir_status(uint8_t temperatura, int *numero, PIO pio, int sm);
void manipulacao_matriz(uint8_t umidade, uint8_t temperatura, PIO pio, int sm);
void sensor_umidade_solo(uint8_t *umidade_atual, uint8_t *reservatorio_atual, uint8_t temperatura_atual);

//Função principal do código
int main(){
    //Inicializando as bibliotecas
    stdio_init_all();
    iniciar_pinos();
    srand(time(NULL));

    //Declaração das interrupções
    gpio_set_irq_enabled_with_callback(btn_a, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);//reiniciar o SE
    gpio_set_irq_enabled_with_callback(btn_b, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);//Trocar de tela
    gpio_set_irq_enabled_with_callback(btn_j, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);//Entra no modo bootsel

    //Configurações o display1306
    i2c_init(I2C_PORT, 400 * 1000);//Iniciando o barramento I2C com o clock de 400khz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);//Configuração do pino para funciona o I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);//Configuração do pino para funciona o I2C
    gpio_pull_up(I2C_SDA);//Ativando os resistores pull-up muito importante para comunicação I2C
    gpio_pull_up(I2C_SCL);//Ativando os resistores pull-up muito importante para comunicação I2C
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);//Iniciando o display
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    //Configurações da matriz de led WS2818B
    PIO pio = pio0;//Declaração do bloco PIO e escolhendo o PIO0 para rodar o código
    int sm = 0;//Declando uma máquina de estado do bloco PIO e escolhendo a máquina 0
    uint offset = pio_add_program(pio, &ws2818b_program);//Carregando o programa na memoria do pio e seu offset
    ws2818b_program_init(pio, sm, offset, matriz_pino, 800000);//Iniciando a matriz com os parâmetros configurado

    //Declaração das variáveis para controle dos sensores
    uint8_t umidade_solo = 50;//Variáveis que irá armazenar os dados do sensor de umidade do solo
    uint8_t reservatorio_agua = 100;//Variáveis que irá armazenar os dados do sensor de nível da água
    uint8_t temperatura_ambiente = 15;//Variáveis que irá armazenar os dados do sensor de temperatura ambiente

    //Declaração de string para apresentar os dados dos sensores
    char umidade [3];
    char reservatorio [3];
    char temperatura [3];

    //Alarme para mudar constantemente a temperatua
    add_repeating_timer_ms(5000, alterna_temperatura, &temperatura_ambiente, &timer_temperatura);

    //Loop principal
    while(true){
        //Chando a função para coletar dado do nível da água e armazenando na variável
        reservatorio_agua = sensor_reservatorio_agua(reservatorio_agua);

        //Chama a função que irá fazer leitura e processar dados do código de umidade do solo e nível da água
        sensor_umidade_solo(&umidade_solo, &reservatorio_agua, temperatura_ambiente);

        //Enviar dados a matriz de led com base na temperatura e nível da água
        manipulacao_matriz(umidade_solo, temperatura_ambiente, pio, sm);

        //IF usa trava de irrigação para saber se está ligado se sim ligando ou desligado o led
        if(trava_irrigacao == false){
            estado_green = true;
        }
        else{
            estado_green = false;
        }

        //IF usa trava de reservatório para saber se está ligado se sim ligando ou desligado o led
        if(trava_reservatorio == false){
            estado_blue = true;
        }
        else{
            estado_blue = false;
        }

        //Converção dos dados das variáveis dos sensores para string para serem exibidos no display
        sprintf(umidade, "%d", umidade_solo);
        sprintf(reservatorio, "%d", reservatorio_agua);
        sprintf(temperatura, "%d", temperatura_ambiente);
        
        //IF para limpar a tela antes de troca para a segunda tela com a temperatura
        if(limpar_tela == true){
            ssd1306_fill(&ssd, false);
            ssd1306_send_data(&ssd);
            limpar_tela = false;
        }

        //IF para controlar quais das tela será apresentado ao usuário
        if(primeira_tela == true){//Com a primeira tela sendo para exibir dados de nível da água e umidade do solo
            ssd1306_draw_string(&ssd, "SISTEMA DE", 26, 1);
            ssd1306_draw_string(&ssd, "IRRIGACAO", 26, 10);
            ssd1306_draw_string(&ssd, "RESERTORIO", 27, 25);
            ssd1306_draw_string(&ssd, "  :100", 48, 35);
            ssd1306_draw_string(&ssd, reservatorio, 38, 35);
            ssd1306_draw_string(&ssd, "UMIDADE SOLO", 14, 45);
            ssd1306_draw_string(&ssd, "  :100", 48, 55);
            ssd1306_draw_string(&ssd, umidade, 38, 55);
        }
        else if(segunda_tela == true){//Com a segunda tela sendo para exibir a temperatura do ambiente
            ssd1306_draw_string(&ssd, "SISTEMA DE", 26, 1);
            ssd1306_draw_string(&ssd, "IRRIGACAO", 26, 10);
            ssd1306_draw_string(&ssd, "TEMPERATURA", 20, 30);
            ssd1306_draw_string(&ssd, "C", 75, 45);
            ssd1306_draw_string(&ssd, temperatura, 55, 45);
        }
        else{//Terceira tela com o estados dos atuadores
            ssd1306_draw_string(&ssd, "SISTEMA DE", 26, 1);
            ssd1306_draw_string(&ssd, "IRRIGACAO", 26, 10);
            ssd1306_draw_string(&ssd, "BOMBA dAGUA", 20, 25);
            if(trava_reservatorio == false){
                ssd1306_draw_string(&ssd, "LIGANDO  ", 30, 35);
            }
            else{
                ssd1306_draw_string(&ssd, "DESLIGADO", 30, 35);
            }
            ssd1306_draw_string(&ssd, "IRRIGADOR", 25, 45);
            if(trava_irrigacao == false){
                ssd1306_draw_string(&ssd, " LIGANDO  ", 25, 55);
            }
            else{
                ssd1306_draw_string(&ssd, "DESLIGADO", 30, 55);
            }
        }

        //Alterando o estado do led RGB com base nos dados dos sensores
        pwm_set_gpio_level(led_red_pino, 100 * temperatura_ambiente);//O led red irá aumentar ou diminir a sua intensidade com base na temperatura do ambiente através do PWM
        gpio_put(led_blue_pino, estado_blue);
        gpio_put(led_green_pino, estado_green);
        
        //Atualiza o display
        ssd1306_send_data(&ssd);
        
        //Delay do código
        sleep_ms(500);
    }
}

//Função para iniciar os pinos da placa
void iniciar_pinos(){
    //Configuração dos leds para indicar o acionamento da bomba e do irrigador
    gpio_init(led_blue_pino);
    gpio_init(led_green_pino);
    gpio_set_dir(led_blue_pino, GPIO_OUT);
    gpio_set_dir(led_green_pino, GPIO_OUT);
    gpio_put(led_blue_pino, estado_blue);
    gpio_put(led_green_pino, estado_green);

    //Configuração dos leds para ajuste via PWM
    gpio_set_function(led_red_pino, GPIO_FUNC_PWM);
    uint slice_numero_red;
    slice_numero_red = pwm_gpio_to_slice_num(led_red_pino);
    pwm_set_clkdiv(slice_numero_red, 4.0);
    pwm_set_wrap(slice_numero_red, 4000);
    pwm_set_gpio_level(led_red_pino, 100);
    pwm_set_enabled(slice_numero_red, true);

    //Configurações dos botões da placa
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

//Função para tratamento de rotinas de interrupções
void gpio_irq_handler(uint gpio, uint32_t events){
    //Pega o tempo atual dos temporizadores da placa
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    //Esse IF servi conter o debounce do botões
    if(tempo_atual - ultimo_tempo > 200){
        ultimo_tempo = tempo_atual;
        //IFs para indicar qual botão foi pressionado
        if(gpio == 5){//Para reiniciar o código caso haja a necessidade
            watchdog_reboot(0, 0, 0);
        }
        else if(gpio == 6){//Mudanças em bool para permiti a troca correta das telas
            if((primeira_tela == true) && (segunda_tela == false)){//Primeira tela
                primeira_tela = false;
                segunda_tela = true;
                limpar_tela = true;
            }
            else if((primeira_tela == false) && (segunda_tela == true)){//Segunda tela
                primeira_tela = false;
                segunda_tela = false;
                limpar_tela = true;
            }
            else{//Terceira tela
                primeira_tela = true;
                segunda_tela = false;
                limpar_tela = true;
            }
            
        }
        else if(gpio == 22){//Entra no modo BootSel
            reset_usb_boot(0, 0);
        }
    }
}

//Função que irá coletar dados do nível da água como também será responsável por preencher novamente o reservatório
uint8_t sensor_reservatorio_agua(uint8_t reservatorio_atual){
    //Neste condicional se o nível estiver abaixa de 90% e a trava for FALSE vai entrar
    if((reservatorio_atual < 90) && (trava_reservatorio == false)){
        reservatorio_atual = reservatorio_atual + 5;//Aqui será preenchindo novamente o reservatório
        if(reservatorio_atual > 90){//Aqui se o nível do reservatório passar de 90 a trava será ativada novamente para não encher de mais o reservatório
            trava_reservatorio = true;
        }
    }
    //Nesta outra condicional está vereficando se o nível do reservatório está muito baixo, caso esteja muda a trava para FALSE
    else if((reservatorio_atual < 15) && (trava_reservatorio == true)){
        trava_reservatorio = false;
    }

    //rertornado o valor reservatório_atual
    return reservatorio_atual;
}

//Função para coletar e processar dados de umidade e água
void sensor_umidade_solo(uint8_t *umidade_atual, uint8_t *reservatorio_atual, uint8_t temperatura_atual){
    //Neste condicional está vereficando se a umidade do solo é inferior a 75 e a trava está FALSE
    if((*umidade_atual < 80) && (trava_irrigacao == false)){
        *umidade_atual = *umidade_atual + 2;//Incrementado mais 1 a umidade do solo
        *reservatorio_atual = *reservatorio_atual - 3;//Para cada uma unidade de umidade de solo irá decrementar menos 3 do reservatório
        if(*umidade_atual >= 80){//Se a umidade for igual ou maior a 75% irá mudar a trava para TRUE
            trava_irrigacao = true;
        }
    }//Caso as condições não sejam atendidas no primeiro IF entrarar neste ELSE
    else{
        //IF para processar quanto será o depreciamento da umidade do solo com base na temperatura
        if(temperatura_atual > 35){
            *umidade_atual = *umidade_atual - 4;
            *reservatorio_atual = *reservatorio_atual - 1;//Para simular um gotejamento devido a alta temperatura
        }
        else if(temperatura_atual > 30){
            *umidade_atual = *umidade_atual - 3;
        }
        else if(temperatura_atual > 25){
            *umidade_atual = *umidade_atual - 2;
        }
        else{
            *umidade_atual = *umidade_atual - 1;
        }
        
        //IF para mudar a trava caso a umidade caiu para menos que 40%
        if(*umidade_atual < 30){
            trava_irrigacao = false;
        }
    }
}

//Função para manipular a matriz de led com base na temperatura ambiente
void manipulacao_matriz(uint8_t umidade, uint8_t temperatura, PIO pio, int sm){
    //Neste IF irá direcionar qual frame será exibido na matriz de led
    if(umidade >= 80){
        imprimir_status(temperatura, face05, pio, sm);
    }
    else if(umidade >= 70){
        imprimir_status(temperatura, face04, pio, sm);
    }
    else if(umidade >= 60){
        imprimir_status(temperatura, face03, pio, sm);
    }
    else if(umidade >= 40){
        imprimir_status(temperatura, face02, pio, sm);
    }
    else if(umidade >= 30){
        imprimir_status(temperatura, face01, pio, sm);
    }
}

//Função para imprimir os frames na matriz
void imprimir_status(uint8_t temperatura, int *numero, PIO pio, int sm){
    uint8_t red, blue, green;

    //Neste IF irá decider qual será a cor dos leds com base na temperatura
    if(temperatura >= 35){
        red = 100;
        blue = 0;
        green = 0;
    }
    else if(temperatura >= 30){
        red = 100;
        blue = 100;
        green = 0;
    }
    else if(temperatura >= 25){
        red = 100;
        blue = 0;
        green = 100;
    }
    else if(temperatura >= 20){
        red = 0;
        blue = 100;
        green = 0;
    }
    else if(temperatura >= 15){
        red = 0;
        blue = 100;
        green = 100;
    }

    //For para percorrer todos os leds da matriz para ativar ou desativar os leds espessificos
    for(uint8_t i = 0; i < numero_led; i++){
        uint8_t RED = (uint8_t)(red * numero[i]);
        uint8_t BLUE = (uint8_t)(blue * numero[i]);
        uint8_t GREEN = (uint8_t)(green * numero[i]);

        pio_sm_put_blocking(pio, sm, GREEN);
        pio_sm_put_blocking(pio, sm, RED);
        pio_sm_put_blocking(pio, sm, BLUE);
    }
}

//Função para alterna a temperatura ambiente
bool alterna_temperatura(struct repeating_timer *temperatura){
    //Declaração de um ponteiro do mesmo tipo da variável quer alterar o valor
    uint8_t *tempe = (uint8_t *)temperatura->user_data;

    //Alterando os valores via ponteiro
    *tempe = 15 + rand() % (40 - 15 + 1);

    //Retornado TRUE para o alarme continuar funcionado
    return true;
}
