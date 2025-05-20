#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include "RVpio.pio.h"

//definição das constantes relativas ao endereçamento do display
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

//definição dos pinos a utilizar
#define ADC_JOYSTICK_X 26
#define ADC_JOYSTICK_Y 27
#define LED_RED 13
#define LED_BLUE 12
#define LED_GREEN  11
#define BUZZER_A 10
#define BUZZER_B 21
#define tam_quad 10

//tamanho da matriz de LEDs
#define matrix 25

//parâmetros do pwm -> para controle do buzzer / os LEDs foram controlados com outro wrap
#define PWM_WRAP 2500
#define PWM_CLKDIV 125


//estrutura para armazenar as posições do joystick
typedef struct
{
    uint16_t x_pos;
    uint16_t y_pos;
} joystick_data_t;

//estrutura para armazenar os parâmetros da PIO
typedef struct pio_refs{
    PIO address;
    int state_machine;
    int offset;
    int pin;
} pio;

//estrutura para faciliar a manipulação de cor da matriz de LEDs
typedef struct rgb{
    double red;
    double green;
    double blue;
} rgb;

// estrutura para armazenar o desenho e as cores da matriz de LEDs
typedef struct drawing {
    double figure[matrix];
    rgb main_color; 
    rgb background_color; 
} sketch;

// fila para organizar e enviar os dados de posição do joystick
QueueHandle_t xQueueJoystickData;

//tarefa do joystick
void vJoystickTask(void *params)
{
    //inicia o ADC nos pinos fornecidos
    adc_gpio_init(ADC_JOYSTICK_Y);
    adc_gpio_init(ADC_JOYSTICK_X);
    adc_init();

    //inicia a estrutura de armazenamento das posições
    joystick_data_t joydata;

    while (true)
    {
        adc_select_input(0); // GPIO 26 = ADC0
        joydata.y_pos = adc_read();

        adc_select_input(1); // GPIO 27 = ADC1
        joydata.x_pos = adc_read();

        xQueueSend(xQueueJoystickData, &joydata, 0); // Envia o valor do joystick para a fila
        vTaskDelay(pdMS_TO_TICKS(100));              // 10 Hz de leitura
    }
}

//função que recebe a estrutura de cores
//e as organiza para enviar para a PIO
uint32_t rgb_matrix(rgb color){
    unsigned char r, g, b;
    r = color.red* 255;
    g = color.green * 255;
    b = color.blue * 255;
    return (g << 24) | (r << 16) | (b << 8);
}

//função que desenha na matriz de LEDs
void draw(sketch my_sketch, pio my_pio, uint8_t vector_size){
    uint32_t color = 0;
    for(int8_t i = 0; i < vector_size; i++){
        if (my_sketch.figure[i] == 1)
           color = rgb_matrix(my_sketch.main_color);
        else
            color = rgb_matrix(my_sketch.background_color);
        pio_sm_put_blocking(my_pio.address, my_pio.state_machine, color);
    }
}

