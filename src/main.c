#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include <math.h>

// pins
#define LED_PIN1 13
#define LED_PIN2 14
#define HEAT_LAMP 15
#define MOTOR_PIN1 16
#define MOTOR_PIN2 17
#define IR_SENSOR_PIN 10
#define BUZZER_PIN_L 19
#define BUZZER_PIN_R 20
#define IN_TEMP_PIN 26
#define TEMT_6000_PIN 27
#define OUT_TEMP_PIN 28

// adc configuration
#define ADC_RESOLUTION 4095.0f
#define ADC_CHANNEL_IN_TEMP 0
#define ADC_CHANNEL_TEMT6000 1
#define ADC_CHANNEL_OUT_TEMP 2

TaskHandle_t outTempTaskHandle;
TaskHandle_t inTempTaskHandle;
TaskHandle_t ledTaskHandle;
TaskHandle_t lightTaskHandle;
TaskHandle_t motorTaskHandle;
TaskHandle_t motionTaskHandle;

volatile float currentOutTemperature = 0.0f;
volatile float currentInTemperature = 0.0f;
volatile float lightPercentage = 0.0f;

void temt_6000_task(void *pvParameters)
{
  (void)pvParameters;
  while (1)
  {
    adc_gpio_init(TEMT_6000_PIN);
    adc_select_input(ADC_CHANNEL_TEMT6000); // TEMT6000 sensor

    uint16_t adc_value = adc_read();
    float prevLightPercentage = lightPercentage; // Store previous light percentage

    lightPercentage = 100.0f * (adc_value / ADC_RESOLUTION);
    printf("Light Percentage: %.2f%%\n", lightPercentage);

    if (prevLightPercentage <= 50 && lightPercentage > 50)
    {
      xTaskNotifyGive(motorTaskHandle); // Notify motor task when light percentage increases above 50%
    }
    else if (prevLightPercentage > 40 && lightPercentage <= 40)
    {
      xTaskNotifyGive(motorTaskHandle); // Notify motor task when light percentage decreases below or equals 50%
    }
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
    if (currentOutTemperature < 27 && currentInTemperature <= 30)
    {
      led_on(HEAT_LAMP);
    }
    else
    {
      led_off(HEAT_LAMP);
    }

    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for 1 second
  }
}

void motorTask(void *pvParameters)
{
  (void)pvParameters;

  while (1)
  {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Wait for notification

    if (lightPercentage > 50) // If light percentage is higher than 50%
    {
      gpio_put(MOTOR_PIN1, 1); // Set one motor pin high
      gpio_put(MOTOR_PIN2, 0); // Set the other motor pin low
      printf("Spinning motor clockwise for 1 second.\n");
    }
    else // If light percentage is lower than or equal to 50%
    {
      gpio_put(MOTOR_PIN1, 0); // Set one motor pin low
      gpio_put(MOTOR_PIN2, 1); // Set the other motor pin high
      printf("Spinning motor counterclockwise for 1 second.\n");
    }
    vTaskDelay(pdMS_TO_TICKS(500)); // Spin motor for 1 second
    gpio_put(MOTOR_PIN1, 0);        // Turn off motor
    gpio_put(MOTOR_PIN2, 0);
    printf("Motor stopped.\n");
  }
}

