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
#include "led_bin.h"
// #include "led_bin_off.h"

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

#define CHUNK_SIZE 256

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

void sync_bootloader()
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
void tested_get_command(void)
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
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // Step 2: Flush RX
    // uint8_t flush_buf[64];
    // while (uart_read_bytes(ECHO_UART_PORT_NUM, flush_buf, sizeof(flush_buf), 50 / portTICK_PERIOD_MS) > 0)
    //     ;
    uint8_t flush_buf[64];
    while (uart_read_bytes(ECHO_UART_PORT_NUM, flush_buf, sizeof(flush_buf), 10 / portTICK_PERIOD_MS) > 0)
        ;

    // Step 3: Send Get command immediately (remove uart_wait_tx_done)
    uint8_t get_cmd[] = {0x00, 0xFF};
    uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)get_cmd, 2);

    // Step 4: Read response
    uint8_t ack;
    int len = uart_read_bytes(ECHO_UART_PORT_NUM, &ack, 1, 2000 / portTICK_PERIOD_MS);
    if (len == 1 && ack == 0x79)
    {
        ESP_LOGI(TAG, "Get cmd ACK");
        // Read bootloader info...
        uint8_t num_bytes;
        len = uart_read_bytes(ECHO_UART_PORT_NUM, &num_bytes, 1, 2000 / portTICK_PERIOD_MS);
        if (len == 1)
        {
            ESP_LOGI(TAG, "Bytes to follow: %d", num_bytes);
            uint8_t data[32];
            len = uart_read_bytes(ECHO_UART_PORT_NUM, data, num_bytes, 2000 / portTICK_PERIOD_MS);
            if (len == num_bytes)
            {
                ESP_LOGI(TAG, "Version: 0x%02X", data[0]);
                for (int i = 1; i < num_bytes; i++)
                {
                    ESP_LOGI(TAG, "Command: 0x%02X", data[i]);
                }
                // Read final ACK
                len = uart_read_bytes(ECHO_UART_PORT_NUM, &ack, 1, 500 / portTICK_PERIOD_MS);
                if (len == 1 && ack == 0x79)
                {
                    ESP_LOGI(TAG, "Get command complete!");
                }
            }
        }
    }
    else
    {
        ESP_LOGI(TAG, "Get cmd NACK: 0x%02X", ack);
    }

    // Step 4: Infinite loop
    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
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

bool wait_for_ack(void)
{
    uint8_t ack;
    int len = uart_read_bytes(ECHO_UART_PORT_NUM, &ack, 1, 1000 / portTICK_PERIOD_MS);
    if (len == 1 && ack == 0x79)
    {
        ESP_LOGI(TAG, "  ACK");
        return true;
    }
    else if (len == 1)
        ESP_LOGE(TAG, "  NACK: 0x%02X", ack);
    else
        ESP_LOGE(TAG, "  Timeout");
    return false;
}

