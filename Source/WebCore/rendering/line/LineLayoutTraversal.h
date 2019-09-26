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

#include "FloatRect.h"
#include "SimpleLineLayoutResolver.h"
#include <wtf/HashMap.h>
#include <wtf/IteratorRange.h>
#include <wtf/Variant.h>
#include <wtf/text/StringView.h>

namespace WebCore {

class InlineTextBox;
class RenderText;

namespace LineLayoutTraversal {

class TextBoxContext;
class TextBoxIterator;

struct EndIterator { };

class TextBox {
public:
    FloatRect rect() const;
    FloatRect logicalRect() const;

    bool hasHyphen() const;
    bool isLeftToRightDirection() const;
    bool dirOverride() const;

    StringView text() const;
    bool isLineBreak() const;

    // These offsets are relative to the text renderer (not flow).
    unsigned localStartOffset() const;
    unsigned localEndOffset() const;
    unsigned length() const;

    bool isLastOnLine() const;
    bool isLast() const;

protected:
    TextBox() = default;
    TextBox(const TextBox&) = default;
    TextBox(TextBox&&) = default;
    ~TextBox() = default;
    TextBox& operator=(const TextBox&) = default;
    TextBox& operator=(TextBox&&) = default;

private:
    const TextBoxIterator& iterator() const;
};

class TextBoxIterator : private TextBox {
public:
    TextBoxIterator() : m_pathVariant(ComplexPath { nullptr, { } }) { };
    explicit TextBoxIterator(const InlineTextBox*);
    TextBoxIterator(Vector<const InlineTextBox*>&& sorted, size_t index);
    
    TextBoxIterator(SimpleLineLayout::RunResolver::Iterator, SimpleLineLayout::RunResolver::Iterator end);

    TextBoxIterator& operator++() { return traverseNextInVisualOrder(); }
    TextBoxIterator& traverseNextInVisualOrder();
    TextBoxIterator& traverseNextInTextOrder();

    explicit operator bool() const { return !atEnd(); }

    bool operator==(const TextBoxIterator&) const;
    bool operator!=(const TextBoxIterator& other) const { return !(*this == other); }

    bool operator==(EndIterator) const { return atEnd(); }
    bool operator!=(EndIterator) const { return !atEnd(); }

    const TextBox& operator*() const { return *this; }
    const TextBox* operator->() const { return this; }

    bool atEnd() const;

private:
    friend class TextBox;

    struct SimplePath {
        SimpleLineLayout::RunResolver::Iterator iterator;
        SimpleLineLayout::RunResolver::Iterator end;
    };
    struct ComplexPath {
        const InlineTextBox* inlineTextBox;
        Vector<const InlineTextBox*> sortedInlineTextBoxes;
        size_t sortedInlineTextBoxIndex { 0 };

        const InlineTextBox* nextInlineTextBoxInTextOrder() const;
    };
    Variant<SimplePath, ComplexPath> m_pathVariant;
};

class TextBoxRange {
public:
    TextBoxRange(TextBoxIterator begin)
        : m_begin(begin)
    {
    }

    TextBoxIterator begin() const { return m_begin; }
    EndIterator end() const { return { }; }

private:
    TextBoxIterator m_begin;
};

TextBoxIterator firstTextBoxFor(const RenderText&);
TextBoxIterator firstTextBoxInTextOrderFor(const RenderText&);
TextBoxRange textBoxesFor(const RenderText&);

inline bool hasTextBoxes(const RenderText& text) { return !firstTextBoxFor(text).atEnd(); }

}
}