void outTemp(void *pvParameters)
{
  (void)pvParameters;

  // NTC thermistor parameters
  float R1 = 10000.0f; // Resistance of the fixed resistor (in ohms)
  float B = 3950.0f;   // NTC thermistor constant (Beta value)
  float T0 = 298.15f;  // Reference temperature (25°C in Kelvin)

  while (1)
  {
    adc_gpio_init(OUT_TEMP_PIN);
    adc_select_input(ADC_CHANNEL_OUT_TEMP); // NTC thermistor

    uint16_t adc_value = adc_read();
    float voltage = (adc_value * 3.3f) / ADC_RESOLUTION; // Convert ADC value to voltage (in volts)

    // Calculate the resistance of the NTC thermistor using the voltage divider equation
    float R2 = (R1 * (3.3f - voltage)) / voltage;

    // Calculate the temperature using the Steinhart-Hart equation
    float T = 1.0f / (1.0f / T0 + log(R2 / R1) / B);

    // Convert the temperature to Celsius and Fahrenheit
    float temp_celsius = T - 273.15f;

    currentOutTemperature = temp_celsius;
    printf("OUT Temperature: %.2f°C\n", temp_celsius);
    vTaskDelay(pdMS_TO_TICKS(2000)); // Delay for 2000 milliseconds
  }
}
void inTemp(void *pvParameters)
{
  (void)pvParameters;

  // NTC thermistor parameters
  float R1 = 10000.0f; // Resistance of the fixed resistor (in ohms)
  float B = 3950.0f;   // NTC thermistor constant (Beta value)
  float T0 = 298.15f;  // Reference temperature (25°C in Kelvin)

  while (1)
  {
    adc_gpio_init(IN_TEMP_PIN);
    adc_select_input(ADC_CHANNEL_IN_TEMP); // NTC thermistor

    uint16_t adc_value = adc_read();
    float voltage = (adc_value * 3.3f) / ADC_RESOLUTION; // Convert ADC value to voltage (in volts)

    // Calculate the resistance of the NTC thermistor using the voltage divider equation
    float R2 = (R1 * (3.3f - voltage)) / voltage;

    // Calculate the temperature using the Steinhart-Hart equation
    float T = 1.0f / (1.0f / T0 + log(R2 / R1) / B);

    // Convert the temperature to Celsius and Fahrenheit
    float temp_celsius = T - 273.15f;

    currentInTemperature = temp_celsius;
    printf("IN Temperature: %.2f°C\n", temp_celsius);
    vTaskDelay(pdMS_TO_TICKS(2000)); // Delay for 2000 milliseconds
  }
}

void irSensTask(void *pvParameters)
{

  while (1)
  {
    // The police light and siren effect only activates when the IR sensor is triggered
    if (gpio_get(IR_SENSOR_PIN) == 0)
    { // Assuming the IR sensor outputs LOW when triggered
      // Simulate "pii" sound
      printf("motionnn\n");

      gpio_put(LED_PIN1, 1);          // Turn the red part on
      gpio_put(BUZZER_PIN_L, 1);      // Turn the buzzer on
      gpio_put(BUZZER_PIN_R, 1);      // Turn the buzzer on
      vTaskDelay(pdMS_TO_TICKS(150)); // Short delay for "pii"

      gpio_put(LED_PIN1, 0); // Turn the red part off
      gpio_put(BUZZER_PIN_L, 0);
      gpio_put(BUZZER_PIN_R, 0);      // Turn the buzzer off
      vTaskDelay(pdMS_TO_TICKS(100)); // Short pause between "pii" and "poo"

      // Simulate "poo" sound
      gpio_put(LED_PIN2, 1); // Turn the blue part on
      gpio_put(BUZZER_PIN_L, 1);
      gpio_put(BUZZER_PIN_R, 1);      // Turn the buzzer on again
      vTaskDelay(pdMS_TO_TICKS(300)); // Longer delay for "poo"

      gpio_put(LED_PIN2, 0); // Turn the blue part off
      gpio_put(BUZZER_PIN_L, 0);
      gpio_put(BUZZER_PIN_R, 0);
      vTaskDelay(pdMS_TO_TICKS(100)); // Short pause before repeating
    }
    else
    {
      // Ensure both LEDs and buzzer are off if the IR sensor is not triggered
      gpio_put(LED_PIN1, 0);
      gpio_put(LED_PIN2, 0);
      gpio_put(BUZZER_PIN_L, 0);
      gpio_put(BUZZER_PIN_R, 0);
      vTaskDelay(pdMS_TO_TICKS(100)); // Short delay to keep checking the sensor status
    }
  }
}

int main()
{
  stdio_init_all();
  adc_init();

  component_init(LED_PIN1, GPIO_OUT);
  component_init(LED_PIN2, GPIO_OUT);
  component_init(HEAT_LAMP, GPIO_OUT);
  component_init(MOTOR_PIN1, GPIO_OUT);
  component_init(MOTOR_PIN2, GPIO_OUT);
  component_init(BUZZER_PIN_L, GPIO_OUT);
  component_init(BUZZER_PIN_R, GPIO_OUT);
  component_init(IR_SENSOR_PIN, GPIO_IN);

  xTaskCreate(outTemp, "outTemp Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, &outTempTaskHandle);
  xTaskCreate(inTemp, "inTemp Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, &inTempTaskHandle);
  xTaskCreate(ledBlinkTask, "LED Blink Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 4, &ledTaskHandle);
  xTaskCreate(temt_6000_task, "light Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &lightTaskHandle);
  xTaskCreate(motorTask, "Motor Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &motorTaskHandle);
  xTaskCreate(irSensTask, "Motion Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 3, &motionTaskHandle);
  vTaskStartScheduler();
  while (1)
  {
  }
}
