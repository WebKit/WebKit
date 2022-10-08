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

#include "CachedImage.h"
#include "DisplayBoxClip.h"
#include "DisplayBoxDecorationData.h"
#include "DisplayBoxDecorationPainter.h"
#include "DisplayBoxRareGeometry.h"
#include "DisplayContainerBox.h"
#include "DisplayFillLayerImageGeometry.h"
#include "DisplayImageBox.h"
#include "DisplayTextBox.h"
#include "DisplayTree.h"
#include "DisplayTreeBuilder.h"
#include "FloatPoint3D.h"
#include "InlineDisplayLine.h"
#include "LayoutBoxGeometry.h"
#include "LayoutElementBox.h"
#include "LayoutInitialContainingBlock.h"
#include "Logging.h"
#include "TransformationMatrix.h"

namespace WebCore {
namespace Display {

BoxFactory::BoxFactory(TreeBuilder& builder, float pixelSnappingFactor)
    : m_treeBuilder(builder)
    , m_pixelSnappingFactor(pixelSnappingFactor)
{
}

RootBackgroundPropagation BoxFactory::determineRootBackgroundPropagation(const Layout::ElementBox& rootLayoutBox)
{
    auto* documentElementBox = documentElementBoxFromRootBox(rootLayoutBox);
    auto* bodyBox = bodyBoxFromRootBox(rootLayoutBox);

    if (documentElementBox && documentElementBox->style().hasBackground())
        return RootBackgroundPropagation::None;

    if (bodyBox && bodyBox->style().hasBackground())
        return RootBackgroundPropagation::BodyToRoot;
    
    return RootBackgroundPropagation::None;
}

std::unique_ptr<ContainerBox> BoxFactory::displayBoxForRootBox(const Layout::ElementBox& rootLayoutBox, const Layout::BoxGeometry& geometry, RootBackgroundPropagation rootBackgroundPropagation) const
{
    ASSERT(is<Layout::InitialContainingBlock>(rootLayoutBox));

    // FIXME: Need to do logical -> physical coordinate mapping here.
    auto borderBoxRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(geometry) };

    auto* documentElementBox = documentElementBoxFromRootBox(rootLayoutBox);

    const RenderStyle* styleForBackground = documentElementBox ? &documentElementBox->style() : nullptr;

    if (rootBackgroundPropagation == RootBackgroundPropagation::BodyToRoot) {
        if (auto* bodyBox = bodyBoxFromRootBox(rootLayoutBox))
            styleForBackground = &bodyBox->style();
    }

    auto style = Style { rootLayoutBox.style(), styleForBackground };

    auto rootBox = makeUnique<ContainerBox>(m_treeBuilder.tree(), UnadjustedAbsoluteFloatRect { snapRectToDevicePixels(borderBoxRect, m_pixelSnappingFactor) }, WTFMove(style));
    // We pass rootBox as its own containingBlockBox here to allow it to be a reference everywhere else.
    setupBoxModelBox(*rootBox, rootLayoutBox, geometry, { *rootBox, { 0, 0 } }, styleForBackground);
    return rootBox;
}

std::unique_ptr<Box> BoxFactory::displayBoxForBodyBox(const Layout::Box& layoutBox, const Layout::BoxGeometry& geometry, const ContainingBlockContext& containingBlockContext, RootBackgroundPropagation rootBackgroundPropagation) const
{
    const RenderStyle* styleForBackground = &layoutBox.style();
    
    if (rootBackgroundPropagation == RootBackgroundPropagation::BodyToRoot)
        styleForBackground = nullptr;
    
    auto style = Style { layoutBox.style(), styleForBackground };
    return displayBoxForLayoutBox(layoutBox, geometry, containingBlockContext, styleForBackground, WTFMove(style));
}

std::unique_ptr<Box> BoxFactory::displayBoxForLayoutBox(const Layout::Box& layoutBox, const Layout::BoxGeometry& geometry, const ContainingBlockContext& containingBlockContext) const
{
    auto style = Style { layoutBox.style() };
    return displayBoxForLayoutBox(layoutBox, geometry, containingBlockContext, &layoutBox.style(), WTFMove(style));
}

std::unique_ptr<Box> BoxFactory::displayBoxForLayoutBox(const Layout::Box& layoutBox, const Layout::BoxGeometry& geometry, const ContainingBlockContext& containingBlockContext, const RenderStyle* styleForBackground, Style&& style) const
{
    // FIXME: Need to map logical to physical rects.
    auto borderBoxRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(geometry) };
    borderBoxRect.move(containingBlockContext.offsetFromRoot);
    auto pixelSnappedBorderBoxRect = UnadjustedAbsoluteFloatRect { snapRectToDevicePixels(borderBoxRect, m_pixelSnappingFactor) };

