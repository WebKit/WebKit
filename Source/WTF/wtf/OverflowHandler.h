/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/OverflowPolicy.h>

namespace WTF {

class AssertNoOverflow {
public:
    static constexpr OverflowPolicy policy = OverflowPolicy::AssertNoOverflow;

    static NO_RETURN_DUE_TO_ASSERT void overflowed()
    {
        ASSERT_NOT_REACHED();
    }

    void clearOverflow() { }

    static NO_RETURN_DUE_TO_CRASH void crash()
    {
        CRASH();
    }

public:
    constexpr bool hasOverflowed() const { return false; }
};

class CrashOnOverflow {
public:
    static constexpr OverflowPolicy policy = OverflowPolicy::CrashOnOverflow;

    static NO_RETURN_DUE_TO_CRASH void overflowed()
    {
        crash();
    }

    void clearOverflow() { }

    static NO_RETURN_DUE_TO_CRASH void crash()
    {
        CRASH();
    }

public:
    bool hasOverflowed() const { return false; }
};

class RecordOverflow {
protected:
    RecordOverflow()
        : m_overflowed(false)
    {
    }

    void clearOverflow()
    {
        m_overflowed = false;
    }

    static NO_RETURN_DUE_TO_CRASH void crash()
    {
        CRASH();
    }

public:
    static constexpr OverflowPolicy policy = OverflowPolicy::RecordOverflow;

    bool hasOverflowed() const { return m_overflowed; }
    void overflowed() { m_overflowed = true; }

private:
    unsigned char m_overflowed;
};

} // namespace WTF

using WTF::AssertNoOverflow;
using WTF::CrashOnOverflow;
using WTF::RecordOverflow;
