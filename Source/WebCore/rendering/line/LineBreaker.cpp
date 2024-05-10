/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 ChangSeok Oh <shivamidow@gmail.com>
 * Copyright (C) 2013 Adobe Systems Inc. All right reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "LineBreaker.h"

#include "BreakingContext.h"

namespace WebCore {

void LineBreaker::skipTrailingWhitespace(LegacyInlineIterator& iterator, const LineInfo& lineInfo)
{
    while (!iterator.atEnd() && !requiresLineBox(iterator, lineInfo, TrailingWhitespace))
        iterator.increment();
}

void LineBreaker::skipLeadingWhitespace(InlineBidiResolver& resolver, LineInfo& lineInfo)
{
    while (!resolver.position().atEnd() && !requiresLineBox(resolver.position(), lineInfo, LeadingWhitespace))
        resolver.increment();

    resolver.commitExplicitEmbedding();
}

LegacyInlineIterator LineBreaker::nextLineBreak(InlineBidiResolver& resolver, LineInfo& lineInfo, RenderTextInfo& renderTextInfo)
{
    ASSERT(resolver.position().root() == &m_block);

    bool appliedStartWidth = resolver.position().offset();

    LineWidth width(m_block, lineInfo.isFirstLine());

    skipLeadingWhitespace(resolver, lineInfo);

    if (resolver.position().atEnd())
        return resolver.position();

    BreakingContext context(*this, resolver, lineInfo, width, renderTextInfo, appliedStartWidth, m_block);

    while (context.currentObject()) {
        context.initializeForCurrentObject();
        if (context.currentObject()->isRenderInline()) {
            context.handleEmptyInline();
        } else if (context.currentObject()->isRenderText()) {
            if (context.handleText()) {
                // We've hit a hard text line break. Our line break iterator is updated, so early return.
                return context.lineBreak();
            }
        } else if (context.currentObject()->isLineBreakOpportunity())
            context.commitLineBreakAtCurrentWidth(*context.currentObject());
        else
            ASSERT_NOT_REACHED();

        if (context.atEnd())
            return context.handleEndOfLine();

        context.commitAndUpdateLineBreakIfNeeded();

        if (context.atEnd())
            return context.handleEndOfLine();

        context.increment();
    }

    context.clearLineBreakIfFitsOnLine(true);

    return context.handleEndOfLine();
}

}
