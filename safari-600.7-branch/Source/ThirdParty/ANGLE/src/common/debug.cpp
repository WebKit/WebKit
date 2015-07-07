//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// debug.cpp: Debugging utilities.

#include "common/debug.h"
#include <stdarg.h>
#include <vector>
#include <fstream>
#include <cstdio>

#if defined(ANGLE_ENABLE_PERF)
#include <d3d9.h>
#endif

namespace gl
{
#if defined(ANGLE_ENABLE_PERF)
typedef void (WINAPI *PerfOutputFunction)(D3DCOLOR, LPCWSTR);
#else
typedef void (*PerfOutputFunction)(unsigned int, const wchar_t*);
#endif

static void output(bool traceFileDebugOnly, PerfOutputFunction perfFunc, const char *format, va_list vararg)
{
#if defined(ANGLE_ENABLE_PERF) || defined(ANGLE_ENABLE_TRACE)
    static std::vector<char> asciiMessageBuffer(512);

    // Attempt to just print to the current buffer
    int len = vsnprintf(&asciiMessageBuffer[0], asciiMessageBuffer.size(), format, vararg);
    if (len < 0 || static_cast<size_t>(len) >= asciiMessageBuffer.size())
    {
        // Buffer was not large enough, calculate the required size and resize the buffer
        len = vsnprintf(NULL, 0, format, vararg);
        asciiMessageBuffer.resize(len + 1);

        // Print again
        vsnprintf(&asciiMessageBuffer[0], asciiMessageBuffer.size(), format, vararg);
    }

    // NULL terminate the buffer to be safe
    asciiMessageBuffer[len] = '\0';
#endif

#if defined(ANGLE_ENABLE_PERF)
    if (perfActive())
    {
        // The perf function only accepts wide strings, widen the ascii message
        static std::wstring wideMessage;
        if (wideMessage.capacity() < asciiMessageBuffer.size())
        {
            wideMessage.reserve(asciiMessageBuffer.size());
        }

        wideMessage.assign(asciiMessageBuffer.begin(), asciiMessageBuffer.begin() + len);

        perfFunc(0, wideMessage.c_str());
    }
#endif // ANGLE_ENABLE_PERF

#if defined(ANGLE_ENABLE_TRACE)
#if defined(NDEBUG)
    if (traceFileDebugOnly)
    {
        return;
    }
#endif // NDEBUG

    static std::ofstream file(TRACE_OUTPUT_FILE, std::ofstream::app);
    if (file)
    {
        file.write(&asciiMessageBuffer[0], len);
        file.flush();
    }

#endif // ANGLE_ENABLE_TRACE
}

void trace(bool traceFileDebugOnly, const char *format, ...)
{
    va_list vararg;
    va_start(vararg, format);
#if defined(ANGLE_ENABLE_PERF)
    output(traceFileDebugOnly, D3DPERF_SetMarker, format, vararg);
#else
    output(traceFileDebugOnly, NULL, format, vararg);
#endif
    va_end(vararg);
}

bool perfActive()
{
#if defined(ANGLE_ENABLE_PERF)
    static bool active = D3DPERF_GetStatus() != 0;
    return active;
#else
    return false;
#endif
}

ScopedPerfEventHelper::ScopedPerfEventHelper(const char* format, ...)
{
#if defined(ANGLE_ENABLE_PERF)
#if !defined(ANGLE_ENABLE_TRACE)
    if (!perfActive())
    {
        return;
    }
#endif // !ANGLE_ENABLE_TRACE
    va_list vararg;
    va_start(vararg, format);
    output(true, reinterpret_cast<PerfOutputFunction>(D3DPERF_BeginEvent), format, vararg);
    va_end(vararg);
#endif // ANGLE_ENABLE_PERF
}

ScopedPerfEventHelper::~ScopedPerfEventHelper()
{
#if defined(ANGLE_ENABLE_PERF)
    if (perfActive())
    {
        D3DPERF_EndEvent();
    }
#endif
}
}