//função que controla os buzzers A e B
void vBuzzerTask(void *params){
    //inicia o pwm para o buzzer A
    gpio_set_function(BUZZER_A, GPIO_FUNC_PWM);
    uint8_t slice_a = pwm_gpio_to_slice_num(BUZZER_A);
    pwm_set_wrap(slice_a, PWM_WRAP);
    pwm_set_clkdiv(slice_a, PWM_CLKDIV);
    pwm_set_enabled(slice_a, true);

    //inicia o pwmpara o buzzer B
    gpio_set_function(BUZZER_B, GPIO_FUNC_PWM);
    uint8_t slice_b = pwm_gpio_to_slice_num(BUZZER_B);
    pwm_set_wrap(slice_b, PWM_WRAP);
    pwm_set_clkdiv(slice_b, PWM_CLKDIV);
    pwm_set_enabled(slice_b, true);
    pwm_set_gpio_level(BUZZER_A, 0);
    pwm_set_gpio_level(BUZZER_B, 0);

    joystick_data_t joydata;
    while(true){
        //recebe da fila as posições do joystick
        if (xQueueReceive(xQueueJoystickData, &joydata, portMAX_DELAY) == pdTRUE){

            //translada o valor lido pelo adc para a posição com referencial no centro
            int16_t x = joydata.x_pos - 2040;
            int16_t y = joydata.y_pos - 2040;

            // recebe os valores absolutos de x e y
            if (x < 0)
                x = -x;
            if (y < 0)
                y = -y;

            //caso o eixo y esteja a mais de 70% de distância do centro
            // caso de chuva preocupante
            if (y > 2040 * 0.8){ 
                //liga e desliga o buzzer - tocando à medida que o laço estiver sendo satisfeito
                pwm_set_gpio_level(BUZZER_A, 50);
                sleep_ms(100);
                pwm_set_gpio_level(BUZZER_A, 0);
                sleep_ms(100);
            } else if (x > 2040 * 0.7){
                //caso do eixo x a mais de 80% de distância do centro
                //caso da água preocupante
                //liga e desliga o buzzer - tocando à medida que o laço estiver sendo satisfeito
                pwm_set_gpio_level(BUZZER_B, 50);
                sleep_ms(100);
                pwm_set_gpio_level(BUZZER_B, 0);
                sleep_ms(100);
            } else {
                // caso para nenhum dos eixos em estado preocupante
                //liga e desliga o buzzer - tocando à medida que o laço estiver sendo satisfeito
                //no entanto, o buzzer soa mais fraco e espaçado
                pwm_set_gpio_level(BUZZER_A, 10);
                pwm_set_gpio_level(BUZZER_B, 10);
                float div = ((float) (x + y) / 2040);
                //verifica se está em cerca de metade da distância, aumentando a frequência de toque
                sleep_ms( div > 0.45 ? 200 : 500);
                pwm_set_gpio_level(BUZZER_A, 0);
                pwm_set_gpio_level(BUZZER_B, 0);
                sleep_ms( div > 0.45 ? 200 : 500);
            }
        }
    }
}

