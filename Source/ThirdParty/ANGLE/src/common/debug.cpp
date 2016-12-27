//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// debug.cpp: Debugging utilities.

#include "common/debug.h"

#include <stdarg.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#include "common/angleutils.h"
#include "common/platform.h"
#include "common/Optional.h"

namespace gl
{

namespace
{

class FormattedString final : angle::NonCopyable
{
  public:
    FormattedString(const char *format, va_list vararg) : mFormat(format)
    {
        va_copy(mVarArg, vararg);
    }

    const char *c_str() { return str().c_str(); }

    const std::string &str()
    {
        if (!mMessage.valid())
        {
            mMessage = FormatString(mFormat, mVarArg);
        }
        return mMessage.value();
    }

    size_t length()
    {
        c_str();
        return mMessage.value().length();
    }

  private:
    const char *mFormat;
    va_list mVarArg;
    Optional<std::string> mMessage;
};
enum DebugTraceOutputType
{
   DebugTraceOutputTypeNone,
   DebugTraceOutputTypeSetMarker,
   DebugTraceOutputTypeBeginEvent
};

DebugAnnotator *g_debugAnnotator = nullptr;

void output(bool traceInDebugOnly, MessageType messageType, DebugTraceOutputType outputType,
            const char *format, va_list vararg)
{
    if (DebugAnnotationsActive())
    {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
        static std::vector<char> buffer(512);
#pragma clang diagnostic pop
        size_t len = FormatStringIntoVector(format, vararg, buffer);
        std::wstring formattedWideMessage(buffer.begin(), buffer.begin() + len);

        ASSERT(g_debugAnnotator != nullptr);
        switch (outputType)
        {
          case DebugTraceOutputTypeNone:
            break;
          case DebugTraceOutputTypeBeginEvent:
            g_debugAnnotator->beginEvent(formattedWideMessage.c_str());
            break;
          case DebugTraceOutputTypeSetMarker:
            g_debugAnnotator->setMarker(formattedWideMessage.c_str());
            break;
        }
    }

    FormattedString formattedMessage(format, vararg);

    if (messageType == MESSAGE_ERR)
    {
        std::cerr << formattedMessage.c_str();
#if !defined(NDEBUG) && defined(_MSC_VER)
        OutputDebugStringA(formattedMessage.c_str());
#endif  // !defined(NDEBUG) && defined(_MSC_VER)
    }

#if defined(ANGLE_ENABLE_DEBUG_TRACE)
#if defined(NDEBUG)
    if (traceInDebugOnly)
    {
        return;
    }
#endif  // NDEBUG
    static std::ofstream file(TRACE_OUTPUT_FILE, std::ofstream::app);
    if (file)
    {
        file.write(formattedMessage.c_str(), formattedMessage.length());
        file.flush();
    }

#if defined(ANGLE_ENABLE_DEBUG_TRACE_TO_DEBUGGER)
    OutputDebugStringA(formattedMessage.c_str());
#endif  // ANGLE_ENABLE_DEBUG_TRACE_TO_DEBUGGER

#endif  // ANGLE_ENABLE_DEBUG_TRACE
}

} // namespace

bool DebugAnnotationsActive()
{
#if defined(ANGLE_ENABLE_DEBUG_ANNOTATIONS)
    return g_debugAnnotator != nullptr && g_debugAnnotator->getStatus();
#else
    return false;
#endif
}

void InitializeDebugAnnotations(DebugAnnotator *debugAnnotator)
{
    UninitializeDebugAnnotations();
    g_debugAnnotator = debugAnnotator;
}

void UninitializeDebugAnnotations()
{
    // Pointer is not managed.
    g_debugAnnotator = nullptr;
}

void trace(bool traceInDebugOnly, MessageType messageType, const char *format, ...)
{
    va_list vararg;
    va_start(vararg, format);
    output(traceInDebugOnly, messageType, DebugTraceOutputTypeSetMarker, format, vararg);
    va_end(vararg);
}

ScopedPerfEventHelper::ScopedPerfEventHelper(const char* format, ...)
{
#if !defined(ANGLE_ENABLE_DEBUG_TRACE)
    if (!DebugAnnotationsActive())
    {
        return;
    }
#endif // !ANGLE_ENABLE_DEBUG_TRACE
    va_list vararg;
    va_start(vararg, format);
    output(true, MESSAGE_EVENT, DebugTraceOutputTypeBeginEvent, format, vararg);
    va_end(vararg);
}

ScopedPerfEventHelper::~ScopedPerfEventHelper()
{
    if (DebugAnnotationsActive())
    {
        g_debugAnnotator->endEvent();
    }
}

}
