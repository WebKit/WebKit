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
#include "InlineElementBox.h"
#include "InlineTextBox.h"
#include "RenderText.h"
#include "SimpleLineLayoutResolver.h"
#include <wtf/HashMap.h>
#include <wtf/IteratorRange.h>
#include <wtf/Variant.h>
#include <wtf/text/StringView.h>

namespace WebCore {

class RenderLineBreak;
class RenderText;

namespace LineLayoutTraversal {

class TextBoxContext;
class TextBoxIterator;

struct EndIterator { };

template<class Iterator>
class Box {
public:
    FloatRect rect() const;
    FloatRect logicalRect() const;

    bool isLeftToRightDirection() const;
    bool dirOverride() const;
    bool isLineBreak() const;

protected:
    Box() = default;
    Box(const Box&) = default;
    Box(Box&&) = default;
    ~Box() = default;
    Box& operator=(const Box&) = default;
    Box& operator=(Box&&) = default;

    const Iterator& iterator() const;
};

template<class Iterator>
class TextBox : public Box<Iterator> {
public:
    bool hasHyphen() const;
    StringView text() const;

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

    using Box<Iterator>::iterator;
};

class TextBoxIterator : private TextBox<TextBoxIterator> {
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

    using BoxType = TextBox<TextBoxIterator>;

private:
    friend class Box<TextBoxIterator>;
    friend class TextBox<TextBoxIterator>;

    struct SimplePath {
        SimpleLineLayout::RunResolver::Iterator iterator;
        SimpleLineLayout::RunResolver::Iterator end;
    };
    struct ComplexPath {
        const InlineTextBox* inlineBox;
        Vector<const InlineTextBox*> sortedInlineTextBoxes;
        size_t sortedInlineTextBoxIndex { 0 };

        const InlineTextBox* nextInlineTextBoxInTextOrder() const;
    };
    Variant<SimplePath, ComplexPath> m_pathVariant;
};

class ElementBoxIterator : private Box<ElementBoxIterator> {
public:
    ElementBoxIterator() : m_pathVariant(ComplexPath { nullptr }) { };

    explicit ElementBoxIterator(const InlineElementBox*);
    ElementBoxIterator(SimpleLineLayout::RunResolver::Iterator, SimpleLineLayout::RunResolver::Iterator end);

    explicit operator bool() const { return !atEnd(); }

    const Box& operator*() const { return *this; }
    const Box* operator->() const { return this; }

    bool atEnd() const;

    using BoxType = Box<ElementBoxIterator>;

private:
    friend class Box<ElementBoxIterator>;

    struct SimplePath {
        SimpleLineLayout::RunResolver::Iterator iterator;
        SimpleLineLayout::RunResolver::Iterator end;
    };
    struct ComplexPath {
        const InlineElementBox* inlineBox;
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
ElementBoxIterator elementBoxFor(const RenderLineBreak&);

// -----------------------------------------------

template<class Iterator> inline FloatRect Box<Iterator>::rect() const
{
    auto simple = [](const typename Iterator::SimplePath& path) {
        return (*path.iterator).rect();
    };

    auto complex = [](const typename Iterator::ComplexPath& path) {
        return path.inlineBox->frameRect();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

template<class Iterator> inline FloatRect Box<Iterator>::logicalRect() const
{
    auto simple = [](const typename Iterator::SimplePath& path) {
        return (*path.iterator).rect();
    };

    auto complex = [](const typename Iterator::ComplexPath& path) {
        return path.inlineBox->logicalFrameRect();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

template<class Iterator> inline bool Box<Iterator>::isLeftToRightDirection() const
{
    auto simple = [](const typename Iterator::SimplePath&) {
        return true;
    };

    auto complex = [](const typename Iterator::ComplexPath& path) {
        return path.inlineBox->isLeftToRightDirection();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

template<class Iterator> inline bool Box<Iterator>::dirOverride() const
{
    auto simple = [](const typename Iterator::SimplePath&) {
        return false;
    };

    auto complex = [](const typename Iterator::ComplexPath& path) {
        return path.inlineBox->dirOverride();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

template<class Iterator> inline bool Box<Iterator>::isLineBreak() const
{
    auto simple = [](const typename Iterator::SimplePath& path) {
        return (*path.iterator).isLineBreak();
    };

    auto complex = [](const typename Iterator::ComplexPath& path) {
        return path.inlineBox->isLineBreak();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

template<class Iterator> inline const Iterator& Box<Iterator>::iterator() const
{
    return static_cast<const Iterator&>(*this);
}

template<class Iterator> inline bool TextBox<Iterator>::hasHyphen() const
{
    auto simple = [](const typename Iterator::SimplePath& path) {
        return (*path.iterator).hasHyphen();
    };

    auto complex = [](const typename Iterator::ComplexPath& path) {
        return path.inlineBox->hasHyphen();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

template<class Iterator> inline StringView TextBox<Iterator>::text() const
{
    auto simple = [](const typename Iterator::SimplePath& path) {
        return (*path.iterator).text();
    };

    auto complex = [](const typename Iterator::ComplexPath& path) {
        return StringView(path.inlineBox->renderer().text()).substring(path.inlineBox->start(), path.inlineBox->len());
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

template<class Iterator> inline unsigned TextBox<Iterator>::localStartOffset() const
{
    auto simple = [](const typename Iterator::SimplePath& path) {
        return (*path.iterator).localStart();
    };

    auto complex = [](const typename Iterator::ComplexPath& path) {
        return path.inlineBox->start();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

template<class Iterator> inline unsigned TextBox<Iterator>::localEndOffset() const
{
    auto simple = [](const typename Iterator::SimplePath& path) {
        return (*path.iterator).localEnd();
    };

    auto complex = [](const typename Iterator::ComplexPath& path) {
        return path.inlineBox->end();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

template<class Iterator> inline unsigned TextBox<Iterator>::length() const
{
    auto simple = [](const typename Iterator::SimplePath& path) {
        return (*path.iterator).end() - (*path.iterator).start();
    };

    auto complex = [](const typename Iterator::ComplexPath& path) {
        return path.inlineBox->len();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

template<class Iterator> inline bool TextBox<Iterator>::isLastOnLine() const
{
    auto simple = [](const typename Iterator::SimplePath& path) {
        auto next = path.iterator;
        ++next;
        return next == path.end || (*path.iterator).lineIndex() != (*next).lineIndex();
    };

    auto complex = [](const typename Iterator::ComplexPath& path) {
        auto* next = path.nextInlineTextBoxInTextOrder();
        return !next || &path.inlineBox->root() != &next->root();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

template<class Iterator> inline bool TextBox<Iterator>::isLast() const
{
    auto simple = [](const typename Iterator::SimplePath& path) {
        auto next = path.iterator;
        ++next;
        return next == path.end;
    };

    auto complex = [](const typename Iterator::ComplexPath& path) {
        return !path.nextInlineTextBoxInTextOrder();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

}
}
