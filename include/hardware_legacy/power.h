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

#ifndef _HARDWARE_POWER_H
#define _HARDWARE_POWER_H

#include <stdint.h>

#if __cplusplus
extern "C" {
#endif

enum {
    PARTIAL_WAKE_LOCK = 1,  // the cpu stays on, but the screen is off
    FULL_WAKE_LOCK = 2      // the screen is also on
};

// while you have a lock held, the device will stay on at least at the
// level you request.
int acquire_wake_lock(int lock, const char* id);
int release_wake_lock(const char* id);

// true if you want the screen on, false if you want it off
int set_screen_state(int on);

int write_to_active(char str_hex_start_bytes[32]);
int write_to_low_power(char str_hex_start_bytes[32]);

// 1/true to set EBI-1 to Deep Power Down
// 0/false to set EBI-1 to Active
int set_ebi1_to_dpd(int on);

// 1/true to set EBI-1 to Self Refresh
// 0/false to set EBI-1 to Active
int set_ebi1_to_sr(int on);

// set how long to stay awake after the last user activity in seconds
int set_last_user_activity_timeout(int64_t delay);


#if __cplusplus
} // extern "C"
#endif

#endif // _HARDWARE_POWER_H
