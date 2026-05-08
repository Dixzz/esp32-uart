/* UART Echo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"

/**
 * This is an example which echos any data it receives on configured UART back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: configured UART
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below (See Kconfig)
 */

#define ECHO_TEST_TXD 5
#define ECHO_TEST_RXD 2

#define ECHO_UART_PORT_NUM (UART_NUM_2)
#define ECHO_UART_BAUD_RATE (CONFIG_EXAMPLE_UART_BAUD_RATE)
#define ECHO_TASK_STACK_SIZE (CONFIG_EXAMPLE_TASK_STACK_SIZE)

static const char *TAG = "UART TEST";

#define BUF_SIZE (1024)

void init()
{

    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN, // UART_PARITY_DISABLE, UART_PARITY_EVEN
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, -1, -1));
    ESP_ERROR_CHECK(gpio_set_pull_mode(ECHO_TEST_RXD, GPIO_PULLDOWN_ONLY));
}

void good()
{
    uint8_t sync_byte = 0x7F;
    uint8_t response;

    while (1)
    {
        ESP_LOGI(TAG, "Sending...");

        uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)&sync_byte, 1);

        // ESP_ERROR_CHECK(uart_wait_tx_done(ECHO_UART_PORT_NUM, 100));

        int len = uart_read_bytes(ECHO_UART_PORT_NUM, &response, 1, 200 / portTICK_PERIOD_MS);

        if (len == 1 && response == 0x79)
        {
            ESP_LOGI(TAG, "Bootloader ACK received!");
            break; // Exit loop, move to Get command
        }
        else if (len == 1)
        {
            ESP_LOGI(TAG, "Got: 0x%02X", response);
        }

        // Read response
        // uint8_t data[BUF_SIZE];
        // int len = uart_read_bytes(ECHO_UART_PORT_NUM, data, 1, 100 / portTICK_PERIOD_MS);

        // if (len == 1 && data[0] == 0x79)
        // {
        //     ESP_LOGI(TAG, "Bootloader ACK received!");
        //     break;
        // }
        // else
        // if (len >= 1)
        // {
        //     ESP_LOGI(TAG, "response: 0x%02X", data[0]);
        // }
        // else
        // {
        //     ESP_LOGI(TAG, "No response. Check wiring and BOOT0.");
        // }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
void app_main(void)
{

    init();

    // good();
    uint8_t sync_byte = 0x7F;
    uint8_t response;

    // Step 1: Sync with bootloader
    while (1)
    {
        uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)&sync_byte, 1);
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, &response, 1, 200 / portTICK_PERIOD_MS);
        if (len == 1 && response == 0x79)
        {
            ESP_LOGI(TAG, "Bootloader ACK received!");
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Step 2: Flush UART RX buffer COMPLETELY
    ESP_ERROR_CHECK(uart_flush_input(ECHO_UART_PORT_NUM));

    // Step 3: Send Get Version command (0x01)
    uint8_t get_version_cmd[] = {0x01, 0xFE};
    uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)get_version_cmd, 2);

    // Step 4: Read ACK for command
    uint8_t ack;
    int len = uart_read_bytes(ECHO_UART_PORT_NUM, &ack, 1, 1000 / portTICK_PERIOD_MS);
    if (len == 1 && ack == 0x79)
    {
        ESP_LOGI(TAG, "Get version ACK received");

        // Read response: [Protocol Version] [Option1] [Option2] [ACK]
        uint8_t data[4];
        len = uart_read_bytes(ECHO_UART_PORT_NUM, data, 4, 1000 / portTICK_PERIOD_MS);
        if (len == 4)
        {
            ESP_LOGI(TAG, "Protocol version: %d.%d", (data[0] >> 4) & 0x0F, data[0] & 0x0F);
            ESP_LOGI(TAG, "Option bytes: 0x%02X 0x%02X", data[1], data[2]);
            if (data[3] == 0x79)
            {
                ESP_LOGI(TAG, "Get version complete!");
            }
        }
        else
        {
            ESP_LOGI(TAG, "Received %d bytes for version response (expected 4)", len);
        }
    }
    else if (len == 1)
    {
        ESP_LOGI(TAG, "Get version NACK: 0x%02X", ack);
    }

    // Configure a temporary buffer for the incoming data
    /*uint8_t *data = (uint8_t *)malloc(BUF_SIZE);

    int len = uart_read_bytes(ECHO_UART_PORT_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
    if (len)
    {
        // data[len] = '\0';
        ESP_LOGI(TAG, "Recv str: %s", (char *)data);
        uart_flush(ECHO_UART_PORT_NUM);
    }*/
}
