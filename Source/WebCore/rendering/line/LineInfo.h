/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LineInfo_h
#define LineInfo_h

#include "LineWidth.h"

namespace WebCore {

class RenderBlock;

class LineInfo {
public:
    LineInfo()
        : m_isFirstLine(true)
        , m_isLastLine(false)
        , m_isEmpty(true)
        , m_previousLineBrokeCleanly(true)
        , m_floatPaginationStrut(0)
        , m_runsFromLeadingWhitespace(0)
    { }

    bool isFirstLine() const { return m_isFirstLine; }
    bool isLastLine() const { return m_isLastLine; }
    bool isEmpty() const { return m_isEmpty; }
    bool previousLineBrokeCleanly() const { return m_previousLineBrokeCleanly; }
    LayoutUnit floatPaginationStrut() const { return m_floatPaginationStrut; }
    unsigned runsFromLeadingWhitespace() const { return m_runsFromLeadingWhitespace; }
    void resetRunsFromLeadingWhitespace() { m_runsFromLeadingWhitespace = 0; }
    void incrementRunsFromLeadingWhitespace() { m_runsFromLeadingWhitespace++; }

    void setFirstLine(bool firstLine) { m_isFirstLine = firstLine; }
    void setLastLine(bool lastLine) { m_isLastLine = lastLine; }
    void setEmpty(bool empty, RenderBlock* block = 0, LineWidth* lineWidth = 0);
    void setPreviousLineBrokeCleanly(bool previousLineBrokeCleanly) { m_previousLineBrokeCleanly = previousLineBrokeCleanly; }
    void setFloatPaginationStrut(LayoutUnit strut) { m_floatPaginationStrut = strut; }

private:
    bool m_isFirstLine;
    bool m_isLastLine;
    bool m_isEmpty;
    bool m_previousLineBrokeCleanly;
    LayoutUnit m_floatPaginationStrut;
    unsigned m_runsFromLeadingWhitespace;
};

}

#endif // LineInfo_h
