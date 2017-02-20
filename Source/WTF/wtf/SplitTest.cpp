/*
 * Copyright (C) 2017 Apple Inc. All Rights Reserved.
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
#include "SplitTest.h"

#include <wtf/Assertions.h>

namespace WTF {

SplitTest& SplitTest::singleton()
{
    static NeverDestroyed<SplitTest> splitTest;
    return splitTest;
}

void SplitTest::initializeSingleton(uint64_t lo, uint64_t hi)
{
    // Double initialization with different values would be bad because it would
    // provide different experiment results.
    RELEASE_ASSERT(!m_initialized || (m_lo == lo && m_hi == hi));
    m_lo = lo;
    m_hi = hi;

    m_initialized = true;
}

std::optional<bool> SplitTest::enableBooleanExperiment()
{
    if (!m_initialized)
        return std::optional<bool>();

    uint64_t value = m_lo ^ m_hi;
    value = (value & 0xFFFFFFFFu) ^ (value >> 32);
    value = (value & 0xFFFFu) ^ (value >> 16);
    value = (value & 0xFFu) ^ (value >> 8);
    value = (value & 0xFu) ^ (value >> 4);
    value = (value & 0x3u) ^ (value >> 2);
    value = (value & 0x1u) ^ (value >> 1);

    return !!value;
}

} // namespace WTF
