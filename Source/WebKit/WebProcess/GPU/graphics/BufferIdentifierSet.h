/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include <WebCore/ProcessQualified.h>
#include <WebCore/RenderingResourceIdentifier.h>

namespace WebKit {

struct BufferIdentifierSet {
    std::optional<WebCore::RenderingResourceIdentifier> front;
    std::optional<WebCore::RenderingResourceIdentifier> back;
    std::optional<WebCore::RenderingResourceIdentifier> secondaryBack;
};

inline TextStream& operator<<(TextStream& ts, const BufferIdentifierSet& set)
{
    auto dumpBuffer = [&](const char* name, const std::optional<WebCore::RenderingResourceIdentifier>& bufferInfo) {
        ts.startGroup();
        ts << name << " ";
        if (bufferInfo)
            ts << *bufferInfo;
        else
            ts << "none";
        ts.endGroup();
    };
    dumpBuffer("front buffer", set.front);
    dumpBuffer("back buffer", set.back);
    dumpBuffer("secondaryBack buffer", set.secondaryBack);

    return ts;
}

enum class BufferInSetType : uint8_t {
    Front = 1 << 0,
    Back = 1 << 1,
    SecondaryBack = 1 << 2,
};

inline TextStream& operator<<(TextStream& ts, BufferInSetType bufferType)
{
    if (bufferType == BufferInSetType::Front)
        ts << "Front";
    else if (bufferType == BufferInSetType::Back)
        ts << "Back";
    else
        ts << "SecondaryBack";

    return ts;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