    // FIXME: Handle isAnonymous()
    
    if (is<Layout::ElementBox>(layoutBox) && downcast<Layout::ElementBox>(layoutBox).cachedImage()) {
        // FIXME: Don't assume it's an image.
        CachedResourceHandle<CachedImage> cachedImageHandle = downcast<Layout::ElementBox>(layoutBox).cachedImage();
        auto imageBox = makeUnique<ImageBox>(m_treeBuilder.tree(), pixelSnappedBorderBoxRect, WTFMove(style), WTFMove(cachedImageHandle));
        setupBoxModelBox(*imageBox, layoutBox, geometry, containingBlockContext, styleForBackground);
        return imageBox;
    }
    
    if (is<Layout::ElementBox>(layoutBox)) {
        // FIXME: The decision to make a ContainerBox should be made based on whether this Display::Box will have children.
        auto containerBox = makeUnique<ContainerBox>(m_treeBuilder.tree(), pixelSnappedBorderBoxRect, WTFMove(style));
        setupBoxModelBox(*containerBox, layoutBox, geometry, containingBlockContext, styleForBackground);
        return containerBox;
    }

    OptionSet<Box::TypeFlags> flags;
    // FIXME: Workaround for webkit.org/b/219335.
    if (layoutBox.isLineBreakBox())
        flags.add(Box::TypeFlags::LineBreakBox);

    return makeUnique<Box>(m_treeBuilder.tree(), pixelSnappedBorderBoxRect, WTFMove(style), flags);
}

std::unique_ptr<Box> BoxFactory::displayBoxForTextRun(const InlineDisplay::Box& box, const InlineDisplay::Line& line, const ContainingBlockContext& containingBlockContext) const
{
    UNUSED_PARAM(line);
    ASSERT(box.text());

    auto boxRect = LayoutRect { box.left(), box.top(), box.width(), box.height() };
    boxRect.move(containingBlockContext.offsetFromRoot);

    auto style = Style { box.layoutBox().style() };
    return makeUnique<TextBox>(m_treeBuilder.tree(), UnadjustedAbsoluteFloatRect { snapRectToDevicePixels(boxRect, m_pixelSnappingFactor) }, WTFMove(style), box);
}

