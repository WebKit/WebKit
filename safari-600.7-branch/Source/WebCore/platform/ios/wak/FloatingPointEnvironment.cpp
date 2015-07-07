/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FloatingPointEnvironment.h"

#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#if PLATFORM(IOS)

namespace WebCore {

FloatingPointEnvironment& FloatingPointEnvironment::shared()
{
    static NeverDestroyed<FloatingPointEnvironment> floatingPointEnvironment;
    return floatingPointEnvironment;
}

FloatingPointEnvironment::FloatingPointEnvironment()
    : m_isInitialized(false)
{
}

void FloatingPointEnvironment::enableDenormalSupport()
{
    RELEASE_ASSERT(isUIThread());
#if defined _ARM_ARCH_7
    fenv_t env; 
    fegetenv(&env); 
    env.__fpscr &= ~0x01000000U;
    fesetenv(&env); 
#endif
    // Supporting denormal mode is already the default on x86, x86_64, and ARM64.
}

void FloatingPointEnvironment::saveMainThreadEnvironment()
{
    RELEASE_ASSERT(!m_isInitialized);
    RELEASE_ASSERT(isUIThread());
    fegetenv(&m_mainThreadEnvironment);
    m_isInitialized = true;
}

void FloatingPointEnvironment::propagateMainThreadEnvironment()
{
    RELEASE_ASSERT(m_isInitialized);
    RELEASE_ASSERT(!isUIThread());
    fesetenv(&m_mainThreadEnvironment);
}

} // namespace WebCore

#endif // PLATFORM(IOS)
