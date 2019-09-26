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

#include "config.h"
#include "LineLayoutTraversal.h"

#include "InlineTextBox.h"
#include "RenderText.h"
#include "SimpleLineLayoutResolver.h"

namespace WebCore {
namespace LineLayoutTraversal {

FloatRect TextBox::rect() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).rect();
    };

    auto complex = [](const TextBoxIterator::ComplexPath& path) {
        return path.inlineTextBox->frameRect();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

FloatRect TextBox::logicalRect() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).rect();
    };

    auto complex = [](const TextBoxIterator::ComplexPath& path) {
        return path.inlineTextBox->logicalFrameRect();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

bool TextBox::hasHyphen() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).hasHyphen();
    };

    auto complex = [](const TextBoxIterator::ComplexPath& path) {
        return path.inlineTextBox->hasHyphen();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

bool TextBox::isLeftToRightDirection() const
{
    auto simple = [](const TextBoxIterator::SimplePath&) {
        return true;
    };

    auto complex = [](const TextBoxIterator::ComplexPath& path) {
        return path.inlineTextBox->isLeftToRightDirection();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

bool TextBox::dirOverride() const
{
    auto simple = [](const TextBoxIterator::SimplePath&) {
        return false;
    };

    auto complex = [](const TextBoxIterator::ComplexPath& path) {
        return path.inlineTextBox->dirOverride();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

StringView TextBox::text() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).text();
    };

    auto complex = [](const TextBoxIterator::ComplexPath& path) {
        return StringView(path.inlineTextBox->renderer().text()).substring(path.inlineTextBox->start(), path.inlineTextBox->len());
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

bool TextBox::isLineBreak() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).isLineBreak();
    };

    auto complex = [](const TextBoxIterator::ComplexPath& path) {
        return path.inlineTextBox->isLineBreak();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

unsigned TextBox::localStartOffset() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).localStart();
    };

    auto complex = [](const TextBoxIterator::ComplexPath& path) {
        return path.inlineTextBox->start();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

unsigned TextBox::localEndOffset() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).localEnd();
    };

    auto complex = [](const TextBoxIterator::ComplexPath& path) {
        return path.inlineTextBox->end();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

unsigned TextBox::length() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).end() - (*path.iterator).start();
    };

    auto complex = [](const TextBoxIterator::ComplexPath& path) {
        return path.inlineTextBox->len();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

bool TextBox::isLastOnLine() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        auto next = path.iterator;
        ++next;
        return next == path.end || (*path.iterator).lineIndex() != (*next).lineIndex();
    };

    auto complex = [](const TextBoxIterator::ComplexPath& path) {
        auto* next = path.nextInlineTextBoxInTextOrder();
        return !next || &path.inlineTextBox->root() != &next->root();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

bool TextBox::isLast() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        auto next = path.iterator;
        ++next;
        return next == path.end;
    };

    auto complex = [](const TextBoxIterator::ComplexPath& path) {
        return !path.nextInlineTextBoxInTextOrder();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}


inline const TextBoxIterator& TextBox::iterator() const
{
    return static_cast<const TextBoxIterator&>(*this);
}

TextBoxIterator::TextBoxIterator(const InlineTextBox* inlineTextBox)
    : m_pathVariant(ComplexPath { inlineTextBox, { } })
{
}
TextBoxIterator::TextBoxIterator(Vector<const InlineTextBox*>&& sorted, size_t index)
    : m_pathVariant(ComplexPath { index < sorted.size() ? sorted[index] : nullptr, WTFMove(sorted), index })
{
}

TextBoxIterator::TextBoxIterator(SimpleLineLayout::RunResolver::Iterator iterator, SimpleLineLayout::RunResolver::Iterator end)
    : m_pathVariant(SimplePath { iterator, end })
{
}

TextBoxIterator& TextBoxIterator::traverseNextInVisualOrder()
{
    auto simple = [](SimplePath& path) {
        ++path.iterator;
    };

    auto complex = [](ComplexPath& path) {
        path.inlineTextBox = path.inlineTextBox->nextTextBox();
    };

    WTF::switchOn(m_pathVariant, simple, complex);

    return *this;
}

const InlineTextBox* TextBoxIterator::ComplexPath::nextInlineTextBoxInTextOrder() const
{
    if (!sortedInlineTextBoxes.isEmpty()) {
        if (sortedInlineTextBoxIndex + 1 < sortedInlineTextBoxes.size())
            return sortedInlineTextBoxes[sortedInlineTextBoxIndex + 1];
        return nullptr;
    }
    return inlineTextBox->nextTextBox();
}

TextBoxIterator& TextBoxIterator::traverseNextInTextOrder()
{
    auto simple = [](SimplePath& path) {
        ++path.iterator;
    };

    auto complex = [](ComplexPath& path) {
        path.inlineTextBox = path.nextInlineTextBoxInTextOrder();
        if (!path.sortedInlineTextBoxes.isEmpty())
            ++path.sortedInlineTextBoxIndex;
    };

    WTF::switchOn(m_pathVariant, simple, complex);

    return *this;
}

bool TextBoxIterator::operator==(const TextBoxIterator& other) const
{
    if (m_pathVariant.index() != other.m_pathVariant.index())
        return false;

    auto simple = [&](const SimplePath& path) {
        return path.iterator == WTF::get<SimplePath>(other.m_pathVariant).iterator;
    };

    auto complex = [&](const ComplexPath& path) {
        return path.inlineTextBox == WTF::get<ComplexPath>(other.m_pathVariant).inlineTextBox;
    };

    return WTF::switchOn(m_pathVariant, simple, complex);
}

bool TextBoxIterator::atEnd() const
{
    auto simple = [&](const SimplePath& path) {
        return path.iterator == path.end;
    };

    auto complex = [&](const ComplexPath& path) {
        return !path.inlineTextBox;
    };

    return WTF::switchOn(m_pathVariant, simple, complex);
}

TextBoxIterator firstTextBoxFor(const RenderText& text)
{
    if (auto* simpleLineLayout = text.simpleLineLayout()) {
        auto range = simpleLineLayout->runResolver().rangeForRenderer(text);
        return { range.begin(), range.end() };
    }

    return TextBoxIterator { text.firstTextBox() };
}

TextBoxIterator firstTextBoxInTextOrderFor(const RenderText& text)
{
    if (!text.simpleLineLayout() && text.containsReversedText()) {
        Vector<const InlineTextBox*> sortedTextBoxes;
        for (auto* textBox = text.firstTextBox(); textBox; textBox = textBox->nextTextBox())
            sortedTextBoxes.append(textBox);
        std::sort(sortedTextBoxes.begin(), sortedTextBoxes.end(), InlineTextBox::compareByStart);
        return TextBoxIterator { WTFMove(sortedTextBoxes), 0 };
    }

    return firstTextBoxFor(text);
}

TextBoxRange textBoxesFor(const RenderText& text)
{
    return { firstTextBoxFor(text) };
}

}
}
