#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include <math.h>

#define NTC_THERM_PIN 28
#define TEMT_6000_PIN 27
#define LED_PIN3 15
#define LED_PIN2 14
#define LED_PIN1 13
#define ADC_RESOLUTION 4095.0f
#define ADC_CHANNEL_TEMT6000 1
#define ADC_CHANNEL_NTC_THERM 2

TaskHandle_t ntcTaskHandle;
TaskHandle_t ledTaskHandle;
TaskHandle_t lightTaskHandle;

volatile float currentTemperature = 0.0f;
volatile float lightPercentage = 0.0f;

void temt_6000_task(void *pvParameters)
{
    (void)pvParameters;
    adc_init();

    while (1)
    {
        adc_gpio_init(TEMT_6000_PIN);
        adc_select_input(ADC_CHANNEL_TEMT6000); // TEMT6000 sensor

        uint16_t adc_value = adc_read();

        lightPercentage = 100.0f * (adc_value / ADC_RESOLUTION);
        printf("Light Percentage: %.2f%%\n", lightPercentage);

        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1000 milliseconds
    }
}

void component_init(uint led_pin, uint state)
{
    gpio_init(led_pin);
    gpio_set_dir(led_pin, state);
}

void led_on(uint led_pin)
{
    gpio_put(led_pin, 1); // Turn LED on
}

void led_off(uint led_pin)
{
    gpio_put(led_pin, 0); // Turn LED off
}

void ledBlinkTask(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        led_on(LED_PIN1);
        vTaskDelay(pdMS_TO_TICKS(500)); // Wait for 1 second
        led_off(LED_PIN1);
        if (lightPercentage > 60)
        {
            led_on(LED_PIN2);
        }
        else
        {
            led_off(LED_PIN2);
        }

        led_on(LED_PIN3);

        vTaskDelay(pdMS_TO_TICKS(500)); // Wait for 1 second
        led_off(LED_PIN3);
    }
}

void ntcTask(void *pvParameters)
{
    (void)pvParameters;

    adc_init();

    // NTC thermistor parameters
    float R1 = 10000.0f; // Resistance of the fixed resistor (in ohms)
    float B = 3950.0f;   // NTC thermistor constant (Beta value)
    float T0 = 298.15f;  // Reference temperature (25°C in Kelvin)

    while (1)
    {
        adc_gpio_init(NTC_THERM_PIN);
        adc_select_input(ADC_CHANNEL_NTC_THERM); // NTC thermistor

        uint16_t adc_value = adc_read();
        float voltage = (adc_value * 3.3f) / ADC_RESOLUTION; // Convert ADC value to voltage (in volts)

        // Calculate the resistance of the NTC thermistor using the voltage divider equation
        float R2 = (R1 * (3.3f - voltage)) / voltage;

        // Calculate the temperature using the Steinhart-Hart equation
        float T = 1.0f / (1.0f / T0 + log(R2 / R1) / B);

        // Convert the temperature to Celsius and Fahrenheit
        float temp_celsius = T - 273.15f;

        currentTemperature = temp_celsius;
        float temp_fahrenheit = (temp_celsius * 9 / 5) + 32;
        printf("Resistance: %.2f Ohms, Temperature: %.2f°C (%.2f°F)\n", R2, temp_celsius, temp_fahrenheit);
        vTaskDelay(pdMS_TO_TICKS(2000)); // Delay for 2000 milliseconds
    }
}

int main()
{
    stdio_init_all();

    component_init(LED_PIN1, GPIO_OUT);
    component_init(LED_PIN2, GPIO_OUT);
    component_init(LED_PIN3, GPIO_OUT);

    xTaskCreate(ntcTask, "NTC Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &ntcTaskHandle);
    xTaskCreate(ledBlinkTask, "LED Blink Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, &ledTaskHandle);
    xTaskCreate(temt_6000_task, "light Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, &lightTaskHandle);
    vTaskStartScheduler();
    while (1)
    {
    }
}
