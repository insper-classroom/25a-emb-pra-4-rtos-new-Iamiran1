/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

static volatile bool fired = false;
const int ECHO_PIN = 6;
const int TRIG_PIN = 7;

volatile bool echo_got = false;
volatile uint32_t start_us;
volatile uint32_t end_us;
QueueHandle_t xQueueTimeEchoStart, xQueueTimeEchoEnd, xQueueDistance;
SemaphoreHandle_t xSemaphoreTrigger;
alarm_id_t alarm;

int64_t alarm_callback(alarm_id_t id, void *user_data)
{
    fired = true;
    // Can return a value here in us to fire in the future
    return 0;
}

void pin_callback(uint gpio, uint32_t events)
{ // pin_callback: Função callback do pino do echo.
    if (gpio_get(ECHO_PIN) == 1)
    {
        start_us = get_absolute_time();
        xQueueSend(xQueueTimeEchoStart, &start_us, 0);
    }
    else if (gpio_get(ECHO_PIN) == 0)
    {
        end_us = get_absolute_time();
        xQueueSend(xQueueTimeEchoEnd, &end_us, 0);
        xSemaphoreGiveFromISR(xSemaphoreTrigger, 0);
    }
}
void echo_task(void *p)
{ // echo_task: Task que faz a leitura do tempo que o pino echo ficou levantado.
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN,
                                       GPIO_IRQ_EDGE_RISE |
                                           GPIO_IRQ_EDGE_FALL,
                                       true,
                                       &pin_callback);

    while (true)
    {
        float start = 0.0;
        float end = 0.0;
        uint32_t delta_t;
        if (xQueueReceive(xQueueTimeEchoStart, &start, 0))
        {
            printf("%d\n", start);
        }
        if (xQueueReceive(xQueueTimeEchoEnd, &end, 0))
        {
            cancel_alarm(alarm);
            printf("%d\n", end);
        }
        if (end > 0)
        {
            
            delta_t = end_us - start_us;
            float distancia = (float)delta_t * 0.017015;
            printf("%f",distancia);
            xQueueSend(xQueueDistance, &distancia, 0);
        }
    }
}

void trigger_task(void *p)
{ // trigger_task: Task responsável por gerar o trigger.
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    int delay = 5;
    while (true)
    {
        if (delay > 0)
        {
            gpio_put(TRIG_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(delay));
            gpio_put(TRIG_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(delay));
        }
    }
}
void oled1_btn_led_init(void)
{
    gpio_init(LED_1_OLED);
    gpio_set_dir(LED_1_OLED, GPIO_OUT);

    gpio_init(LED_2_OLED);
    gpio_set_dir(LED_2_OLED, GPIO_OUT);

    gpio_init(LED_3_OLED);
    gpio_set_dir(LED_3_OLED, GPIO_OUT);

    gpio_init(BTN_1_OLED);
    gpio_set_dir(BTN_1_OLED, GPIO_IN);
    gpio_pull_up(BTN_1_OLED);

    gpio_init(BTN_2_OLED);
    gpio_set_dir(BTN_2_OLED, GPIO_IN);
    gpio_pull_up(BTN_2_OLED);

    gpio_init(BTN_3_OLED);
    gpio_set_dir(BTN_3_OLED, GPIO_IN);
    gpio_pull_up(BTN_3_OLED);
}

void oled_task(void *p)
{
    printf("Inicializando Driver\n");
    ssd1306_init();
    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);
    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();
    char cnt = 15;

    while (1)
    {
        if (xSemaphoreTake(xSemaphoreTrigger, pdMS_TO_TICKS(500)) == pdTRUE)
        {
            float distance = 0.0;
            if (xQueueReceive(xQueueDistance, &distance, 0))
            {
                gfx_clear_buffer(&disp);
                char buffer[12];
                sprintf(buffer, "Dist: %.2f cm", distance);
                gfx_draw_string(&disp, 0, 0, 1, buffer);
                gfx_draw_line(&disp, 15, 27, cnt,
                    27);
                vTaskDelay(pdMS_TO_TICKS(50));
                if (++cnt == 112)
                    cnt = 15;
                gfx_show(&disp);



            }
        }
       else{
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "Falhou");
            gfx_draw_line(&disp, 15, 27, cnt,
                27);
            vTaskDelay(pdMS_TO_TICKS(50));
            if (++cnt == 112)
                cnt = 15;
            gfx_show(&disp);
       }
}
}



int main()
{
    stdio_init_all();

    xQueueTimeEchoStart = xQueueCreate(32, sizeof(float));
    xQueueTimeEchoEnd = xQueueCreate(32, sizeof(float));
    xQueueDistance = xQueueCreate(32, sizeof(float));
    xSemaphoreTrigger = xSemaphoreCreateBinary();

    printf("RTC Alarm Repeat!\n");
    while (true){
        if (getchar_timeout_us(100) == 'A'){
            while (1){
                xTaskCreate(trigger_task, "Trigger Task", 256, NULL, 1, NULL);
                alarm = add_alarm_in_ms(5000, alarm_callback, NULL, false);
                xTaskCreate(echo_task, "Echo Task", 256, NULL, 1, NULL);
                xTaskCreate(oled_task, "Oled", 4095, NULL, 1, NULL);
                vTaskStartScheduler();
                if (getchar_timeout_us(100) == 'b')
                {
                    break;
                }
        }
    }
}

}