//função que trata da matriz de LEDs
void vMatrixTask(void *params){

    //início da PIO
    pio my_pio = {
        .pin = 7,
        .address = 0,
        .offset = 0,
        .state_machine = 0
    };

    //início do desenho de exclamação - será utilizado para os casos de alerta
    sketch my_sketch = {
        .figure = {
            0, 0, 1, 0, 0,
            0, 0, 0, 0, 0,
            0, 0, 1, 0, 0,
            0, 1, 1, 1, 0,
            1, 1, 1, 1, 1
        },
        .main_color = {
            .blue = 0.0,
            .red = 0.05,
            .green = 0.0
        },
        .background_color = {
            .blue = 0.0,
            .red = 0.01,
            .green = 0.01
        }
    };

    //desenhos alternativos - para casos de não-alerta
    sketch erase = {
        .figure = {
            1, 1, 1, 1, 1,
            1, 0, 0, 0, 1,
            1, 0, 1, 0, 1,
            1, 0, 0, 0, 1,
            1, 1, 1, 1, 1
        },
        .main_color = {
            .blue = 0.0,
            .red = 0.0,
            .green = 0.0
        },
        .background_color = {
            .blue = 0.0,
            .red = 0.00,
            .green = 0.00
        }
    };

    //configuração da PIO
    my_pio.address = pio0;
    if (!set_sys_clock_khz(128000, false))
        printf("\nclock errado!\n");
    my_pio.offset = pio_add_program(my_pio.address, &pio_review_program);
    my_pio.state_machine = pio_claim_unused_sm(my_pio.address, true);

    pio_review_program_init(my_pio.address, my_pio.state_machine, my_pio.offset, my_pio.pin);

    joystick_data_t joydata;

    while(true){
        //verifica a fila para receber as posições do joystick
        if(xQueueReceive(xQueueJoystickData, &joydata, portMAX_DELAY) == pdTRUE){
            //translada as posições para uma referência do centro
            int16_t x = (int16_t) joydata.x_pos - 2040;
            int16_t y = (int16_t) joydata.y_pos - 2040;
            //obtém os valores absolutos
            if (x < 0)
                x = -x;
            if (y < 0)
                y = -y;
            

            if (y > 2040 * 0.8){
                //caso alarmante de chuva
                //desenha uma exclamação na matriz
                draw(my_sketch, my_pio, matrix);
            } else if (x > 2040 * 0.7){
                //caso alarmante de água
                //desenha uma exclamação na matriz
                draw(my_sketch, my_pio, matrix);
            } else {
                //caso não alarmante
                float div = ((float) (x + y) / 2040);
                rgb color;
                //verifica uma posição preocupante (muita chuva ou muita água/ ou chuva + água >= 45%)
                if (div < 0.45) {
                    color.blue = 0.0;
                    color.green = 0.01;
                    color.red = 0.0;
                } else {
                    color.blue = 0.0;
                    color.green = 0.01;
                    color.red = 0.01;
                }
                erase.background_color = color;
                draw(erase, my_pio, matrix);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

}

//apaga apenas uma região do display
void ssd1306_fill_rect(ssd1306_t *ssd, bool value, uint8_t left, uint8_t right, uint8_t bottom, uint8_t top) {
    // Itera por todas as posições do display
    for (uint8_t y = bottom; y < top; ++y) {
        for (uint8_t x = left; x < right; ++x) {
            ssd1306_pixel(ssd, x, y, value);
        }
    }
}

void vDisplayTask(void *params)
{
    //inicia a I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    //inicia o display
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    joystick_data_t joydata;
    bool cor = true;

    ssd1306_line(&ssd, 3, 25, 123, 25, cor);           // Desenha uma linha
    while (true)
    {
        //recebe da fila as posição em y e x
        if (xQueueReceive(xQueueJoystickData, &joydata, portMAX_DELAY) == pdTRUE)
        {
            //translada x e y para uma posição central
            int16_t x = joydata.x_pos - 2020;
            int16_t y = joydata.y_pos - 2020;

            //obtém o valor absoluto
            if (x < 0)
                x = -x;
            if (y < 0)
                y = -y;

            //normaliza para os casos de x e y e obtém a porcentagem
            int chuva = ((float) y / 2048) * 100;
            int agua = ((float) x / 2048) * 100;
            //cria os buffers de armazenamento das strings
            char buffer_x[20];
            char buffer_y[20];
            //grava as strings com os valores obtidos nos buffers
            sprintf(buffer_x, "CHUVA: %d\b\b%%", chuva);
            sprintf(buffer_y, "AGUA: %d\b\b%%", agua);
            //limpa a porção inferior do display
            ssd1306_fill_rect(&ssd, !cor, 4, 128 - 4, 26, 64 - 3);
            
            if (y > 2040 * 0.8){
                //limpa a porção superior do display
                ssd1306_fill_rect(&ssd, !cor, 0, 128, 0, 25);
                ssd1306_draw_string(&ssd, "PERIGO!!!", 20, 6); // Desenha uma string
                ssd1306_draw_string(&ssd, "MUITA CHUVA!", 8, 16);  // Desenha uma string
            } else if (x > 2040 * 0.7){
                //limpa a porção superior do display
                ssd1306_fill_rect(&ssd, !cor, 0, 128, 0, 25);
                ssd1306_draw_string(&ssd, "PERIGO!!!", 20, 6); // Desenha uma string
                ssd1306_draw_string(&ssd, "TRANSBORDO!", 8, 16);  // Desenha uma string
            } else {
                //limpa a porção superior do display
                ssd1306_fill_rect(&ssd, !cor, 0, 128, 0, 25);
                ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6); // Desenha uma string
                ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16);  // Desenha uma string
            }

            //escreve os valores de porcentagem de chuva e nível de água
            ssd1306_draw_string(&ssd, buffer_x, 2, 33); // Desenha uma string
            ssd1306_draw_string(&ssd, buffer_y, 2, 48);  // Desenha uma string
            //ssd1306_rect(&ssd, y, x, tam_quad, tam_quad, cor, !cor); // Quadrado 5x5
            ssd1306_send_data(&ssd);
        }
    }
}

//tarefa que trata do LED verde
void vLedGreenTask(void *params)
{
    gpio_set_function(LED_GREEN, GPIO_FUNC_PWM);   // Configura GPIO como PWM
    uint slice = pwm_gpio_to_slice_num(LED_GREEN); // Obtém o slice de PWM
    pwm_set_wrap(slice, 100);                     // Define resolução (0–100)
    pwm_set_chan_level(slice, PWM_CHAN_B, 0);     // Duty inicial
    pwm_set_enabled(slice, true);                 // Ativa PWM

    joystick_data_t joydata;
    while (true)
    {
        if (xQueueReceive(xQueueJoystickData, &joydata, portMAX_DELAY) == pdTRUE)
        {
            // Brilho proporcional ao desvio do centro
            int16_t desvio_centro = (int16_t)joydata.x_pos - 2040;
            int16_t desvio_centro_y = (int16_t)joydata.y_pos - 2040;
            if (desvio_centro < 0)
                desvio_centro = -desvio_centro;
            if (desvio_centro_y < 0)
                desvio_centro_y = -desvio_centro_y;
            //verifica se a posição é anterior à 70% (x) e 80% (y) do centro
            if (desvio_centro < 2040 * 0.7 && desvio_centro_y < 2040 * 0.8){
                uint16_t pwm_value = (desvio_centro * 100) / 2048;
                //printf("Desvio centro: %d", desvio_centro);
                pwm_set_chan_level(slice, PWM_CHAN_B, pwm_value);
            } else {
                //printf("Desvio centro > 2048 * 0.75");
                pwm_set_chan_level(slice, PWM_CHAN_B, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Atualiza a cada 50ms
    }
}

//tarefa que trata do LED azul
void vLedBlueTask(void *params)
{
    gpio_set_function(LED_BLUE, GPIO_FUNC_PWM);   // Configura GPIO como PWM
    uint slice = pwm_gpio_to_slice_num(LED_BLUE); // Obtém o slice de PWM
    pwm_set_wrap(slice, 100);                     // Define resolução (0–100)
    pwm_set_chan_level(slice, PWM_CHAN_A, 0);     // Duty inicial
    pwm_set_enabled(slice, true);                 // Ativa PWM

    joystick_data_t joydata;
    while (true)
    {
        if (xQueueReceive(xQueueJoystickData, &joydata, portMAX_DELAY) == pdTRUE)
        {
            // Brilho proporcional ao desvio do centro
            int16_t desvio_centro = (int16_t)joydata.y_pos - 2040;
            int16_t desvio_centro_x = (int16_t)joydata.x_pos - 2040;
            if (desvio_centro < 0)
                desvio_centro = -desvio_centro;
            if (desvio_centro_x < 0)
                desvio_centro_x = -desvio_centro_x;
            //verifica se a posição é anterior à 70% (x) e 80% (y) do centro
            if (desvio_centro < 2040 * 0.8 && desvio_centro_x < 2040 * 0.7){
                uint16_t pwm_value = (desvio_centro * 100) / 2048;
                pwm_set_chan_level(slice, PWM_CHAN_A, pwm_value);
            } else {
                pwm_set_chan_level(slice, PWM_CHAN_A, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Atualiza a cada 50ms
    }
}

//tarefa que trata do LED vermelho
void vLedRedTask(void *params)
{
    gpio_set_function(LED_RED, GPIO_FUNC_PWM);   // Configura GPIO como PWM
    uint slice = pwm_gpio_to_slice_num(LED_RED); // Obtém o slice de PWM
    pwm_set_wrap(slice, 100);                     // Define resolução (0–100)
    pwm_set_chan_level(slice, PWM_CHAN_B, 0);     // Duty inicial
    pwm_set_enabled(slice, true);                 // Ativa PWM

    joystick_data_t joydata;
    while (true)
    {
        if (xQueueReceive(xQueueJoystickData, &joydata, portMAX_DELAY) == pdTRUE)
        {
            // Brilho proporcional ao desvio do centro
            int16_t desvio_centro_y = (int16_t)joydata.y_pos - 2040;
            int16_t desvio_centro_x = (int16_t)joydata.x_pos - 2040;

            if (desvio_centro_y < 0)
                desvio_centro_y = -desvio_centro_y;
            if (desvio_centro_x < 0)
                desvio_centro_x = -desvio_centro_x;

            //verifica se a posição é posterior à 70% (x) e 80% (y) do centro
            if (desvio_centro_y > 2040 * 0.81 /*&& desvio_centro_x > 2024 * 0.7*/){
                uint16_t pwm_value = (desvio_centro_y * 100) / 2048;
                //printf("RED Y: %d\n", desvio_centro_y);
                pwm_set_gpio_level(LED_RED, 0);
                pwm_set_gpio_level(LED_RED, pwm_value);
            } else if (desvio_centro_x > 2040 * 0.71){
                uint16_t pwm_value = (desvio_centro_x * 100) / 2048;
                //printf("RED X: %d\n", desvio_centro_x);
                pwm_set_gpio_level(LED_RED, 0);
                pwm_set_gpio_level(LED_RED, pwm_value);
            } else {
                //printf("RED < 2048 * 0.75");
                pwm_set_gpio_level(LED_RED, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Atualiza a cada 50ms
    }
}

void vLedsYellowTask(void *params)
{

    joystick_data_t joydata;
    while (true)
    {
        if (xQueueReceive(xQueueJoystickData, &joydata, portMAX_DELAY) == pdTRUE)
        {
            // Brilho proporcional ao desvio do centro
            int16_t desvio_centro_y = (int16_t)joydata.y_pos - 2040;
            int16_t desvio_centro_x = (int16_t)joydata.x_pos - 2040;

            if (desvio_centro_y < 0)
                desvio_centro_y = -desvio_centro_y;
            if (desvio_centro_x < 0)
                desvio_centro_x = -desvio_centro_x;

            if (
                (desvio_centro_y > 2040 * 0.51 && desvio_centro_y < 2040 * 0.69) ||
                (desvio_centro_x > 2040 * 0.51 && desvio_centro_x < 2040 * 0.79)
                ){
                uint16_t pwm_value = 1000;
                pwm_set_gpio_level(LED_RED, 0);
                pwm_set_gpio_level(LED_GREEN, 0);
                pwm_set_gpio_level(LED_RED, pwm_value);
                pwm_set_gpio_level(LED_GREEN, pwm_value);
            }  else {
                pwm_set_gpio_level(LED_RED, 0);
                pwm_set_gpio_level(LED_GREEN, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Atualiza a cada 50ms
    }
}


// Modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

int main()
{
    // Ativa BOOTSEL via botão
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    stdio_init_all();

    // Cria a fila para compartilhamento de valor do joystick
    xQueueJoystickData = xQueueCreate(10, sizeof(joystick_data_t));

    // Criação das tasks
    xTaskCreate(vJoystickTask, "Joystick Task", 256, NULL, 1, NULL);
    xTaskCreate(vDisplayTask, "Display Task", 512, NULL, 1, NULL);
    xTaskCreate(vLedGreenTask, "LED red Task", 256, NULL, 1, NULL);
    xTaskCreate(vLedBlueTask, "LED blue Task", 256, NULL, 1, NULL);
    xTaskCreate(vLedRedTask, "LED red Task", 256, NULL, 1, NULL);
    xTaskCreate(vMatrixTask, "Matrix Task", 256, NULL, 1, NULL);
    xTaskCreate(vBuzzerTask, "Buzzer Task", 256, NULL, 1, NULL);
    //xTaskCreate(vLedsYellowTask, "LEDs yellow Task", 256, NULL, 1, NULL);
    // Inicia o agendador
    vTaskStartScheduler();
    panic_unsupported();
}
