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

#include "config.h"
#include "DisplayTextBox.h"

#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Display {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(TextBox);

TextBox::TextBox(Tree& tree, UnadjustedAbsoluteFloatRect borderBox, Style&& displayStyle, const InlineDisplay::Box& box)
    : Box(tree, borderBox, WTFMove(displayStyle), { TypeFlags::TextBox })
    , m_expansion(box.expansion())
    , m_text(box.text())
{
}

const char* TextBox::boxName() const
{
    return "text box";
}

String TextBox::debugDescription() const
{
    TextStream stream;

    stream << boxName() << " " << absoluteBoxRect() << " (" << this << ")";
    auto textContent = text().originalContent().substring(text().start(), text().length()).toString();
    textContent = makeStringByReplacingAll(textContent, '\\', "\\\\"_s);
    textContent = makeStringByReplacingAll(textContent, '\n', "\\n"_s);
    const size_t maxPrintedLength = 80;
    if (textContent.length() > maxPrintedLength) {
        auto substring = StringView(textContent).left(maxPrintedLength);
        stream << " \"" << substring << "\"…";
    } else
        stream << " \"" << textContent << "\"";
    
    return stream.release();
}

} // namespace Display
} // namespace WebCore

