/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "Length.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

class TextUnderlineOffset {
public:
    static TextUnderlineOffset createWithAuto()
    {
        return TextUnderlineOffset(Type::Auto);
    }

    static TextUnderlineOffset createWithLength(float length)
    {
        TextUnderlineOffset result(Type::Length);
        result.setLengthValue(length);
        return result;
    }

    bool isAuto() const
    {
        return !isLength();
    }

    bool isLength() const
    {
        return static_cast<bool>(m_length);
    }

    void setLengthValue(float length)
    {
        ASSERT(isLength());
        m_length = length;
    }

    float lengthValue() const
    {
        ASSERT(isLength());
        return *m_length;
    }

    float lengthOr(float defaultValue) const
    {
        return m_length.value_or(defaultValue);
    }

    bool operator==(const TextUnderlineOffset& other) const
    {
        return m_length == other.m_length;
    }

    bool operator!=(const TextUnderlineOffset& other) const
    {
        return !(*this == other);
    }

private:
    enum class Type {
        Auto,
        Length
    };

    TextUnderlineOffset(Type type)
    {
        if (type == Type::Length)
            m_length = 0;
    }

    std::optional<float> m_length;
};

inline TextStream& operator<<(TextStream& ts, const TextUnderlineOffset& offset)
{
    if (offset.isAuto())
        ts << "auto";
    else
        ts << TextStream::FormatNumberRespectingIntegers(offset.lengthValue());
    return ts;
}

} // namespace WebCore
