/* SPDX-License-Identifier: Apache-2.0 */
/* Board glue between canokey-core's USBD stack and the STM32L4 HAL PCD driver.
 * Required by canokey-core 3.0.3, which replaced TinyUSB with the ST USB
 * Device (USBD) middleware. */
#include "main.h"
#include <device-config.h>
#include <usbd_core.h>

PCD_HandleTypeDef hpcd;

void Error_Handler(void);

static USBD_StatusTypeDef HAL2USBD(HAL_StatusTypeDef hal) {
  switch (hal) {
  case HAL_OK:
    return USBD_OK;
  case HAL_ERROR:
    return USBD_FAIL;
  case HAL_BUSY:
    return USBD_BUSY;
  case HAL_TIMEOUT:
    return USBD_FAIL;
  }
  return USBD_FAIL;
}

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev) {
  hpcd.pData = pdev;
  hpcd.Instance = USB;
  hpcd.Init.dev_endpoints = 8;
  hpcd.Init.speed = PCD_SPEED_FULL;
  hpcd.Init.ep0_mps = USB_MAX_EP0_SIZE;
  hpcd.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd.Init.Sof_enable = DISABLE;
  hpcd.Init.low_power_enable = DISABLE;
  hpcd.Init.lpm_enable = DISABLE;
  hpcd.Init.battery_charging_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd) != HAL_OK) Error_Handler();

  /* The fsdev USB IP has no automatic PMA allocation: every endpoint buffer
   * address must be assigned before the endpoint is opened, otherwise all
   * endpoints default to PMA 0 and corrupt the BTABLE (PMA 0x00..0x3F).
   * Endpoint map must match usb_resources_alloc() in usb.c: EP1 ctap HID,
   * EP2 CCID, EP3 kbd HID; all single-buffered, total 464 bytes of PMA. */
  HAL_PCDEx_PMAConfig(&hpcd, 0x00, PCD_SNG_BUF, 0x40);  /* EP0 OUT 64B   */
  HAL_PCDEx_PMAConfig(&hpcd, 0x80, PCD_SNG_BUF, 0x80);  /* EP0 IN  64B   */
  HAL_PCDEx_PMAConfig(&hpcd, 0x01, PCD_SNG_BUF, 0xC0);  /* EP1 OUT 64B   */
  HAL_PCDEx_PMAConfig(&hpcd, 0x81, PCD_SNG_BUF, 0x100); /* EP1 IN  64B   */
  HAL_PCDEx_PMAConfig(&hpcd, 0x02, PCD_SNG_BUF, 0x140); /* EP2 OUT 64B   */
  HAL_PCDEx_PMAConfig(&hpcd, 0x82, PCD_SNG_BUF, 0x180); /* EP2 IN  64B   */
  HAL_PCDEx_PMAConfig(&hpcd, 0x03, PCD_SNG_BUF, 0x1C0); /* EP3 OUT 8B    */
  HAL_PCDEx_PMAConfig(&hpcd, 0x83, PCD_SNG_BUF, 0x1C8); /* EP3 IN  8B    */

  return USBD_OK;
}

void HAL_PCD_MspInit(PCD_HandleTypeDef *pcdHandle) {
  if (pcdHandle->Instance == USB) {
    /* USB clock enable */
    __HAL_RCC_USB_CLK_ENABLE();
    /* USB interrupt Init */
    HAL_NVIC_SetPriority(USB_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USB_IRQn);
  }
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *pcdHandle) {
  if (pcdHandle->Instance == USB) {
    __HAL_RCC_USB_CLK_DISABLE();
    HAL_NVIC_DisableIRQ(USB_IRQn);
  }
}

void USBD_LL_Init_Done(void) {}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev) {
  UNUSED(pdev);
  return HAL2USBD(HAL_PCD_DeInit(&hpcd));
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev) {
  UNUSED(pdev);
  return HAL2USBD(HAL_PCD_Start(&hpcd));
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev) {
  UNUSED(pdev);
  return HAL2USBD(HAL_PCD_Stop(&hpcd));
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps) {
  UNUSED(pdev);
  return HAL2USBD(HAL_PCD_EP_Open(&hpcd, ep_addr, ep_mps, ep_type));
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
  UNUSED(pdev);
  return HAL2USBD(HAL_PCD_EP_Close(&hpcd, ep_addr));
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
  UNUSED(pdev);
  return HAL2USBD(HAL_PCD_EP_Flush(&hpcd, ep_addr));
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
  UNUSED(pdev);
  return HAL2USBD(HAL_PCD_EP_SetStall(&hpcd, ep_addr));
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
  UNUSED(pdev);
  return HAL2USBD(HAL_PCD_EP_ClrStall(&hpcd, ep_addr));
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
  UNUSED(pdev);
  if ((ep_addr & 0x80) == 0x80) return hpcd.IN_ep[ep_addr & 0x7F].is_stall;
  return hpcd.OUT_ep[ep_addr & 0x7F].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr) {
  UNUSED(pdev);
  return HAL2USBD(HAL_PCD_SetAddress(&hpcd, dev_addr));
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_num, const uint8_t *pbuf, uint16_t size) {
  UNUSED(pdev);
  return HAL2USBD(HAL_PCD_EP_Transmit(&hpcd, ep_num, (uint8_t *)pbuf, size));
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint16_t size) {
  UNUSED(pdev);
  return HAL2USBD(HAL_PCD_EP_Receive(&hpcd, ep_addr, pbuf, size));
}

/* USBD_LL_DevConnected / USBD_LL_DevDisconnected are provided by canokey-core's usbd_core.c */

void USBD_LL_Delay(uint32_t Delay) { HAL_Delay(Delay); }

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr) {
  UNUSED(pdev);
  return HAL_PCD_EP_GetRxCount(&hpcd, ep_addr & 0x7F);
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *h) {
  USBD_LL_SetupStage((USBD_HandleTypeDef *)h->pData, (uint8_t *)h->Setup);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *h, uint8_t epnum) {
  USBD_LL_DataInStage((USBD_HandleTypeDef *)h->pData, epnum, h->IN_ep[epnum].xfer_buff);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *h, uint8_t epnum) {
  USBD_LL_DataOutStage((USBD_HandleTypeDef *)h->pData, epnum, h->OUT_ep[epnum].xfer_buff);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *h) {
  USBD_LL_Reset((USBD_HandleTypeDef *)h->pData);
  USBD_LL_SetSpeed((USBD_HandleTypeDef *)h->pData, USBD_SPEED_FULL);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *h) {
  USBD_LL_Suspend((USBD_HandleTypeDef *)h->pData);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *h) {
  USBD_LL_Resume((USBD_HandleTypeDef *)h->pData);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *h) {
  USBD_LL_DevConnected((USBD_HandleTypeDef *)h->pData);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *h) {
  USBD_LL_DevDisconnected((USBD_HandleTypeDef *)h->pData);
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *h) {
  USBD_LL_SOF((USBD_HandleTypeDef *)h->pData);
}
