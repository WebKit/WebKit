/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGPU)

#include "WHLSLNameSpace.h"

namespace WebCore {

namespace WHLSL {

struct Token;

class CodeLocation {
public:
    CodeLocation()
        : m_startOffset(0x7FFFFFFF)
        , m_endOffset(0x7FFFFFFF)
        , m_nameSpace(static_cast<unsigned>(AST::NameSpace::StandardLibrary))
    {
    }

    CodeLocation(unsigned startOffset, unsigned endOffset, AST::NameSpace nameSpace)
        : m_startOffset(startOffset)
        , m_endOffset(endOffset)
        , m_nameSpace(static_cast<unsigned>(nameSpace))
    {
    }

    CodeLocation(const Token&);

    CodeLocation(const CodeLocation& location1, const CodeLocation& location2)
        : m_startOffset(location1.startOffset())
        , m_endOffset(location2.endOffset())
        , m_nameSpace(static_cast<unsigned>(location1.nameSpace()))
    {
        ASSERT(location1.nameSpace() == location2.nameSpace());
    }

    unsigned startOffset() const
    {
        return m_startOffset;
    }

    unsigned endOffset() const
    {
        return m_endOffset;
    }

    AST::NameSpace nameSpace() const
    {
        return static_cast<AST::NameSpace>(m_nameSpace);
    }

    bool operator==(const CodeLocation& other) const 
    {
        return m_startOffset == other.m_startOffset
            && m_endOffset == other.m_endOffset
            && m_nameSpace == other.m_nameSpace;
    }

    bool operator!=(const CodeLocation& other) const { return !(*this == other); }

    explicit operator bool() const { return *this != CodeLocation(); }

private:
    unsigned m_startOffset : 31;
    unsigned m_endOffset : 31;
    unsigned m_nameSpace : 2;
};

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
