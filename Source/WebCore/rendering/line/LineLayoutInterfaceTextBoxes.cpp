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
#include "LineLayoutInterfaceTextBoxes.h"

#include "InlineTextBox.h"
#include "RenderText.h"
#include "SimpleLineLayoutResolver.h"

namespace WebCore {
namespace LineLayoutInterface {

FloatRect TextBox::rect() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).rect();
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return inlineTextBox->frameRect();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

FloatRect TextBox::logicalRect() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).rect();
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return inlineTextBox->logicalFrameRect();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

bool TextBox::hasHyphen() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).hasHyphen();
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return inlineTextBox->hasHyphen();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

bool TextBox::isLeftToRightDirection() const
{
    auto simple = [](const TextBoxIterator::SimplePath&) {
        return true;
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return inlineTextBox->isLeftToRightDirection();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

bool TextBox::dirOverride() const
{
    auto simple = [](const TextBoxIterator::SimplePath&) {
        return false;
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return inlineTextBox->dirOverride();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

StringView TextBox::text() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).text();
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return StringView(inlineTextBox->renderer().text()).substring(inlineTextBox->start(), inlineTextBox->len());
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

unsigned TextBox::localStartOffset() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).localStart();
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return inlineTextBox->start();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

unsigned TextBox::localEndOffset() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).localEnd();
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return inlineTextBox->end();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

unsigned TextBox::length() const
{
    auto simple = [](const TextBoxIterator::SimplePath& path) {
        return (*path.iterator).end() - (*path.iterator).start();
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return inlineTextBox->len();
    };

    return WTF::switchOn(iterator().m_pathVariant, simple, complex);
}

inline const TextBoxIterator& TextBox::iterator() const
{
    return static_cast<const TextBoxIterator&>(*this);
}

TextBoxIterator::TextBoxIterator(const InlineTextBox* inlineTextBox)
    : m_pathVariant(inlineTextBox)
{
}

TextBoxIterator::TextBoxIterator(SimpleLineLayout::RunResolver::Iterator iterator, SimpleLineLayout::RunResolver::Iterator end)
    : m_pathVariant(TextBoxIterator::SimplePath { iterator, end })
{
}

TextBoxIterator& TextBoxIterator::traverseNext()
{
    auto simple = [](TextBoxIterator::SimplePath& path) {
        ++path.iterator;
    };

    auto complex = [](const InlineTextBox*& inlineTextBox) {
        inlineTextBox = inlineTextBox->nextTextBox();
    };

    WTF::switchOn(m_pathVariant, simple, complex);

    return *this;
}

bool TextBoxIterator::operator==(const TextBoxIterator& other) const
{
    if (m_pathVariant.index() != other.m_pathVariant.index())
        return false;

    auto simple = [&](const TextBoxIterator::SimplePath& path) {
        return path.iterator == WTF::get<TextBoxIterator::SimplePath>(other.m_pathVariant).iterator;
    };

    auto complex = [&](const InlineTextBox* inlineTextBox) {
        return inlineTextBox == WTF::get<const InlineTextBox*>(other.m_pathVariant);
    };

    return WTF::switchOn(m_pathVariant, simple, complex);
}

bool TextBoxIterator::atEnd() const
{
    auto simple = [&](const TextBoxIterator::SimplePath& path) {
        return path.iterator == path.end;
    };

    auto complex = [&](const InlineTextBox* inlineTextBox) {
        return !inlineTextBox;
    };

    return WTF::switchOn(m_pathVariant, simple, complex);
}

Provider::Provider() = default;
Provider::~Provider() = default;

TextBoxIterator Provider::firstTextBoxFor(const RenderText& text)
{
    if (auto* simpleLineLayout = text.simpleLineLayout()) {
        auto& parent = downcast<const RenderBlockFlow>(*text.parent());
        auto& resolver = m_simpleLineLayoutResolvers.ensure(&parent, [&] {
            return makeUnique<SimpleLineLayout::RunResolver>(parent, *simpleLineLayout);
        }).iterator->value;

        auto range = resolver->rangeForRenderer(text);
        return { range.begin(), range.end() };
    }

    return TextBoxIterator { text.firstTextBox() };
}

TextBoxRange Provider::textBoxRangeFor(const RenderText& text)
{
    return { firstTextBoxFor(text) };
}

TextBoxIterator Provider::iteratorForInlineTextBox(const InlineTextBox* inlineTextBox)
{
    return TextBoxIterator { inlineTextBox };
}



}
}