void sync_bootloader_v2()
{

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
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{

    init();

    // sync_bootloader();
    sync_bootloader_v2();

    // Step 2: Flush RX
    // uint8_t flush_buf[64];
    // while (uart_read_bytes(ECHO_UART_PORT_NUM, flush_buf, sizeof(flush_buf), 50 / portTICK_PERIOD_MS) > 0)
    //     ;
    uint8_t flush_buf[64];
    while (uart_read_bytes(ECHO_UART_PORT_NUM, flush_buf, sizeof(flush_buf), 10 / portTICK_PERIOD_MS) > 0)
        ;

    // Step 3: Send Get command immediately (remove uart_wait_tx_done)
    uint8_t get_cmd[] = {0x00, 0xFF}; // 0x00 GET COMMAND LIST
    uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)get_cmd, 2);

    // Step 4: Read response
    uint8_t ack;
    int len = uart_read_bytes(ECHO_UART_PORT_NUM, &ack, 1, 2000 / portTICK_PERIOD_MS);
    if (len == 1 && ack == 0x79)
    {
        ESP_LOGI(TAG, "Get cmd ACK");
        // Read bootloader info...
        uint8_t num_bytes;
        len = uart_read_bytes(ECHO_UART_PORT_NUM, &num_bytes, 1, 2000 / portTICK_PERIOD_MS);
        if (len == 1)
        {
            ESP_LOGI(TAG, "Bytes to follow: %d", num_bytes);
            uint8_t data[32];
            len = uart_read_bytes(ECHO_UART_PORT_NUM, data, num_bytes, 2000 / portTICK_PERIOD_MS);
            if (len == num_bytes)
            {
                ESP_LOGI(TAG, "Version: 0x%02X", data[0]);
                for (int i = 1; i < num_bytes; i++)
                {
                    ESP_LOGI(TAG, "Command: 0x%02X", data[i]);
                }
                // Read final ACK
                len = uart_read_bytes(ECHO_UART_PORT_NUM, &ack, 1, 500 / portTICK_PERIOD_MS);
                if (len == 1 && ack == 0x79)
                {
                    ESP_LOGI(TAG, "Get command complete!");
                }
            }
        }
    }
    else
    {
        ESP_LOGI(TAG, "Get cmd NACK: 0x%02X", ack);
    }

    /// MASS Extended Erase command - TESTED
    /*while (uart_read_bytes(ECHO_UART_PORT_NUM, flush_buf, sizeof(flush_buf), 10 / portTICK_PERIOD_MS) > 0)
        ;

    uint8_t ee_cmd[] = {0x44, 0xBB}; // 0x44 ^ 0xBB = 0xFF
    uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)ee_cmd, 2);

    uart_read_bytes(ECHO_UART_PORT_NUM, &ack, 1, 1000 / portTICK_PERIOD_MS);

    if (ack != 0x79)
    {
        ESP_LOGE(TAG, "EE cmd failed: 0x%02X", ack);
        return;
    }
    ESP_LOGI(TAG, "EE cmd ACK");

    // Global mass erase: N=0xFFFF, checksum=0x00
    uint8_t global[] = {0xFF, 0xFF, 0x00}; //0xFFFF = Mass erase, 0xFFFE = Bank1 erase
    uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)global, 3);

    // TESTED - Wait for erase complete (can take seconds)
    len = uart_read_bytes(ECHO_UART_PORT_NUM, &ack, 1, 15000 / portTICK_PERIOD_MS);
    if (len == 1 && ack == 0x79)
        ESP_LOGI(TAG, "Global erase complete!");
    else
        ESP_LOGE(TAG, "Erase timeout/NACK: 0x%02X", ack);*/

    while (uart_read_bytes(ECHO_UART_PORT_NUM, flush_buf, sizeof(flush_buf), 10 / portTICK_PERIOD_MS) > 0)
        ;

    // firmware flash
    for (int addr_offset = 0; addr_offset < bare_led_bin_len; addr_offset += CHUNK_SIZE)
    {
        int chunk_len = bare_led_bin_len - addr_offset;
        if (chunk_len > CHUNK_SIZE)
            chunk_len = CHUNK_SIZE;

        uint32_t addr = 0x08000000 + addr_offset;

        // Step A: Send Write Memory command
        uint8_t wm_cmd[] = {0x31, 0xCE};
        uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)wm_cmd, 2);
        if (!wait_for_ack())
            return; // 0x79

        // Step B: Send address (4 bytes) + checksum
        uint8_t addr_bytes[4];
        addr_bytes[0] = (addr >> 24) & 0xFF;
        addr_bytes[1] = (addr >> 16) & 0xFF;
        addr_bytes[2] = (addr >> 8) & 0xFF;
        addr_bytes[3] = addr & 0xFF;
        uint8_t addr_csum = addr_bytes[0] ^ addr_bytes[1] ^ addr_bytes[2] ^ addr_bytes[3];

        uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)addr_bytes, 4);
        uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)&addr_csum, 1);
        if (!wait_for_ack())
            return; // 0x79

        // Step C: Send N + data + checksum
        uint8_t N = chunk_len - 1;
        uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)&N, 1);
        uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)&bare_led_bin[addr_offset], chunk_len); // direct from esp flash

        uint8_t data_csum = N;
        for (int i = 0; i < chunk_len; i++)
            data_csum ^= bare_led_bin[addr_offset + i];
        uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)&data_csum, 1);
        if (!wait_for_ack())
            return; // 0x79

        ESP_LOGI(TAG, "Wrote %d bytes at 0x%08lX", chunk_len, addr);
    }

    // Step D: Go command — jump to firmware
    uint8_t go_cmd[] = {0x21, 0xDE};
    uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)go_cmd, 2);
    if (!wait_for_ack())
        return; 

    uint8_t go_addr[] = {0x08, 0x00, 0x00, 0x00, 0x08}; // 0x08000000 + checksum
    uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)go_addr, 5);
    if (!wait_for_ack())
        return; 
        
    ESP_LOGI(TAG, "Flashing complete. STM32 should now run LED firmware.");

    // Infinite loop
    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
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
