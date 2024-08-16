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

#include "TZoneLog.h"

#if BUSE(TZONE)

#include "BAssert.h"
#include <mutex.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

namespace bmalloc { namespace api {

TZoneLog* TZoneLog::theTZoneLog = nullptr;

void TZoneLog::init()
{
    auto logEnv = getenv("TZONE_LOGGING");

    if (logEnv) {
        if (!strcasecmp(logEnv, "stderr"))
            m_logDest = LogDestination::Stderr;
#if BUSE(OS_LOG)
        else if (!strcasecmp(logEnv, "oslog")) {
            m_logDest = LogDestination::OSLog;
            m_osLog = os_log_create("com.apple.WebKit", "TZone");
        }
#endif
    }
}

extern TZoneLog& TZoneLog::singleton()
{
    if (!theTZoneLog)
        ensureSingleton();
    BASSERT(theTZoneLog);
    return *theTZoneLog;
}

BATTRIBUTE_PRINTF(2, 3)
extern void TZoneLog::log(const char* format, ...)
{
    if (LogDestination::Off)
        return;

    va_list argList;
    va_start(argList, format);
    if (m_logDest == LogDestination::OSLog)
        osLogWithLineBuffer(format, argList);
    else if (m_logDest == LogDestination::Stderr)
        vfprintf(stderr, format, argList);
    va_end(argList);
}

#if BUSE(OS_LOG)
BATTRIBUTE_PRINTF(3, 0)
void TZoneLog::osLogWithLineBuffer(const char* format, va_list list)
{
    if (!format)
        return;

    auto len = strlen(format);
    if (!len)
        return;

    char* nextBufferPtr = m_buffer + m_bufferCursor;
    bool endsWithNewline = format[strlen(format) - 1] == '\n';

    auto newCursor = vsnprintf(nextBufferPtr, s_osLogBufferSize - m_bufferCursor, format, list) + m_bufferCursor;

    if (endsWithNewline || newCursor >= s_osLogBufferSize) {
        // Dump current buffer
        os_log_debug(osLog(), "%s\n", m_buffer);
        m_bufferCursor = 0;
        return;
    }

    m_bufferCursor = newCursor;
}
#endif

void TZoneLog::ensureSingleton()
{
    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [] {
            theTZoneLog = new TZoneLog();
            theTZoneLog->init();
        }
    );
};

} } // namespace bmalloc::api

#endif // BUSE(TZONE)

