#include "main.h"
#include "stm32l4xx_hal.h"
#include <stdint.h>

extern SPI_HandleTypeDef FM_SPI;

static void device_delay_us(int us) {
  for (int i = 0; i < us * 1000; ++i)
    asm volatile ("nop");
}

int fm_read_reg(uint8_t reg, uint8_t *buf, uint8_t len) {
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_RESET);
  reg |= 0x20;
  HAL_SPI_Transmit(&FM_SPI, &reg, 1, 1000);
  HAL_SPI_Receive(&FM_SPI, buf, len, 1000);
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_SET);
  return 0;
}

int fm_write_reg(uint8_t reg, uint8_t *buf, uint8_t len) {
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&FM_SPI, &reg, 1, 1000);
  HAL_SPI_Receive(&FM_SPI, buf, len, 1000);
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_SET);
  return 0;
}

int fm_read_eeprom(uint16_t addr, uint8_t *buf, uint8_t len) {
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_RESET);
  device_delay_us(100);
  uint8_t data[2] = {0x60 | (addr >> 8), addr & 0xFF};
  HAL_SPI_Transmit(&FM_SPI, data, 2, 1000);
  HAL_SPI_Receive(&FM_SPI, buf, len, 1000);
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_SET);
  return 0;
}

int fm_write_eeprom(uint16_t addr, uint8_t *buf, uint8_t len) {
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_RESET);
  device_delay_us(100);
  uint8_t data[2] = {0xCE, 0x55};
  HAL_SPI_Transmit(&FM_SPI, data, 2, 1000);
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_SET);
  device_delay_us(100);
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_RESET);
  data[0] = 0x40 | (addr >> 8);
  data[1] = addr & 0xFF;
  HAL_SPI_Transmit(&FM_SPI, data, 2, 1000);
  HAL_SPI_Transmit(&FM_SPI, buf, len, 1000);
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_SET);
  return 0;
}

int fm_read_fifo(uint8_t *buf, uint8_t len) {
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_RESET);
  uint8_t addr = 0xA0;
  HAL_SPI_Transmit(&FM_SPI, &addr, 1, 1000);
  HAL_SPI_Receive(&FM_SPI, buf, len, 1000);
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_SET);
  return 0;
}

int fm_write_fifo(uint8_t *buf, uint8_t len) {
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_RESET);
  uint8_t addr = 0x80;
  HAL_SPI_Transmit(&FM_SPI, &addr, 1, 1000);
  HAL_SPI_Receive(&FM_SPI, buf, len, 1000);
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_SET);
  return 0;
}
