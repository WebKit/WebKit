/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "FloatingState.h"

namespace WebCore {
namespace Layout {

class BlockFormattingContext;

// This class holds block level information shared across child inline formatting contexts. 
class BlockLayoutState {
public:
    struct LineClamp {
        size_t maximumNumberOfLines { 0 };
        size_t numberOfVisibleLines { 0 };
        bool isLineClampRootOverflowHidden { true };
    };
    enum class LeadingTrimSide : uint8_t {
        Start = 1 << 0,
        End   = 1 << 1
    };
    using LeadingTrim = OptionSet<LeadingTrimSide>;
    BlockLayoutState(FloatingState&, std::optional<LineClamp> = { }, LeadingTrim = { }, std::optional<LayoutUnit> intrusiveInitialLetterLogicalBottom = { });

    FloatingState& floatingState() { return m_floatingState; }
    const FloatingState& floatingState() const { return m_floatingState; }

    std::optional<LineClamp> lineClamp() const { return m_lineClamp; }
    LeadingTrim leadingTrim() const { return m_leadingTrim; }

    std::optional<LayoutUnit> intrusiveInitialLetterLogicalBottom() const { return m_intrusiveInitialLetterLogicalBottom; }

private:
    FloatingState& m_floatingState;
    std::optional<LineClamp> m_lineClamp;
    LeadingTrim m_leadingTrim;
    std::optional<LayoutUnit> m_intrusiveInitialLetterLogicalBottom;
};

inline BlockLayoutState::BlockLayoutState(FloatingState& floatingState, std::optional<LineClamp> lineClamp, LeadingTrim leadingTrim, std::optional<LayoutUnit> intrusiveInitialLetterLogicalBottom)
    : m_floatingState(floatingState)
    , m_lineClamp(lineClamp)
    , m_leadingTrim(leadingTrim)
    , m_intrusiveInitialLetterLogicalBottom(intrusiveInitialLetterLogicalBottom)
{
}

}
}
