#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define DBG_MSG(format, ...) printf("[DBG]%s: " format "\r\n", __func__, ##__VA_ARGS__)
#define INF_MSG(format, ...) printf("[INF]%s: " format "\r\n", __func__, ##__VA_ARGS__)
#define ERR_MSG(format, ...) printf("[ERR]%s: " format "\r\n", __func__, ##__VA_ARGS__)
