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
#include <wtf/IteratorRange.h>
#include <wtf/Variant.h>
#include <wtf/text/StringView.h>

namespace WebCore {

class InlineTextBox;
class RenderText;

namespace LineLayoutInterface {

class TextBoxContext;
class TextBoxIterator;

class TextBox {
public:
    TextBox(const TextBoxIterator& iterator)
        : m_iterator(iterator)
    { }

    FloatRect rect() const;
    FloatRect logicalRect() const;

    bool hasHyphen() const;
    bool isLeftToRightDirection() const;
    bool dirOverride() const;

    StringView text() const;

private:
    const TextBoxIterator& m_iterator;
};

class TextBoxIterator {
public:
    TextBoxIterator(const InlineTextBox*);
    TextBoxIterator(SimpleLineLayout::RunResolver::Iterator);

    TextBoxIterator& operator++() { return traverseNext(); }

    bool operator==(const TextBoxIterator&) const;
    bool operator!=(const TextBoxIterator& other) const { return !(*this == other); }

    TextBox operator*() const { return { *this }; }

private:
    friend class TextBox;

    TextBoxIterator& traverseNext();

    Variant<SimpleLineLayout::RunResolver::Iterator, const InlineTextBox*> m_pathVariant;
};

class TextBoxRange {
public:
    TextBoxRange(const RenderText&);

    TextBoxIterator begin() const { return m_range.begin(); }
    TextBoxIterator end() const { return m_range.end(); }

private:
    friend class TextBoxIterator;

    Optional<SimpleLineLayout::RunResolver> m_simpleLineRunResolver;
    WTF::IteratorRange<TextBoxIterator> m_range;
};

TextBoxRange textBoxes(const RenderText&);

}
}
