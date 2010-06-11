/*
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#include <hardware_legacy/power.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>

#define LOG_TAG "power"
#include <utils/Log.h>
#include <cutils/properties.h>

#include "qemu.h"
#ifdef QEMU_POWER
#include "power_qemu.h"
#endif

enum {
    ACQUIRE_PARTIAL_WAKE_LOCK = 0,
    RELEASE_WAKE_LOCK,
    REQUEST_STATE,
    OUR_FD_COUNT
};

const char * const OLD_PATHS[] = {
    "/sys/android_power/acquire_partial_wake_lock",
    "/sys/android_power/release_wake_lock",
    "/sys/android_power/request_state"
};

const char * const NEW_PATHS[] = {
    "/sys/power/wake_lock",
    "/sys/power/wake_unlock",
    "/sys/power/state"
};

const char * const AUTO_OFF_TIMEOUT_DEV = "/sys/android_power/auto_off_timeout";

//XXX static pthread_once_t g_initialized = THREAD_ONCE_INIT;
static int g_initialized = 0;
static int g_fds[OUR_FD_COUNT];
static int g_error = 1;

static const char *off_state = "mem";
static const char *on_state = "on";

static int64_t systemTime()
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec*1000000000LL + t.tv_nsec;
}

static int
open_file_descriptors(const char * const paths[])
{
    int i;
    for (i=0; i<OUR_FD_COUNT; i++) {
        int fd = open(paths[i], O_RDWR);
        if (fd < 0) {
            fprintf(stderr, "fatal error opening \"%s\"\n", paths[i]);
            g_error = errno;
            return -1;
        }
        g_fds[i] = fd;
    }

    g_error = 0;
    return 0;
}

static inline void
initialize_fds(void)
{
    // XXX: should be this:
    //pthread_once(&g_initialized, open_file_descriptors);
    // XXX: not this:
    if (g_initialized == 0) {
        if(open_file_descriptors(NEW_PATHS) < 0) {
            open_file_descriptors(OLD_PATHS);
            on_state = "wake";
            off_state = "standby";
        }
        g_initialized = 1;
    }
}

int
acquire_wake_lock(int lock, const char* id)
{
    initialize_fds();

//    LOGI("acquire_wake_lock lock=%d id='%s'\n", lock, id);

    if (g_error) return g_error;

    int fd;

    if (lock == PARTIAL_WAKE_LOCK) {
        fd = g_fds[ACQUIRE_PARTIAL_WAKE_LOCK];
    }
    else {
        return EINVAL;
    }

    return write(fd, id, strlen(id));
}

int
release_wake_lock(const char* id)
{
    initialize_fds();

//    LOGI("release_wake_lock id='%s'\n", id);

    if (g_error) return g_error;

    ssize_t len = write(g_fds[RELEASE_WAKE_LOCK], id, strlen(id));
    return len >= 0;
}

int
set_last_user_activity_timeout(int64_t delay)
{
//    LOGI("set_last_user_activity_timeout delay=%d\n", ((int)(delay)));

    int fd = open(AUTO_OFF_TIMEOUT_DEV, O_RDWR);
    if (fd >= 0) {
        char buf[32];
        ssize_t len;
        len = sprintf(buf, "%d", ((int)(delay)));
        len = write(fd, buf, len);
        close(fd);
        return 0;
    } else {
        return errno;
    }
}

int
set_screen_state(int on)
{
    QEMU_FALLBACK(set_screen_state(on));

    LOGI("*** set_screen_state %d", on);

    initialize_fds();

    //LOGI("go_to_sleep eventTime=%lld now=%lld g_error=%s\n", eventTime,
      //      systemTime(), strerror(g_error));

    if (g_error) return g_error;

    char buf[32];
    int len;
    if(on)
        len = sprintf(buf, on_state);
    else
        len = sprintf(buf, off_state);
    len = write(g_fds[REQUEST_STATE], buf, len);
    if(len < 0) {
        LOGE("Failed setting last user activity: g_error=%d\n", g_error);
    }
    return 0;
}

int
write_to_low_power(char str_start_bytes[PROPERTY_VALUE_MAX])
{
    LOGW("Set Unstable Memory to Low Power (SelfRefresh) State\n");

    int fd_low_power = 0;

    if((fd_low_power = open("/sys/devices/system/memory/low_power", O_WRONLY)) < 0) {
        LOGE("Unable to open low_power mem node %d\n", -errno);
        return -1;
    }
    else
        LOGW("fd_low_power opened successfully\n");

    if(write(fd_low_power, str_start_bytes, strlen(str_start_bytes)) < strlen(str_start_bytes))
    {
        LOGE("Failed to put unstable memory in lowpower mode. %d\n", errno);
        close(fd_low_power);
        return -1;
    }
    LOGW("Unstable memory in low power mode. Wrote %s to low_power\n", str_start_bytes);
    close(fd_low_power);
    return 0;
}

int
write_to_active(char str_start_bytes[PROPERTY_VALUE_MAX])
{
    LOGW("Set Unstable Memory to Active State\n");

    int fd_active = 0;

    if((fd_active = open("/sys/devices/system/memory/active", O_WRONLY)) < 0) {
        LOGE("Unable to open active mem node %d\n", -errno);
        return -1;
    }
    else
        LOGW("fd_active opened successfully\n");

    if(write(fd_active, str_start_bytes, strlen(str_start_bytes)) < strlen(str_start_bytes))
    {
        LOGE("Failed to put unstable memory in Active. %d\n", errno);
        close(fd_active);
        return -1;
    }
    LOGW("Unstable memory in Active state. Wrote %s to active\n", str_start_bytes);
    close(fd_active);
    return 0;
}

/************************************
 If 'SR' equals 1
    Set EBI-1 to Self-Refresh.

 If 'SR' equals 0
    Set EBI-1 to Active
************************************/
int
set_ebi1_to_sr(int SR)
{
    LOGW("setEBI1toSR(%d)\n", SR);

    int count = 0;
    int fd_low_power_memory_start_bytes=0;

    char str_low_power_memory_start_bytes[PROPERTY_VALUE_MAX]="0";

    if(property_get("ro.dev.dmm.sr.start_address", str_low_power_memory_start_bytes, "0") <= 0) {
        LOGE("Failed to property_get() lowpower memory start address:%s\n",str_low_power_memory_start_bytes);
        return -1;
    }
    else
        LOGW("str_low_power_memory_start_bytes = %s\n", str_low_power_memory_start_bytes);

    if(SR) {
        // Set EBI-1 from Active to SR

        if(write_to_low_power(str_low_power_memory_start_bytes) < 0) {
            LOGE("Failed to put unstable memory in lowpower mode: -1\n");
            return -1;
        }
        else {
            LOGW("Unstable memory in lowpower mode: Active-to-SR\n");
            return 0;
        }
    }
    else {
        // Set EBI-1 from SR to Active

        if(write_to_active(str_low_power_memory_start_bytes) < 0) {
            LOGE("Failed to put unstable memory back into Active mode: -1\n");
            return -1;
        }
        else {
            LOGW("Unstable memory in Active mode: SR-to-Active\n");
            return 0;
        }
    }
    return 0;
}

