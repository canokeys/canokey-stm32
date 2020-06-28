// SPDX-License-Identifier: Apache-2.0
#include "main.h"
#include "stm32l4xx_ll_usart.h"
extern UART_HandleTypeDef DBG_UART;

/* Retargeting functions for gcc-arm-embedded */

void _ttywrch(int ch) {
  /* Write one char "ch" to the default console
   * Need implementing with UART here. */

  LL_USART_TransmitData8(DBG_UART.Instance, ch);
  while (!LL_USART_IsActiveFlag_TC(DBG_UART.Instance))
    ;
}

int _write(int fd, char *ptr, int len) {
  /* Write "len" of char from "ptr" to file id "fd"
   * Return number of char written.
   * Need implementing with UART here. */
  int i;
  for (i = 0; i < len; ++i) {
    _ttywrch(ptr[i]);
  }
  return len;
}

int _read(int fd, char *ptr, int len) {
  /* Read "len" of char to "ptr" from file id "fd"
   * Return number of char read.
   * Need implementing with UART here. */
  return len;
}