/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "ImageControlsButtonElementMac.h"

#if ENABLE(SERVICE_CONTROLS)

#include "ContextMenuController.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "HTMLDivElement.h"
#include "Page.h"
#include "Range.h"
#include "RenderBlockFlow.h"
#include "RenderStyle.h"
#include "RenderTheme.h"
#include "ShadowRoot.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageControlsButtonElementMac);

class RenderImageControlsButton final : public RenderBlockFlow {
    WTF_MAKE_ISO_ALLOCATED_INLINE(RenderImageControlsButton);
public:
    RenderImageControlsButton(HTMLElement&, RenderStyle&&);
    virtual ~RenderImageControlsButton();

private:
    void updateLogicalWidth() override;
    LogicalExtentComputedValues computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const override;

    const char* renderName() const override { return "RenderImageControlsButton"; }
};

RenderImageControlsButton::RenderImageControlsButton(HTMLElement& element, RenderStyle&& style)
    : RenderBlockFlow(element, WTFMove(style))
{
}

RenderImageControlsButton::~RenderImageControlsButton() = default;

void RenderImageControlsButton::updateLogicalWidth()
{
    RenderBox::updateLogicalWidth();

    IntSize frameSize = theme().imageControlsButtonSize(*this);
    setLogicalWidth(isHorizontalWritingMode() ? frameSize.width() : frameSize.height());
}

RenderBox::LogicalExtentComputedValues RenderImageControlsButton::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const
{
    auto computedValues = RenderBox::computeLogicalHeight(logicalHeight, logicalTop);
    IntSize frameSize = theme().imageControlsButtonSize(*this);
    computedValues.m_extent = isHorizontalWritingMode() ? frameSize.height() : frameSize.width();
    return computedValues;
}

ImageControlsButtonElementMac::ImageControlsButtonElementMac(Document& document)
    : HTMLDivElement(HTMLNames::divTag, document)
{
}

ImageControlsButtonElementMac::~ImageControlsButtonElementMac() = default;

RefPtr<ImageControlsButtonElementMac> ImageControlsButtonElementMac::tryCreate(Document& document)
{
    if (!document.page())
        return nullptr;

    auto button = adoptRef(*new ImageControlsButtonElementMac(document));
    button->setAttributeWithoutSynchronization(HTMLNames::classAttr, AtomString("x-webkit-image-controls-button", AtomString::ConstructFromLiteral));

    IntSize positionOffset = RenderTheme::singleton().imageControlsButtonPositionOffset();
    button->setInlineStyleProperty(CSSPropertyTop, positionOffset.height(), CSSPrimitiveValue::CSS_PX);

    // FIXME: Why is right: 0px off the right edge of the parent?
    button->setInlineStyleProperty(CSSPropertyRight, positionOffset.width(), CSSPrimitiveValue::CSS_PX);

    return WTFMove(button);
}

void ImageControlsButtonElementMac::defaultEventHandler(Event& event)
{
    if (event.type() == eventNames().clickEvent) {
        RefPtr<Frame> frame = document().frame();
        if (!frame)
            return;

        Page* page = document().page();
        if (!page)
            return;

        page->contextMenuController().showImageControlsMenu(event);
        event.setDefaultHandled();
        return;
    }

    HTMLDivElement::defaultEventHandler(event);
}

RenderPtr<RenderElement> ImageControlsButtonElementMac::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderImageControlsButton>(*this, WTFMove(style));
}

} // namespace WebCore

#endif // ENABLE(SERVICE_CONTROLS)