/************************************
 If 'DPD' equals 1
    Set EBI-1 to Deep Power Down.
    If this fails
        Set EBI-1 to Self-Refresh.

 If 'DPD' equals 0
    Set EBI-1 to Active
************************************/
int
set_ebi1_to_dpd(int DPD)
{
    LOGW("setEBI1DPD(%d)", DPD);

    int count = 0;
    int fd_Probe=0, fd_Remove=0, fd_State=0;

    static int in_low_power_state;

    char str_movable_start_bytes[PROPERTY_VALUE_MAX], str_block[32]="0";

    char pDirName[128]="/sys/devices/system/memory/memory", fDirName[128], strState[7]="/state";

    if(property_get("ro.dev.dmm.dpd.start_address", str_movable_start_bytes, "0") <= 0) {
        LOGE("Failed to property_get() movable start bytes:%s\n",str_movable_start_bytes);
        return -1;
    }
    else
        LOGI("str_movable_start_bytes = %s\n", str_movable_start_bytes);

     if(property_get("ro.dev.dmm.dpd.block", str_block, "0") <= 0) {
        LOGE("Failed to property_get() block number:%s\n",str_block);
        return -1;
    }
    else
        LOGI("str_block = %s\n", str_block);

    sprintf(fDirName, "%s%s%s", pDirName, str_block, strState);

    LOGI("Directory Location = %s\n", fDirName);

    if(DPD) {
        if((fd_State=open(fDirName, O_RDWR)) < 0) {
            LOGE("Failed to open %s: %d", fDirName, -errno);
            return -1;
        }

        if(write(fd_State, "offline", strlen("offline")) < strlen("offline")) {
            LOGE("Failed Logical hotremove of unstable memory");
            close(fd_State);
            if(write_to_low_power(str_movable_start_bytes) < 0) {
                LOGE("Failed to put unstable memory in lowpower mode: -1");
                return -1;
            }
            else {
                in_low_power_state = 1;
                LOGW("Unstable memory in lowpower mode: in_low_power_state = %d", in_low_power_state);
                return 0;
            }
        }
        else
            LOGW("Succeeded Logical hotremove of unstable memory");

        close(fd_State);

        if((fd_Remove = open("/sys/devices/system/memory/remove", O_WRONLY)) < 0) {
            LOGE("Failed to open remove file %d", -errno);
            return -1;
        }
        if(write(fd_Remove, str_movable_start_bytes, strlen(str_movable_start_bytes)) < strlen(str_movable_start_bytes)) {
            LOGE("Failed Physical hotremove of unstable memory");
            close(fd_Remove);
            return -1;
        }
        else
            LOGW("Succeeded Physical hotremove of unstable memory");

        LOGW("Wrote %s to Remove", str_movable_start_bytes);
        close(fd_Remove);
    }
    else {
        if(in_low_power_state) {
            if(write_to_active(str_movable_start_bytes) < 0) {
                LOGE("Failed to put unstable memory in active mode: -1");
                return -1;
            }
            else {
                in_low_power_state = 0;
                LOGW("Unstable memory in active mode: in_low_power_state = %d", in_low_power_state);
                return 0;
            }
        }
        else {
            if((fd_Probe = open("/sys/devices/system/memory/probe", O_WRONLY)) < 0) {
                LOGE("Failed to open probe file: %d", -errno);
                return -1;
            }

            if(write(fd_Probe, str_movable_start_bytes, strlen(str_movable_start_bytes)) < strlen(str_movable_start_bytes))         {
                LOGE("Failed Physical hotplug of unstable memory");
                close(fd_Probe);
                return -1;
            }
            close(fd_Probe);
            LOGW("Physical Hotplug Succeded. Wrote %s to Probe", str_movable_start_bytes);

            if((fd_State=open(fDirName, O_RDWR)) < 0) {
                LOGE("Failed to open %s: %d", fDirName, -errno);
                return -1;
            }
            if(write(fd_State, "online", strlen("online")) < strlen("online")) {
                LOGE("Failed Logical hotplug of unstable memory: %d", errno);
                close(fd_State);
                return -1;
            }
            close(fd_State);
            LOGW("Logical Hotplug Succeded. Wrote online to state");
        }
    }
    return 0;
}