void BoxFactory::setupBoxGeometry(BoxModelBox& box, const Layout::Box&, const Layout::BoxGeometry& layoutGeometry, const ContainingBlockContext& containingBlockContext) const
{
    auto borderBoxRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(layoutGeometry) };
    borderBoxRect.move(containingBlockContext.offsetFromRoot);

    auto paddingBoxRect = LayoutRect { layoutGeometry.paddingBox() };
    paddingBoxRect.moveBy(borderBoxRect.location());
    box.setAbsolutePaddingBoxRect(UnadjustedAbsoluteFloatRect { snapRectToDevicePixels(paddingBoxRect, m_pixelSnappingFactor) });

    auto contentBoxRect = LayoutRect { layoutGeometry.contentBox() };
    contentBoxRect.moveBy(borderBoxRect.location());
    box.setAbsoluteContentBoxRect(UnadjustedAbsoluteFloatRect { snapRectToDevicePixels(contentBoxRect, m_pixelSnappingFactor) });

    if (is<ReplacedBox>(box)) {
        auto& replacedBox = downcast<ReplacedBox>(box);
        // FIXME: Need to get the correct rect taking object-fit etc into account.
        auto replacedContentRect = LayoutRect { layoutGeometry.contentBoxLeft(), layoutGeometry.contentBoxTop(), layoutGeometry.contentBoxWidth(), layoutGeometry.contentBoxHeight() };
        replacedContentRect.moveBy(borderBoxRect.location());
        auto pixelSnappedReplacedContentRect = UnadjustedAbsoluteFloatRect { snapRectToDevicePixels(replacedContentRect, m_pixelSnappingFactor) };
        replacedBox.setReplacedContentRect(pixelSnappedReplacedContentRect);
    }
}

std::unique_ptr<BoxDecorationData> BoxFactory::constructBoxDecorationData(const Layout::Box& layoutBox, const Layout::BoxGeometry& layoutGeometry, const RenderStyle* styleForBackground, LayoutSize offsetFromRoot) const
{
    auto boxDecorationData = makeUnique<BoxDecorationData>();

    if (styleForBackground) {
        auto backgroundImageGeometry = calculateFillLayerImageGeometry(*styleForBackground, layoutGeometry, offsetFromRoot, m_pixelSnappingFactor);
        boxDecorationData->setBackgroundImageGeometry(WTFMove(backgroundImageGeometry));
    }

    bool includeLogicalLeftEdge = true; // FIXME.
    bool includeLogicalRightEdge = true; // FIXME.
    auto borderEdges = calculateBorderEdges(layoutBox.style(), m_pixelSnappingFactor, includeLogicalLeftEdge, includeLogicalRightEdge);
    boxDecorationData->setBorderEdges(WTFMove(borderEdges));

    return boxDecorationData;
}

FloatPoint3D BoxFactory::computeTransformOrigin(const BoxModelBox& box, const Layout::BoxGeometry& layoutGeometry, const RenderStyle& renderStyle, LayoutSize offsetFromRoot) const
{
    auto transformBoxRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(layoutGeometry) };

    auto absoluteOrigin = LayoutPoint {
        offsetFromRoot.width() + transformBoxRect.x() + valueForLength(renderStyle.transformOriginX(), transformBoxRect.width()),
        offsetFromRoot.height() + transformBoxRect.y() + valueForLength(renderStyle.transformOriginY(), transformBoxRect.height())
    };

    auto snappedAbsoluteOrigin = roundPointToDevicePixels(absoluteOrigin, m_pixelSnappingFactor);
    auto boxRelativeTransformOriginXY = snappedAbsoluteOrigin - box.absoluteBorderBoxRect().location();

    return { boxRelativeTransformOriginXY.width(), boxRelativeTransformOriginXY.height(), renderStyle.transformOriginZ() };
}

TransformationMatrix BoxFactory::computeTransformationMatrix(const BoxModelBox& box, const Layout::BoxGeometry& layoutGeometry, const RenderStyle& renderStyle, LayoutSize offsetFromRoot) const
{
    auto boxRelativeTransformOrigin = computeTransformOrigin(box, layoutGeometry, renderStyle, offsetFromRoot);

    // FIXME: Respect transform-box.
    auto transformBoxRect = box.absoluteBorderBoxRect();

    // FIXME: This is similar to RenderStyle::applyTransform(), but that fails to pixel snap transform origin.
    auto transform = TransformationMatrix { };
    transform.translate3d(boxRelativeTransformOrigin.x(), boxRelativeTransformOrigin.y(), boxRelativeTransformOrigin.z());

    for (auto& operation : renderStyle.transform().operations())
        operation->apply(transform, transformBoxRect.size());

    transform.translate3d(-boxRelativeTransformOrigin.x(), -boxRelativeTransformOrigin.y(), -boxRelativeTransformOrigin.z());
    return transform;

}

