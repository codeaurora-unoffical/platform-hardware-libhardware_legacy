/*
 * Copyright (C) 2018 The Android Open Source Project
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

#define LOG_TAG "power"
#define ATRACE_TAG ATRACE_TAG_POWER

#include <android-base/logging.h>
#include <android/system/suspend/1.0/ISystemSuspend.h>
#include <hardware_legacy/power.h>
#include <utils/Trace.h>

#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

using android::sp;
using android::system::suspend::V1_0::ISystemSuspend;
using android::system::suspend::V1_0::IWakeLock;
using android::system::suspend::V1_0::WakeLockType;

static std::mutex gLock;
static std::unordered_map<std::string, sp<IWakeLock>> gWakeLockMap;

static sp<ISystemSuspend> getSystemSuspendServiceOnce() {
    static std::once_flag initFlag;
    static sp<ISystemSuspend> suspendService = nullptr;
    std::call_once(initFlag, []() {
        // It's possible for the calling process to not have permissions to
        // ISystemSuspend. getService will then return nullptr.
        suspendService = ISystemSuspend::getService();
    });
    return suspendService;
}

int acquire_wake_lock(int, const char* id) {
    ATRACE_CALL();
    sp<ISystemSuspend> suspendService = getSystemSuspendServiceOnce();
    if (!suspendService) {
        return -1;
    }

    std::lock_guard<std::mutex> l{gLock};
    if (!gWakeLockMap[id]) {
        gWakeLockMap[id] = suspendService->acquireWakeLock(WakeLockType::PARTIAL, id);
    }
    return 0;
}

int release_wake_lock(const char* id) {
    ATRACE_CALL();
    std::lock_guard<std::mutex> l{gLock};
    if (gWakeLockMap[id]) {
        gWakeLockMap[id]->release();
        gWakeLockMap[id] = nullptr;
        return 0;
    }
    return -1;
}
