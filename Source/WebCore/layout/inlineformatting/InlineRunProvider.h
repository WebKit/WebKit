/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "InlineItem.h"
#include <wtf/IsoMalloc.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {
namespace Layout {

using ItemPosition = unsigned;

class InlineRunProvider {
    WTF_MAKE_ISO_ALLOCATED(InlineRunProvider);
public:
    InlineRunProvider();

    void append(const InlineItem&);
    void insertBefore(const Box&, const Box& before);
    void remove(const Box&);

    struct Run {

        static Run createBoxRun(const InlineItem&);
        static Run createFloatRun(const InlineItem&);
        static Run createHardLineBreakRun(const InlineItem&);
        static Run createSoftLineBreakRun(const InlineItem&);
        static Run createWhitespaceRun(const InlineItem&, ItemPosition start, unsigned length, bool isCollapsible);
        static Run createNonWhitespaceRun(const InlineItem&, ItemPosition start, unsigned length);

        enum class Type {
            Box,
            Float,
            SoftLineBreak,
            HardLineBreak,
            Whitespace,
            NonWhitespace
        };
        Type type() const { return m_type; }
        bool isText() const { return m_type == Run::Type::Whitespace || m_type == Run::Type::NonWhitespace || m_type == Run::Type::SoftLineBreak || m_type == Run::Type::HardLineBreak; }
        bool isWhitespace() const { return m_type == Type::Whitespace; }
        bool isNonWhitespace() const { return m_type == Type::NonWhitespace; }
        bool isLineBreak() const { return m_type == Run::Type::SoftLineBreak || m_type == Run::Type::HardLineBreak; }
        bool isBox() const { return m_type == Type::Box; }
        bool isFloat() const { return m_type == Type::Float; }

        struct TextContext {

            enum class IsCollapsed { No, Yes };
            TextContext(ItemPosition, unsigned length, IsCollapsed);

            ItemPosition start() const { return m_start; }
            // Note that 'end' position does not equal to start + length when run overlaps multiple InlineItems.
            unsigned length() const { return m_length; }
            bool isCollapsed() const { return m_isCollapsed == IsCollapsed::Yes; }

        private:
            friend class InlineRunProvider;

            void expand(unsigned length) { m_length += length; }

            ItemPosition m_start { 0 };
            unsigned m_length { 0 };
            IsCollapsed m_isCollapsed { IsCollapsed::No };
        };
        std::optional<TextContext> textContext() const { return m_textContext; }
        // Note that style() and inlineItem() always returns the first InlineItem for a run.
        const RenderStyle& style() const { return m_inlineItem.style(); }
        const InlineItem& inlineItem() const { return m_inlineItem; }

    private:
        friend class InlineRunProvider;

        Run(const InlineItem&, Type, std::optional<TextContext>);
        std::optional<TextContext>& textContext() { return m_textContext; }

        const Type m_type;
        const InlineItem& m_inlineItem;
        std::optional<TextContext> m_textContext;
    };
    const Vector<InlineRunProvider::Run>& runs() const { return m_inlineRuns; }

private:
    void commitTextRun();
    void processInlineTextItem(const InlineItem&);
    unsigned moveToNextNonWhitespacePosition(const InlineItem&, ItemPosition currentPosition);
    unsigned moveToNextBreakablePosition(const InlineItem&, ItemPosition currentPosition);
    bool isContinousContent(Run::Type newRunType, const InlineItem& newInlineItem);

    LazyLineBreakIterator m_lineBreakIterator;

    Vector<InlineRunProvider::Run> m_inlineRuns;
};

inline InlineRunProvider::Run InlineRunProvider::Run::createBoxRun(const InlineItem& inlineItem)
{
    return { inlineItem, Type::Box, std::nullopt };
}

inline InlineRunProvider::Run InlineRunProvider::Run::createFloatRun(const InlineItem& inlineItem)
{
    return { inlineItem, Type::Float, std::nullopt };
}

inline InlineRunProvider::Run InlineRunProvider::Run::createSoftLineBreakRun(const InlineItem& inlineItem)
{
    return { inlineItem, Type::SoftLineBreak, std::nullopt };
}

inline InlineRunProvider::Run InlineRunProvider::Run::createHardLineBreakRun(const InlineItem& inlineItem)
{
    return { inlineItem, Type::HardLineBreak, std::nullopt };
}

inline InlineRunProvider::Run InlineRunProvider::Run::createWhitespaceRun(const InlineItem& inlineItem, ItemPosition start, unsigned length, bool isCollapsible)
{
    ASSERT(length);
    auto isCollapsed = isCollapsible && length > 1 ? TextContext::IsCollapsed::Yes : TextContext::IsCollapsed::No;
    return { inlineItem, Type::Whitespace, TextContext(start, length, isCollapsed) };
}

inline InlineRunProvider::Run InlineRunProvider::Run::createNonWhitespaceRun(const InlineItem& inlineItem, ItemPosition start, unsigned length)
{
    return { inlineItem, Type::NonWhitespace, TextContext(start, length, TextContext::IsCollapsed::No) };
}

inline InlineRunProvider::Run::Run(const InlineItem& inlineItem, Type type, std::optional<TextContext> textContext)
    : m_type(type)
    , m_inlineItem(inlineItem)
    , m_textContext(textContext)
{
}

inline InlineRunProvider::Run::TextContext::TextContext(ItemPosition start, unsigned length, IsCollapsed isCollapsed)
    : m_start(start)
    , m_length(length)
    , m_isCollapsed(isCollapsed)
{
}

}
}
#endif