std::unique_ptr<BoxRareGeometry> BoxFactory::constructBoxRareGeometry(const BoxModelBox& box, const Layout::Box& layoutBox, const Layout::BoxGeometry& layoutGeometry, LayoutSize offsetFromRoot) const
{
    auto& renderStyle = layoutBox.style();

    if (!box.style().hasTransform() && !renderStyle.hasBorderRadius())
        return nullptr;

    auto boxRareGeometry = makeUnique<BoxRareGeometry>();

    if (renderStyle.hasBorderRadius()) {
        bool includeLogicalLeftEdge = true; // FIXME.
        bool includeLogicalRightEdge = true; // FIXME.

        auto borderBoxRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(layoutGeometry) };
        auto borderRoundedRect = renderStyle.getRoundedBorderFor(borderBoxRect, includeLogicalLeftEdge, includeLogicalRightEdge);
        auto snappedRoundedRect = borderRoundedRect.pixelSnappedRoundedRectForPainting(m_pixelSnappingFactor);

        auto borderRadii = makeUnique<FloatRoundedRect::Radii>(snappedRoundedRect.radii());
        boxRareGeometry->setBorderRadii(WTFMove(borderRadii));
    }

    if (box.style().hasTransform()) {
        auto transformationMatrix = computeTransformationMatrix(box, layoutGeometry, layoutBox.style(), offsetFromRoot);
        boxRareGeometry->setTransform(WTFMove(transformationMatrix));
    }

    return boxRareGeometry;
}

void BoxFactory::setupBoxModelBox(BoxModelBox& box, const Layout::Box& layoutBox, const Layout::BoxGeometry& layoutGeometry, const ContainingBlockContext& containingBlockContext, const RenderStyle* styleForBackground) const
{
    setupBoxGeometry(box, layoutBox, layoutGeometry, containingBlockContext);

    auto boxRareGeometry = constructBoxRareGeometry(box, layoutBox, layoutGeometry, containingBlockContext.offsetFromRoot);
    box.setBoxRareGeometry(WTFMove(boxRareGeometry));

    box.setHasTransform(box.style().hasTransform());

    auto& renderStyle = layoutBox.style();
    if (!(styleForBackground && styleForBackground->hasBackground()) && !renderStyle.hasBorder()) // FIXME: Misses border-radius.
        return;

    auto boxDecorationData = constructBoxDecorationData(layoutBox, layoutGeometry, styleForBackground, containingBlockContext.offsetFromRoot);
    box.setBoxDecorationData(WTFMove(boxDecorationData));

    if (box.participatesInZOrderSorting()) {
        RefPtr<BoxClip> clip = containingBlockContext.box.clipForDescendants();
        box.setAncestorClip(WTFMove(clip));
    }
}

const Layout::ElementBox* BoxFactory::documentElementBoxFromRootBox(const Layout::ElementBox& rootLayoutBox)
{
    auto* documentBox = rootLayoutBox.firstChild();
    if (!documentBox || !documentBox->isDocumentBox() || !is<Layout::ElementBox>(documentBox))
        return nullptr;

    return downcast<Layout::ElementBox>(documentBox);
}

const Layout::Box* BoxFactory::bodyBoxFromRootBox(const Layout::ElementBox& rootLayoutBox)
{
    auto* documentBox = rootLayoutBox.firstChild();
    if (!documentBox || !documentBox->isDocumentBox() || !is<Layout::ElementBox>(documentBox))
        return nullptr;

    auto* bodyBox = downcast<Layout::ElementBox>(documentBox)->firstChild();
    if (!bodyBox || !bodyBox->isBodyBox())
        return nullptr;

    return bodyBox;
}

} // namespace Display
} // namespace WebCore

