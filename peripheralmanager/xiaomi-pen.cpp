/*
 * Copyright (C) 2023 Paranoid Android
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define COMMON_DATA_CMD 0
#define SELECT_TOUCH_ID 3

#define SET_CUR_VALUE 0
#define TOUCH_PEN_MODE 20
#define TOUCH_PEN_VALUE 18
#define TOUCH_MAGIC 'T'
#define TOUCH_DEV_PATH "/dev/xiaomi-touch"
#define TOUCH_ID 0
#define CMD_DATA_BUF_SIZE 128

typedef struct {
	int8_t touch_id = TOUCH_ID;
	uint8_t cmd = SET_CUR_VALUE;
	uint16_t mode;
	uint16_t data_len = CMD_DATA_BUF_SIZE;
	int32_t data_buf[CMD_DATA_BUF_SIZE];
} common_data_t;

#define XIAOMI_IOC_COMMON_DATA _IOW(TOUCH_MAGIC, COMMON_DATA_CMD, common_data_t)
#define XIAOMI_IOC_SELECT_TOUCH_ID _IOW(TOUCH_MAGIC, SELECT_TOUCH_ID, unsigned long)

int main() {
    int fd = open(TOUCH_DEV_PATH, O_RDWR);
    common_data_t data = {
        .mode = TOUCH_PEN_MODE,
        .data_buf = { TOUCH_PEN_VALUE },
    };

    ioctl(fd, XIAOMI_IOC_SELECT_TOUCH_ID, TOUCH_ID);
    ioctl(fd, XIAOMI_IOC_COMMON_DATA, &data);
    close(fd);
}
