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
#include "ImageControlsRootElementMac.h"

#if ENABLE(SERVICE_CONTROLS)

#include "HTMLElement.h"
#include "ImageControlsButtonElementMac.h"
#include "RenderBlockFlow.h"
#include "RenderImage.h"
#include "RenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageControlsRootElementMac);

class RenderImageControls final : public RenderBlockFlow {
    WTF_MAKE_ISO_ALLOCATED_INLINE(RenderImageControls);
public:
    RenderImageControls(HTMLElement&, RenderStyle&&);
    virtual ~RenderImageControls();

private:
    void updateLogicalWidth() override;
    LogicalExtentComputedValues computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const override;

    const char* renderName() const override { return "RenderImageControls"; }
};

RenderImageControls::RenderImageControls(HTMLElement& element, RenderStyle&& style)
    : RenderBlockFlow(element, WTFMove(style))
{
}

RenderImageControls::~RenderImageControls() = default;

void RenderImageControls::updateLogicalWidth()
{
    RenderBox::updateLogicalWidth();

    RenderElement* renderer = element()->shadowHost()->renderer();
    if (!is<RenderImage>(*renderer))
        return;

    setLogicalWidth(downcast<RenderImage>(*renderer).logicalWidth());
}

RenderBox::LogicalExtentComputedValues RenderImageControls::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const
{
    auto computedValues = RenderBox::computeLogicalHeight(logicalHeight, logicalTop);
    RenderElement* renderer = element()->shadowHost()->renderer();
    if (!is<RenderImage>(*renderer))
        return computedValues;

    computedValues.m_extent = downcast<RenderImage>(*renderer).logicalHeight();
    return computedValues;
}

RefPtr<ImageControlsRootElement> ImageControlsRootElement::tryCreate(Document& document)
{
    if (!document.page())
        return nullptr;

    Ref<ImageControlsRootElementMac> controls = adoptRef(*new ImageControlsRootElementMac(document));
    controls->setAttributeWithoutSynchronization(HTMLNames::classAttr, AtomString("x-webkit-image-controls", AtomString::ConstructFromLiteral));

    if (RefPtr<ImageControlsButtonElementMac> button = ImageControlsButtonElementMac::tryCreate(document))
        controls->appendChild(*button);

    return WTFMove(controls);
}

ImageControlsRootElementMac::ImageControlsRootElementMac(Document& document)
    : ImageControlsRootElement(document)
{
}

ImageControlsRootElementMac::~ImageControlsRootElementMac() = default;

RenderPtr<RenderElement> ImageControlsRootElementMac::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderImageControls>(*this, WTFMove(style));
}

} // namespace WebCore

#endif // ENABLE(SERVICE_CONTROLS)
