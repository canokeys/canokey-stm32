// SPDX-License-Identifier: Apache-2.0
#include "main.h"
#include <fs.h>
#include <memzero.h>
#include <stdalign.h>
#include <string.h>

#define CHIP_FLASH_SIZE 0x40000
#define LOOKAHEAD_SIZE 16
#define WRITE_SIZE 8
#define READ_SIZE 1
#define FS_BASE (&_lfs_begin)
#define FS_BANK FLASH_BANK_1
#define FLASH_ADDR(b, o) (FS_BASE + (b)*FLASH_PAGE_SIZE + (o))
#define FLASH_ADDR2BLOCK(a) (((a) & ~0x8000000u) / FLASH_PAGE_SIZE)

static struct lfs_config config;
static uint8_t read_buffer[LFS_CACHE_SIZE];
static alignas(4) uint8_t prog_buffer[LFS_CACHE_SIZE];
// uint8_t file_buffer[LFS_LFS_CACHE_SIZE];
static alignas(4) uint8_t lookahead_buffer[LOOKAHEAD_SIZE];
extern uint8_t _lfs_begin;

int block_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
  UNUSED(c);
  // DBG_MSG("blk %d @ %p len %u buf %p\r\n", block, (void*)FLASH_ADDR(block, off), size, buffer);
  memcpy(buffer, (const void *)FLASH_ADDR(block, off), size);
  return 0;
}

static int program_space(uint32_t paddr, const void *buffer, lfs_size_t size) {
  int ret = 0;
  uint32_t typ;
  for (lfs_size_t i = 0; size;) {
    // DBG_MSG("%d\n", i);
    // if (size >= 512) {
    //   typ = FLASH_TYPEPROGRAM_FAST;
    // } else if (size >= 256) {
    //   typ = FLASH_TYPEPROGRAM_FAST_AND_LAST;
    // } else {
      typ = FLASH_TYPEPROGRAM_DOUBLEWORD;
    // }
    if (HAL_FLASH_Program(typ, paddr + i, *(const uint64_t *)((uintptr_t)buffer + i)) !=
        HAL_OK) {
      ERR_MSG("Flash prog failed @%#lx\n", paddr + i);
      ret = LFS_ERR_CORRUPT;
      break;
    }
    if (typ == FLASH_TYPEPROGRAM_DOUBLEWORD) {
      i += 8;
      size -= 8;
    } else {
      i += 256;
      size -= 256;
    }
  }

  return ret;
}

int block_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
  UNUSED(c);

  if (size % WRITE_SIZE != 0 || off % WRITE_SIZE != 0) return LFS_ERR_INVAL;

  uint32_t paddr = (uint32_t)FLASH_ADDR(block, off);
  int ret;

  // DBG_MSG("blk %d @ %p len %u buf %p\r\n", block, (void*)paddr, size, buffer);
  // for (size_t i = 0; i < size; i++)
  // {
  //   if(*(uint8_t*)(paddr+i) != 0xFF) {
  //     DBG_MSG("blank check: %p = %x\n", paddr+i, *(uint8_t*)(paddr+i));
  //   }
  // }
  
  HAL_FLASH_Unlock();
  ret = program_space(paddr, buffer, size);
  HAL_FLASH_Lock();

  // Invalidate cache
  // __HAL_FLASH_DATA_CACHE_DISABLE();
  // // __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
  // __HAL_FLASH_DATA_CACHE_RESET();
  // // __HAL_FLASH_INSTRUCTION_CACHE_RESET();
  // // __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
  // __HAL_FLASH_DATA_CACHE_ENABLE();

  // DBG_MSG("verify %d\n", memcmp(buffer, (const void *)FLASH_ADDR(block, off), size));

  return ret;
}

int block_erase(const struct lfs_config *c, lfs_block_t block) {
  UNUSED(c);
  int ret = 0;
  uint32_t PageError;
  FLASH_EraseInitTypeDef EraseInitStruct;
  EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
  EraseInitStruct.Banks = FS_BANK;
  EraseInitStruct.Page = block + FLASH_ADDR2BLOCK((uintptr_t)FS_BASE);
  EraseInitStruct.NbPages = 1;
  // DBG_MSG("block 0x%x\r\n", EraseInitStruct.Page);

  HAL_FLASH_Unlock();

  if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK) {
    ret = LFS_ERR_IO;
    ERR_MSG("HAL_FLASHEx_Erase %#x failed\n", (unsigned int)PageError);
    goto erase_fail;
  }

  // Invalidate cache
  // __HAL_FLASH_DATA_CACHE_DISABLE();
  // // __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
  // __HAL_FLASH_DATA_CACHE_RESET();
  // // __HAL_FLASH_INSTRUCTION_CACHE_RESET();
  // // __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
  // __HAL_FLASH_DATA_CACHE_ENABLE();

erase_fail:
  HAL_FLASH_Lock();
  // DBG_MSG("done\n");

  return ret;
}

int block_sync(const struct lfs_config *c) {
  UNUSED(c);
  return 0;
}

void littlefs_init() {
  if (FLASH_SIZE < CHIP_FLASH_SIZE) {
    ERR_MSG("FLASH_SIZE=0x%x, less than required, may not work\n", FLASH_SIZE);
  }
  memzero(&config, sizeof(config));
  config.read = block_read;
  config.prog = block_prog;
  config.erase = block_erase;
  config.sync = block_sync;
  config.read_size = READ_SIZE;
  config.prog_size = WRITE_SIZE;
  config.block_size = FLASH_PAGE_SIZE;
  config.block_count = CHIP_FLASH_SIZE / FLASH_PAGE_SIZE - FLASH_ADDR2BLOCK((uintptr_t)FS_BASE);
  config.block_cycles = 100000;
  config.cache_size = LFS_CACHE_SIZE;
  config.lookahead_size = LOOKAHEAD_SIZE;
  config.read_buffer = read_buffer;
  config.prog_buffer = prog_buffer;
  config.lookahead_buffer = lookahead_buffer;
  DBG_MSG("Flash base %p, %u blocks (%u bytes)\r\n", FS_BASE, config.block_count, FLASH_PAGE_SIZE);

  int err;
  for (int retry = 0; retry < 3; retry++) {
    err = fs_mount(&config);
    if (!err) return;
  }
  // should happen for the first boot
  DBG_MSG("Formating data area...\r\n");
  fs_format(&config);
  err = fs_mount(&config);
  if (err) {
    ERR_MSG("Failed to mount FS after formating\r\n");
    for (;;)
      ;
  }
}
