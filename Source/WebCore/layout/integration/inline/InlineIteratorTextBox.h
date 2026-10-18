/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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

#include <WebCore/InlineIteratorBox.h>
#include <WebCore/RenderText.h>

namespace WebCore {

namespace InlineIterator {

enum class TextRunMode { Painting, Editing };

class TextBox : public Box {
public:
    TextBox(const LayoutIntegration::InlineContent&, size_t boxIndex);

    bool hasHyphen() const;
    StringView originalText() const;

    unsigned start() const;
    unsigned end() const;
    unsigned length() const;

    TextBoxSelectableRange selectableRange() const;

    const FontCascade& LEGACY_NODELETE fontCascade() const;

    TextRun textRun(TextRunMode = TextRunMode::Painting) const;

    const RenderText& renderer() const { return downcast<RenderText>(Box::renderer()); }

    // This returns the next text box generated for the same RenderText/Layout::InlineTextBox.
    TextBoxIterator LEGACY_NODELETE nextTextBox() const;
};

class TextBoxIterator : public LeafBoxIterator {
public:
    TextBoxIterator() = default;
    TextBoxIterator(const Box&);

    TextBoxIterator& operator++() { return traverseNextTextBox(); }

    const TextBox& operator*() const LIFETIME_BOUND { return get(); }
    const TextBox* operator->() const LIFETIME_BOUND { return &get(); }

    // This traverses to the next text box generated for the same RenderText/Layout::InlineTextBox.
    TextBoxIterator& LEGACY_NODELETE traverseNextTextBox();

private:
    BoxIterator& traverseLineRightwardOnLine() = delete;
    BoxIterator& traverseLineLeftwardOnLine() = delete;
    BoxIterator& traverseLineRightwardOnLineIgnoringLineBreak() = delete;
    BoxIterator& traverseLineLeftwardOnLineIgnoringLineBreak() = delete;

    const TextBox& get() const LIFETIME_BOUND { return downcast<TextBox>(m_box); }
};

TextBoxIterator lineLeftmostTextBoxFor(const RenderText&);
TextBoxIterator textBoxFor(const LayoutIntegration::InlineContent&, const InlineDisplay::Box&);
TextBoxIterator textBoxFor(const LayoutIntegration::InlineContent&, size_t boxIndex);
BoxRange<TextBoxIterator> textBoxesFor(const RenderText&);

inline TextBox::TextBox(const LayoutIntegration::InlineContent& inlineContent, size_t boxIndex)
    : Box(inlineContent, boxIndex)
{
}

inline bool TextBox::hasHyphen() const
{
    return displayBox().text().hasHyphen();
}

inline StringView TextBox::originalText() const
{
    return displayBox().text().originalContent();
}

inline unsigned TextBox::start() const
{
    return displayBox().text().start();
}

inline unsigned TextBox::end() const
{
    return displayBox().text().end();
}

inline unsigned TextBox::length() const
{
    return displayBox().text().length();
}

inline TextBoxSelectableRange TextBox::selectableRange() const
{
    auto& box = displayBox();
    auto& textContent = box.text();
    auto extraTrailingLength = [&] -> unsigned {
        if (textContent.hasHyphen())
            return protect(box.style())->hyphenString().length();
        if (downcast<Layout::InlineTextBox>(box.layoutBox()).isCombined()) {
            ASSERT(textContent.renderedContent().length() >= length());
            return textContent.renderedContent().length() - length();
        }
        return 0;
    };
    return {
        start(),
        length(),
        extraTrailingLength(),
        box.isLineBreak(),
        textContent.partiallyVisibleContentLength(),
        (line().isLeftToRightInlineDirection() ? TextDirection::LTR : TextDirection::RTL) != direction()
    };
}

}
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::InlineIterator::TextBox)
static bool isType(const WebCore::InlineIterator::Box& box) { return box.isText(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::InlineIterator::TextBoxIterator)
static bool isType(const WebCore::InlineIterator::BoxIterator& box) { return !box || box->isText(); }
SPECIALIZE_TYPE_TRAITS_END()

