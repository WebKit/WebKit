/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef NotImplemented_h
#define NotImplemented_h

#include <wtf/Assertions.h>

#if PLATFORM(GTK)
    #define suppressNotImplementedWarning() getenv("DISABLE_NI_WARNING")
#else
    #define suppressNotImplementedWarning() false
#endif

#if LOG_DISABLED
    #define notImplemented() ((void)0)
#else

namespace WebCore {
WEBCORE_EXPORT WTFLogChannel* notImplementedLoggingChannel();
}

#define notImplemented() do { \
        static bool havePrinted = false; \
        if (!havePrinted && !suppressNotImplementedWarning()) { \
            WTFLogVerbose(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, WebCore::notImplementedLoggingChannel(), "UNIMPLEMENTED: "); \
            havePrinted = true; \
        } \
    } while (0)

#endif // NDEBUG

#endif // NotImplemented_h
