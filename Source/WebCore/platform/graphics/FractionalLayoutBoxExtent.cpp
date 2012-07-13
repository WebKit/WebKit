/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FractionalLayoutBoxExtent.h"

#include "RenderStyle.h"

namespace WebCore {

FractionalLayoutUnit FractionalLayoutBoxExtent::logicalTop(const RenderStyle* style) const
{
    return style->isHorizontalWritingMode() ? m_top : m_left;
}

FractionalLayoutUnit FractionalLayoutBoxExtent::logicalBottom(const RenderStyle* style) const
{
    return style->isHorizontalWritingMode() ? m_bottom : m_right;
}

FractionalLayoutUnit FractionalLayoutBoxExtent::logicalLeft(const RenderStyle* style) const
{
    return style->isHorizontalWritingMode() ? m_left : m_top;
}

FractionalLayoutUnit FractionalLayoutBoxExtent::logicalRight(const RenderStyle* style) const
{
    return style->isHorizontalWritingMode() ? m_right : m_bottom;
}

FractionalLayoutUnit FractionalLayoutBoxExtent::before(const RenderStyle* style) const
{
    switch (style->writingMode()) {
    case TopToBottomWritingMode:
        return m_top;
    case BottomToTopWritingMode:
        return m_bottom;
    case LeftToRightWritingMode:
        return m_left;
    case RightToLeftWritingMode:
        return m_right;
    }
    ASSERT_NOT_REACHED();
    return m_top;
}

FractionalLayoutUnit FractionalLayoutBoxExtent::after(const RenderStyle* style) const
{
    switch (style->writingMode()) {
    case TopToBottomWritingMode:
        return m_bottom;
    case BottomToTopWritingMode:
        return m_top;
    case LeftToRightWritingMode:
        return m_right;
    case RightToLeftWritingMode:
        return m_left;
    }
    ASSERT_NOT_REACHED();
    return m_bottom;
}

FractionalLayoutUnit FractionalLayoutBoxExtent::start(const RenderStyle* style) const
{
    if (style->isHorizontalWritingMode())
        return style->isLeftToRightDirection() ? m_left : m_right;
    return style->isLeftToRightDirection() ? m_top : m_bottom;
}

FractionalLayoutUnit FractionalLayoutBoxExtent::end(const RenderStyle* style) const
{
    if (style->isHorizontalWritingMode())
        return style->isLeftToRightDirection() ? m_right : m_left;
    return style->isLeftToRightDirection() ? m_bottom : m_top;
}

void FractionalLayoutBoxExtent::setBefore(const RenderStyle* style, FractionalLayoutUnit value)
{
    switch (style->writingMode()) {
    case TopToBottomWritingMode:
        m_top = value;
        break;
    case BottomToTopWritingMode:
        m_bottom = value;
        break;
    case LeftToRightWritingMode:
        m_left = value;
        break;
    case RightToLeftWritingMode:
        m_right = value;
        break;
    default:
        ASSERT_NOT_REACHED();
        m_top = value;
    }
}

void FractionalLayoutBoxExtent::setAfter(const RenderStyle* style, FractionalLayoutUnit value)
{
    switch (style->writingMode()) {
    case TopToBottomWritingMode:
        m_bottom = value;
        break;
    case BottomToTopWritingMode:
        m_top = value;
        break;
    case LeftToRightWritingMode:
        m_right = value;
        break;
    case RightToLeftWritingMode:
        m_left = value;
        break;
    default:
        ASSERT_NOT_REACHED();
        m_bottom = value;
    }
}

void FractionalLayoutBoxExtent::setStart(const RenderStyle* style, FractionalLayoutUnit value)
{
    if (style->isHorizontalWritingMode()) {
        if (style->isLeftToRightDirection())
            m_left = value;
        else
            m_right = value;
    } else {
        if (style->isLeftToRightDirection())
            m_top = value;
        else
            m_bottom = value;
    }
}

void FractionalLayoutBoxExtent::setEnd(const RenderStyle* style, FractionalLayoutUnit value)
{
    if (style->isHorizontalWritingMode()) {
        if (style->isLeftToRightDirection())
            m_right = value;
        else
            m_left = value;
    } else {
        if (style->isLeftToRightDirection())
            m_bottom = value;
        else
            m_top = value;
    }
}

FractionalLayoutUnit& FractionalLayoutBoxExtent::mutableLogicalLeft(const RenderStyle* style)
{
    return style->isHorizontalWritingMode() ? m_left : m_top;
}

FractionalLayoutUnit& FractionalLayoutBoxExtent::mutableLogicalRight(const RenderStyle* style)
{
    return style->isHorizontalWritingMode() ? m_right : m_bottom;
}

FractionalLayoutUnit& FractionalLayoutBoxExtent::mutableBefore(const RenderStyle* style)
{
    return style->isHorizontalWritingMode() ? (style->isFlippedBlocksWritingMode() ? m_bottom : m_top) :
                                              (style->isFlippedBlocksWritingMode() ? m_right: m_left);
}

FractionalLayoutUnit& FractionalLayoutBoxExtent::mutableAfter(const RenderStyle* style)
{
    return style->isHorizontalWritingMode() ? (style->isFlippedBlocksWritingMode() ? m_top : m_bottom) :
                                              (style->isFlippedBlocksWritingMode() ? m_left: m_right);
}

} // namespace WebCore
