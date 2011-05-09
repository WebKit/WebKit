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

namespace WebCore {

// Deal with denormals. They can very seriously impact performance on x86.

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
class DenormalDisabler {
public:
    DenormalDisabler()
    {
        m_savedCSR = getCSR();
        setCSR(m_savedCSR | 0x8040);
    }

    ~DenormalDisabler()
    {
        setCSR(m_savedCSR);
    }

private:
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

    int m_savedCSR;
};

#else
// FIXME: add implementations for other architectures and compilers such as Visual Studio.
class DenormalDisabler {
public:
    DenormalDisabler() { }
};

#endif

} // WebCore

#endif // DenormalDisabler_h
