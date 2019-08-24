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
    auto simple = [](const SimpleLineLayout::RunResolver::Iterator simpleLineIterator) {
        return (*simpleLineIterator).rect();
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return inlineTextBox->frameRect();
    };

    return WTF::switchOn(m_iterator.m_pathVariant, simple, complex);
}

FloatRect TextBox::logicalRect() const
{
    auto simple = [](const SimpleLineLayout::RunResolver::Iterator simpleLineIterator) {
        return (*simpleLineIterator).rect();
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return inlineTextBox->logicalFrameRect();
    };

    return WTF::switchOn(m_iterator.m_pathVariant, simple, complex);
}

bool TextBox::hasHyphen() const
{
    auto simple = [](const SimpleLineLayout::RunResolver::Iterator simpleLineIterator) {
        return (*simpleLineIterator).hasHyphen();
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return inlineTextBox->hasHyphen();
    };

    return WTF::switchOn(m_iterator.m_pathVariant, simple, complex);
}

bool TextBox::isLeftToRightDirection() const
{
    auto simple = [](const SimpleLineLayout::RunResolver::Iterator) {
        return true;
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return inlineTextBox->isLeftToRightDirection();
    };

    return WTF::switchOn(m_iterator.m_pathVariant, simple, complex);
}

bool TextBox::dirOverride() const
{
    auto simple = [](const SimpleLineLayout::RunResolver::Iterator) {
        return false;
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return inlineTextBox->dirOverride();
    };

    return WTF::switchOn(m_iterator.m_pathVariant, simple, complex);
}

StringView TextBox::text() const
{
    auto simple = [](const SimpleLineLayout::RunResolver::Iterator simpleLineIterator) {
        return (*simpleLineIterator).text();
    };

    auto complex = [](const InlineTextBox* inlineTextBox) {
        return StringView(inlineTextBox->renderer().text()).substring(inlineTextBox->start(), inlineTextBox->len());
    };

    return WTF::switchOn(m_iterator.m_pathVariant, simple, complex);
}

TextBoxIterator::TextBoxIterator(const InlineTextBox* inlineTextBox)
    : m_pathVariant(inlineTextBox)
{
}

TextBoxIterator::TextBoxIterator(SimpleLineLayout::RunResolver::Iterator simpleLineIterator)
    : m_pathVariant(simpleLineIterator)
{
}

TextBoxIterator& TextBoxIterator::traverseNext()
{
    auto simple = [](SimpleLineLayout::RunResolver::Iterator& simpleLineIterator) {
        ++simpleLineIterator;
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

    auto simple = [&](const SimpleLineLayout::RunResolver::Iterator simpleLineIterator) {
        return simpleLineIterator == WTF::get<SimpleLineLayout::RunResolver::Iterator>(other.m_pathVariant);
    };

    auto complex = [&](const InlineTextBox* inlineTextBox) {
        return inlineTextBox == WTF::get<const InlineTextBox*>(other.m_pathVariant);
    };

    return WTF::switchOn(m_pathVariant, simple, complex);
}

static Optional<SimpleLineLayout::RunResolver> simpleLineRunResolverForText(const RenderText& text)
{
    if (!text.simpleLineLayout())
        return WTF::nullopt;
    return SimpleLineLayout::runResolver(downcast<const RenderBlockFlow>(*text.parent()), *text.simpleLineLayout());
}

static auto rangeForText(const RenderText& text, Optional<SimpleLineLayout::RunResolver>& simpleLineRunResolver)
{
    if (simpleLineRunResolver) {
        auto range = simpleLineRunResolver->rangeForRenderer(text);
        return WTF::makeIteratorRange(TextBoxIterator(range.begin()), TextBoxIterator(range.end()));
    }
    return WTF::makeIteratorRange(TextBoxIterator(text.firstTextBox()), TextBoxIterator(nullptr));
}

TextBoxRange::TextBoxRange(const RenderText& text)
    : m_simpleLineRunResolver(simpleLineRunResolverForText(text))
    , m_range(rangeForText(text, m_simpleLineRunResolver))
{
}

TextBoxRange textBoxes(const RenderText& text)
{
    return { text };
}

}
}
