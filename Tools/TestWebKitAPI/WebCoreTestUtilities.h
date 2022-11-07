/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Test.h"
#include <WebCore/Color.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <wtf/text/TextStream.h>

namespace TestWebKitAPI {

// Caller should initialize lastFootprint with memoryFootprint() for the initial call.
::testing::AssertionResult memoryFootprintChangedBy(size_t& lastFootprint, double expectedChange, double error);

class ScopedSetAuxiliaryProcessTypeForTesting {
public:
    explicit ScopedSetAuxiliaryProcessTypeForTesting(WebCore::AuxiliaryProcessType type)
        : m_oldType(WebCore::processType())
    {
        setAuxiliaryProcessTypeForTesting(type);
    }
    ~ScopedSetAuxiliaryProcessTypeForTesting()
    {
        setAuxiliaryProcessTypeForTesting(m_oldType);
    }
private:
    std::optional<WebCore::AuxiliaryProcessType> m_oldType;
};

}

namespace WebCore {

inline std::ostream& operator<<(std::ostream& os, const WebCore::Color& value)
{
    TextStream s { TextStream::LineMode::SingleLine };
    s << value;
    return os << s.release();
}

inline std::ostream& operator<<(std::ostream& os, const WebCore::FloatSize& value)
{
    TextStream s { TextStream::LineMode::SingleLine };
    s << value;
    return os << s.release();
}

inline std::ostream& operator<<(std::ostream& os, const WebCore::FloatRect& value)
{
    TextStream s { TextStream::LineMode::SingleLine };
    s << value;
    return os << s.release();
}

}
