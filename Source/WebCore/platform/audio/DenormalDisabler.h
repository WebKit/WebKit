/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DenormalDisabler_h
#define DenormalDisabler_h

#include <wtf/MathExtras.h>
#include <cinttypes>

namespace WebCore {

// Deal with denormals. They can very seriously impact performance on x86.

// Define HAVE_DENORMAL if we support flushing denormals to zero.
#if OS(WINDOWS) && COMPILER(MSVC)
#define HAVE_DENORMAL
#endif

#if COMPILER(GCC_COMPATIBLE) && defined(__SSE__)
#define HAVE_DENORMAL
#endif

#ifdef HAVE_DENORMAL
class DenormalDisabler final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DenormalDisabler()
            : m_savedCSR(0)
    {
#if OS(WINDOWS) && COMPILER(MSVC)
        // Save the current state, and set mode to flush denormals.
        //
        // http://stackoverflow.com/questions/637175/possible-bug-in-controlfp-s-may-not-restore-control-word-correctly
        _controlfp_s(&m_savedCSR, 0, 0);
        unsigned int unused;
        _controlfp_s(&unused, _DN_FLUSH, _MCW_DN);
#else
        m_savedCSR = getCSR();
        setCSR(m_savedCSR | (isDAZSupported() ? 0x8040 : 0x8000));
#endif
    }

    ~DenormalDisabler()
    {
#if OS(WINDOWS) && COMPILER(MSVC)
        unsigned int unused;
        _controlfp_s(&unused, m_savedCSR, _MCW_DN);
#else
        setCSR(m_savedCSR);
#endif
    }

    // This is a nop if we can flush denormals to zero in hardware.
    static inline float flushDenormalFloatToZero(float f)
    {
#if OS(WINDOWS) && COMPILER(MSVC) && (!_M_IX86_FP)
        // For systems using x87 instead of sse, there's no hardware support
        // to flush denormals automatically. Hence, we need to flush
        // denormals to zero manually.
        return (std::abs(f) < FLT_MIN) ? 0.0f : f;
#else
        return f;
#endif
    }
private:
#if COMPILER(GCC_COMPATIBLE) && defined(__SSE__)
    static inline bool isDAZSupported()
    {
#if CPU(X86_64)
        return true;
#else
        static bool s_isInited = false;
        static bool s_isSupported = false;
        if (s_isInited)
            return s_isSupported;

        struct fxsaveResult {
            uint8_t before[28];
            uint32_t CSRMask;
            uint8_t after[480];
        } __attribute__ ((aligned (16)));

        fxsaveResult registerData;
        memset(&registerData, 0, sizeof(fxsaveResult));
        asm volatile("fxsave %0" : "=m" (registerData));
        s_isSupported = registerData.CSRMask & 0x0040;
        s_isInited = true;
        return s_isSupported;
#endif
    }

    inline int getCSR()
    {
        int result;
        asm volatile("stmxcsr %0" : "=m" (result));
        return result;
    }

    inline void setCSR(int a)
    {
        int temp = a;
        asm volatile("ldmxcsr %0" : : "m" (temp));
    }

#endif

    unsigned int m_savedCSR;
};

#else
// FIXME: add implementations for other architectures and compilers
class DenormalDisabler final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DenormalDisabler() { }

    // Assume the worst case that other architectures and compilers
    // need to flush denormals to zero manually.
    static inline float flushDenormalFloatToZero(float f)
    {
        return (std::abs(f) < FLT_MIN) ? 0.0f : f;
    }
};

#endif

} // WebCore

#undef HAVE_DENORMAL
#endif // DenormalDisabler_h
