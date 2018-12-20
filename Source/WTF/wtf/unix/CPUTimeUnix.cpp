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

#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>

namespace WTF {

static Seconds timevalToSeconds(const struct timeval& value)
{
    return Seconds(value.tv_sec) + Seconds::fromMicroseconds(value.tv_usec);
}

Optional<CPUTime> CPUTime::get()
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

}
