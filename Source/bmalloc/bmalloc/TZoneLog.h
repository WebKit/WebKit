/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "BExport.h"
#include "BInline.h"
#include "BPlatform.h"

#if BUSE(TZONE)

#include <os/log.h>
#include <stdarg.h>

namespace bmalloc { namespace api {

class TZoneLog {
public:
    enum LogDestination {
        Off,
        Stderr,
#if BUSE(OS_LOG)
        OSLog
#endif
    };

protected:
    TZoneLog()
        : m_logDest(LogDestination::Off)
    { }

public:
    TZoneLog(TZoneLog &other) = delete;
    void operator=(const TZoneLog &) = delete;

    BEXPORT static TZoneLog& singleton();
    BEXPORT void log(const char* format, ...) BATTRIBUTE_PRINTF(2, 3);

private:
    static void ensureSingleton();
    void init();
    static TZoneLog* theTZoneLog;

    BINLINE bool useStdErr() { return m_logDest == LogDestination::Stderr; }
#if BUSE(OS_LOG)
    BINLINE bool useOSLog() { return m_logDest == LogDestination::OSLog; }
    BINLINE os_log_t osLog() const { return m_osLog; }
    void osLogWithLineBuffer(const char* format, va_list) BATTRIBUTE_PRINTF(2, 0);
#endif

    LogDestination m_logDest;

#if BUSE(OS_LOG)
    os_log_t m_osLog;
    static constexpr unsigned s_osLogBufferSize = 121;
    char m_buffer[s_osLogBufferSize];
    unsigned m_bufferCursor { 0 };
#endif
};

} } // namespace bmalloc::api

#define TZONE_LOG_DEBUG(...) TZoneLog::singleton().log(__VA_ARGS__)

#else // not BUSE(TZONE)
#define TZONE_LOG_DEBUG(...)
#endif // BUSE(TZONE)
