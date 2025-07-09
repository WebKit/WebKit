/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
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

#include "config.h"
#include <wtf/CPUTime.h>

#if OS(DARWIN)
#include <mach/thread_info.h>
#endif
#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <wtf/Threading.h>

namespace WTF {

static Seconds timevalToSeconds(const struct timeval& value)
{
    return Seconds(value.tv_sec) + Seconds::fromMicroseconds(value.tv_usec);
}

std::optional<CPUTime> CPUTime::get()
{
    struct rusage resource { };
    int ret = getrusage(RUSAGE_SELF, &resource);
    ASSERT_UNUSED(ret, !ret);
    return CPUTime { MonotonicTime::now(), timevalToSeconds(resource.ru_utime), timevalToSeconds(resource.ru_stime) };
}

Seconds CPUTime::forCurrentThread()
{
    struct timespec ts { };
    int ret = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    RELEASE_ASSERT(!ret);
    return Seconds(ts.tv_sec) + Seconds::fromNanoseconds(ts.tv_nsec);
}

Seconds CPUTime::forCurrentThreadSlow()
{
#if OS(DARWIN)
    mach_port_t thread = Thread::currentSingleton().machThread();
    thread_basic_info_data_t threadInfo;
    mach_msg_type_number_t threadInfoCount = THREAD_BASIC_INFO_COUNT;
    kern_return_t kr = thread_info(thread, THREAD_BASIC_INFO, (thread_info_t)&threadInfo, &threadInfoCount);
    RELEASE_ASSERT(kr == KERN_SUCCESS);
    return Seconds(threadInfo.user_time.seconds + threadInfo.system_time.seconds) + Seconds::fromMicroseconds(threadInfo.user_time.microseconds + threadInfo.system_time.microseconds);
#else
    return forCurrentThread();
#endif
}

}
