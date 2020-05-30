/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <hardware_legacy/vibrator.h>
#include "qemu.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define LED_DEVICE "/sys/class/leds/vibrator"
#define TIMEOUT_STR_LEN         20

static int write_value(const char *file, const char *value)
{
    int to_write, written, ret, fd;

    fd = TEMP_FAILURE_RETRY(open(file, O_WRONLY));
    if (fd < 0) {
        return -errno;
    }

    to_write = strlen(value) + 1;
    written = TEMP_FAILURE_RETRY(write(fd, value, to_write));
    if (written == -1) {
        ret = -errno;
    } else if (written != to_write) {
        /* even though EAGAIN is an errno value that could be set
           by write() in some cases, none of them apply here.  So, this return
           value can be clearly identified when debugging and suggests the
           caller that it may try to call vibrator_on() again */
        ret = -EAGAIN;
    } else {
        ret = 0;
    }

    errno = 0;
    close(fd);

    return ret;
}

static int write_led_file(const char *file, const char *value)
{
    char file_str[50];

    snprintf(file_str, sizeof(file_str), "%s/%s", LED_DEVICE, file);
    return write_value(file_str, value);
}

static int vibra_led_on(unsigned int timeout_ms)
{
    int ret;
    char value[TIMEOUT_STR_LEN]; /* large enough for millions of years */

    ret = write_led_file("state", "1");
    if (ret)
        return ret;

    snprintf(value, sizeof(value), "%u\n", timeout_ms);
    ret = write_led_file("duration", value);
    if (ret)
        return ret;

    return write_led_file("activate", "1");
}

static int vibra_led_off()
{
    return write_led_file("activate", "0");
}

int vibrator_on(int timeout_ms)
{
    /* constant on, up to maximum allowed time */
    return vibra_led_on(timeout_ms);
}

int vibrator_off()
{
    return vibra_led_off();
}
