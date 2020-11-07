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
#include "DisplayBoxFactory.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBoxDecorationData.h"
#include "DisplayContainerBox.h"
#include "DisplayFillLayerImageGeometry.h"
#include "DisplayImageBox.h"
#include "DisplayTextBox.h"
#include "InlineLineGeometry.h"
#include "LayoutBoxGeometry.h"
#include "LayoutContainerBox.h"
#include "LayoutReplacedBox.h"

namespace WebCore {
namespace Display {

BoxFactory::BoxFactory(float pixelSnappingFactor)
    : m_pixelSnappingFactor(pixelSnappingFactor)
{
}

std::unique_ptr<Box> BoxFactory::displayBoxForRootBox(const Layout::ContainerBox& rootLayoutBox, const Layout::BoxGeometry& geometry) const
{
    // FIXME: Need to do logical -> physical coordinate mapping here.
    auto borderBoxRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(geometry) };

    auto style = Style { rootLayoutBox.style() };
    auto rootBox = makeUnique<ContainerBox>(snapRectToDevicePixels(borderBoxRect, m_pixelSnappingFactor), WTFMove(style));
    setupBoxModelBox(*rootBox, rootLayoutBox, geometry, { });
    return rootBox;
}

std::unique_ptr<Box> BoxFactory::displayBoxForLayoutBox(const Layout::Box& layoutBox, const Layout::BoxGeometry& geometry, LayoutSize offsetFromRoot) const
{
    // FIXME: Need to map logical to physical rects.
    auto borderBoxRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(geometry) };
    borderBoxRect.move(offsetFromRoot);
    auto pixelSnappedBorderBoxRect = snapRectToDevicePixels(borderBoxRect, m_pixelSnappingFactor);

    // FIXME: Handle isAnonymous()
    // FIXME: Do hoisting of <body> styles to the root where appropriate.

    // FIXME: Need to do logical -> physical coordinate mapping here.
    auto style = Style { layoutBox.style() };
    
    if (is<Layout::ReplacedBox>(layoutBox)) {
        // FIXME: Don't assume it's an image.
        RefPtr<Image> image;
        if (auto* cachedImage = downcast<Layout::ReplacedBox>(layoutBox).cachedImage())
            image = cachedImage->image();

        auto imageBox = makeUnique<ImageBox>(pixelSnappedBorderBoxRect, WTFMove(style), WTFMove(image));
        setupBoxModelBox(*imageBox, layoutBox, geometry, offsetFromRoot);
        return imageBox;
    }
    
    if (is<Layout::ContainerBox>(layoutBox)) {
        // FIXME: The decision to make a ContainerBox should be made based on whether this Display::Box will have children.
        auto containerBox = makeUnique<ContainerBox>(pixelSnappedBorderBoxRect, WTFMove(style));
        setupBoxModelBox(*containerBox, layoutBox, geometry, offsetFromRoot);
        return containerBox;
    }

    return makeUnique<Box>(snapRectToDevicePixels(borderBoxRect, m_pixelSnappingFactor), WTFMove(style));
}

std::unique_ptr<Box> BoxFactory::displayBoxForTextRun(const Layout::LineRun& run, const Layout::InlineLineGeometry& lineGeometry, LayoutSize offsetFromRoot) const
{
    ASSERT(run.text());
    auto lineRect = lineGeometry.logicalRect();
    auto lineLayoutRect = LayoutRect { lineRect.left(), lineRect.top(), lineRect.width(), lineRect.height() };

    auto runRect = LayoutRect { run.logicalLeft(), run.logicalTop(), run.logicalWidth(), run.logicalHeight() };
    runRect.moveBy(lineLayoutRect.location());
    runRect.move(offsetFromRoot);

    auto style = Style { run.layoutBox().style() };
    return makeUnique<TextBox>(snapRectToDevicePixels(runRect, m_pixelSnappingFactor), WTFMove(style), run);
}

void BoxFactory::setupBoxGeometry(BoxModelBox& box, const Layout::Box&, const Layout::BoxGeometry& layoutGeometry, LayoutSize offsetFromRoot) const
{
    auto borderBoxRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(layoutGeometry) };
    borderBoxRect.move(offsetFromRoot);

    auto paddingBoxRect = LayoutRect { layoutGeometry.paddingBox() };
    paddingBoxRect.moveBy(borderBoxRect.location());
    box.setAbsolutePaddingBoxRect(snapRectToDevicePixels(paddingBoxRect, m_pixelSnappingFactor));

    auto contentBoxRect = LayoutRect { layoutGeometry.contentBox() };
    contentBoxRect.moveBy(borderBoxRect.location());
    box.setAbsoluteContentBoxRect(snapRectToDevicePixels(contentBoxRect, m_pixelSnappingFactor));

    if (is<ReplacedBox>(box)) {
        auto& replacedBox = downcast<ReplacedBox>(box);
        // FIXME: Need to get the correct rect taking object-fit etc into account.
        auto replacedContentRect = LayoutRect { layoutGeometry.contentBoxLeft(), layoutGeometry.contentBoxTop(), layoutGeometry.contentBoxWidth(), layoutGeometry.contentBoxHeight() };
        replacedContentRect.moveBy(borderBoxRect.location());
        auto pixelSnappedReplacedContentRect = snapRectToDevicePixels(replacedContentRect, m_pixelSnappingFactor);
        replacedBox.setReplacedContentRect(pixelSnappedReplacedContentRect);
    }
}

std::unique_ptr<BoxDecorationData> BoxFactory::constructBoxDecorationData(const Layout::Box& layoutBox, const Layout::BoxGeometry& layoutGeometry, LayoutSize offsetFromRoot) const
{
    auto boxDecorationData = makeUnique<BoxDecorationData>();

    auto backgroundImageGeometry = calculateFillLayerImageGeometry(layoutBox, layoutGeometry, offsetFromRoot, m_pixelSnappingFactor);
    boxDecorationData->setBackgroundImageGeometry(WTFMove(backgroundImageGeometry));

    bool includeLogicalLeftEdge = true; // FIXME.
    bool includeLogicalRightEdge = true; // FIXME.
    auto borderEdges = calculateBorderEdges(layoutBox.style(), m_pixelSnappingFactor, includeLogicalLeftEdge, includeLogicalRightEdge);
    boxDecorationData->setBorderEdges(WTFMove(borderEdges));

    auto& renderStyle = layoutBox.style();

    if (renderStyle.hasBorderRadius()) {
        auto borderBoxRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(layoutGeometry) };
        auto borderRoundedRect = renderStyle.getRoundedBorderFor(borderBoxRect, includeLogicalLeftEdge, includeLogicalRightEdge);
        auto snappedRoundedRect = borderRoundedRect.pixelSnappedRoundedRectForPainting(m_pixelSnappingFactor);

        auto borderRadii = makeUnique<FloatRoundedRect::Radii>(snappedRoundedRect.radii());
        boxDecorationData->setBorderRadii(WTFMove(borderRadii));
    }

    return boxDecorationData;
}

void BoxFactory::setupBoxModelBox(BoxModelBox& box, const Layout::Box& layoutBox, const Layout::BoxGeometry& layoutGeometry, LayoutSize offsetFromRoot) const
{
    setupBoxGeometry(box, layoutBox, layoutGeometry, offsetFromRoot);

    auto& renderStyle = layoutBox.style();
    if (!renderStyle.hasBackground() && !renderStyle.hasBorder())
        return;

    auto boxDecorationData = constructBoxDecorationData(layoutBox, layoutGeometry, offsetFromRoot);
    box.setBoxDecorationData(WTFMove(boxDecorationData));
}


} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
