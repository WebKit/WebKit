/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "CPUTime.h"

#import "MachSendRight.h"
#import <mach/mach.h>
#import <mach/mach_time.h>
#import <mach/task.h>
#import <mach/task_info.h>
#import <mach/thread_info.h>
#import <sys/time.h>

namespace WTF {

static const int64_t microsecondsPerSecond = 1000000;

static int64_t timeValueToMicroseconds(const time_value_t& value)
{
    int64_t result = value.seconds;
    result *= microsecondsPerSecond;
    result += value.microseconds;
    return result;
}

Optional<CPUTime> CPUTime::get()
{
    // Account for current threads.
    task_thread_times_info threadInfoData;
    mach_msg_type_number_t threadInfoCount = TASK_THREAD_TIMES_INFO_COUNT;
    kern_return_t result = task_info(mach_task_self(), TASK_THREAD_TIMES_INFO, reinterpret_cast<task_info_t>(&threadInfoData), &threadInfoCount);
    if (result != KERN_SUCCESS)
        return WTF::nullopt;

    int64_t userTime = timeValueToMicroseconds(threadInfoData.user_time);
    int64_t systemTime = timeValueToMicroseconds(threadInfoData.system_time);

    // Account for termined threads.
    task_basic_info taskInfoData;
    mach_msg_type_number_t taskInfoCount = TASK_BASIC_INFO_COUNT;
    result = task_info(mach_task_self(), TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&taskInfoData), &taskInfoCount);
    if (result != KERN_SUCCESS)
        return WTF::nullopt;

    userTime += timeValueToMicroseconds(taskInfoData.user_time);
    systemTime += timeValueToMicroseconds(taskInfoData.system_time);

    return CPUTime { MonotonicTime::now(), Seconds::fromMicroseconds(userTime), Seconds::fromMicroseconds(systemTime) };
}

Seconds CPUTime::forCurrentThread()
{
    mach_msg_type_number_t infoCount = THREAD_BASIC_INFO_COUNT;
    thread_basic_info_data_t info;

    auto threadPort = MachSendRight::adopt(mach_thread_self());
    auto ret = thread_info(threadPort.sendRight(), THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&info), &infoCount);
    RELEASE_ASSERT(ret == KERN_SUCCESS);

    return Seconds(info.user_time.seconds + info.system_time.seconds) + Seconds::fromMicroseconds(info.user_time.microseconds + info.system_time.microseconds);
}

}
