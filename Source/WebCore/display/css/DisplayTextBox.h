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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include "InlineDisplayBox.h"

namespace WebCore {
namespace Display {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(TextBox);

class TextBox : public Box {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(TextBox);
public:
    TextBox(Tree&, UnadjustedAbsoluteFloatRect borderBox, Style&&, const InlineDisplay::Box&);

    InlineDisplay::Box::Expansion expansion() const { return m_expansion; }
    const InlineDisplay::Box::Text& text() const { return m_text; }

private:
    const char* boxName() const final;
    String debugDescription() const final;
    
    InlineDisplay::Box::Expansion m_expansion;
    InlineDisplay::Box::Text m_text;
};

} // namespace Display
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_DISPLAY_BOX(TextBox, isTextBox())

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
