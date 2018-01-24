/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
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

#pragma once

#include "RenderStyleConstants.h"
#include <wtf/text/AtomicString.h>

namespace WebCore {
    
class LineClampValue {
public:
    LineClampValue()
        : m_type(LineClampLineCount)
        , m_value(-1)
    {
    }
    
    LineClampValue(int value, ELineClampType type)
        : m_type(type)
        , m_value(value)
    {
    }
    
    int value() const { return m_value; }
    
    bool isPercentage() const { return m_type == LineClampPercentage; }

    bool isNone() const { return m_value == -1; }

    bool operator==(const LineClampValue& o) const
    {
        return value() == o.value() && isPercentage() == o.isPercentage();
    }
    
    bool operator!=(const LineClampValue& o) const
    {
        return !(*this == o);
    }
    
private:
    ELineClampType m_type;
    int m_value;
};

class LinesClampValue {
public:
    LinesClampValue()
    { }

    LinesClampValue(const LineClampValue& start, const LineClampValue& end, const AtomicString& center)
        : m_start(start)
        , m_end(end)
        , m_center(center)
    { }
    
    bool isNone() const { return m_start.isNone(); }

    bool operator==(const LinesClampValue& o) const
    {
        return m_start == o.start() && m_end == o.end() && m_center == o.center();
    }
    
    bool operator!=(const LinesClampValue& o) const
    {
        return !(*this == o);
    }
    
    LineClampValue start() const { return m_start; }
    LineClampValue end() const { return m_end; }
    AtomicString center() const { return m_center; }

private:
    LineClampValue m_start;
    LineClampValue m_end;
    AtomicString m_center;
};

} // namespace WebCore
