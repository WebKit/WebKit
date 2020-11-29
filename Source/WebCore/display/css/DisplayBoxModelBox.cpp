/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DisplayBoxModelBox.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBoxClip.h"
#include "DisplayBoxDecorationData.h"
#include "DisplayBoxRareGeometry.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Display {

BoxModelBox::BoxModelBox(AbsoluteFloatRect borderBox, Style&& displayStyle, OptionSet<Flags> flags)
    : Box(borderBox, WTFMove(displayStyle), flags | Flags::BoxModelBox)
{
}

BoxModelBox::~BoxModelBox() = default;

void BoxModelBox::setBoxDecorationData(std::unique_ptr<BoxDecorationData>&& decorationData)
{
    m_boxDecorationData = WTFMove(decorationData);
}

void BoxModelBox::setBoxRareGeometry(std::unique_ptr<BoxRareGeometry>&& rareGeometry)
{
    m_boxRareGeometry = WTFMove(rareGeometry);
}

FloatRoundedRect BoxModelBox::borderRoundedRect() const
{
    auto borderRect = FloatRoundedRect { absoluteBorderBoxRect(), { } };
    auto* borderRadii = m_boxDecorationData ? m_boxDecorationData->borderRadii() : nullptr;
    if (borderRadii)
        borderRect.setRadii(*borderRadii);
    
    return borderRect;
}

FloatRoundedRect BoxModelBox::innerBorderRoundedRect() const
{
    if (!m_boxDecorationData)
        return borderRoundedRect();

    if (!m_boxDecorationData->borderRadii())
        return roundedInsetBorderForRect(absoluteBorderBoxRect(), { }, borderWidths(m_boxDecorationData->borderEdges()));

    return roundedInsetBorderForRect(absoluteBorderBoxRect(), *m_boxDecorationData->borderRadii(), borderWidths(m_boxDecorationData->borderEdges()));
}

void BoxModelBox::setAncestorClip(RefPtr<BoxClip>&& clip)
{
    m_ancestorClip = WTFMove(clip);
}

bool BoxModelBox::hasAncestorClip() const
{
    return m_ancestorClip && m_ancestorClip->clipRect();
}

RefPtr<BoxClip> BoxModelBox::clipForDescendants() const
{
    if (!style().hasClippedOverflow())
        return m_ancestorClip;

    auto clip = m_ancestorClip ? m_ancestorClip->copy() : BoxClip::create();

    auto pushClip = [&](BoxClip& boxClip) {
        if (m_boxDecorationData && m_boxDecorationData->hasBorderRadius()) {
            auto roundedInnerBorder = innerBorderRoundedRect();
            if (roundedInnerBorder.isRounded()) {
                boxClip.pushRoundedClip(roundedInnerBorder);
                return;
            }

            boxClip.pushClip(roundedInnerBorder.rect());
            return;
        }

        boxClip.pushClip(absolutePaddingBoxRect());
    };
    pushClip(clip);

    return clip;
}

String BoxModelBox::debugDescription() const
{
    TextStream stream;
    stream << "display box model box " << absoluteBorderBoxRect() << " (" << this << ")";
    if (m_ancestorClip)
        stream << " ancestor clip " << m_ancestorClip->clipRect() << " affected by radius " << m_ancestorClip->affectedByBorderRadius();

    return stream.release();
}

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
