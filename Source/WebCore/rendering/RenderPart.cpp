/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
#include "RenderPart.h"

#include "Frame.h"
#include "FrameView.h"
#include "HTMLFrameElementBase.h"
#include "PluginViewBase.h"
#include "RenderSVGRoot.h"
#include "RenderView.h"

using namespace std;

namespace WebCore {

RenderPart::RenderPart(Element* node)
    : RenderWidget(node)
{
    setInline(false);
}

RenderPart::~RenderPart()
{
    clearWidget();
}

void RenderPart::setWidget(PassRefPtr<Widget> widget)
{
    if (widget == this->widget())
        return;

    RenderWidget::setWidget(widget);

    // make sure the scrollbars are set correctly for restore
    // ### find better fix
    viewCleared();
}

void RenderPart::viewCleared()
{
}

#if USE(ACCELERATED_COMPOSITING)
bool RenderPart::requiresLayer() const
{
    if (RenderWidget::requiresLayer())
        return true;
    
    return requiresAcceleratedCompositing();
}

bool RenderPart::requiresAcceleratedCompositing() const
{
    // There are two general cases in which we can return true. First, if this is a plugin 
    // renderer and the plugin has a layer, then we need a layer. Second, if this is 
    // a renderer with a contentDocument and that document needs a layer, then we need
    // a layer.
    if (widget() && widget()->isPluginViewBase() && static_cast<PluginViewBase*>(widget())->platformLayer())
        return true;

    if (!node() || !node()->isFrameOwnerElement())
        return false;

    HTMLFrameOwnerElement* element = static_cast<HTMLFrameOwnerElement*>(node());
    if (Document* contentDocument = element->contentDocument()) {
        if (RenderView* view = contentDocument->renderView())
            return view->usesCompositing();
    }

    return false;
}
#endif

#if ENABLE(SVG)
RenderSVGRoot* RenderPart::embeddedSVGContentRenderer() const
{
    if (!node() || !widget() || !widget()->isFrameView())
        return 0;

    FrameView* view = static_cast<FrameView*>(widget());
    RenderObject* contentRenderer = view->frame() ? view->frame()->contentRenderer() : 0;
    if (!contentRenderer)
        return 0;

    RenderObject* svgRootRenderer = contentRenderer->firstChild();
    if (!svgRootRenderer || !svgRootRenderer->isSVGRoot())
        return 0;

    return toRenderSVGRoot(svgRootRenderer);
}

int RenderPart::computeEmbeddedDocumentReplacedWidth(bool includeMaxWidth, RenderStyle* contentRenderStyle) const
{
    int logicalWidth = computeReplacedLogicalWidthUsing(contentRenderStyle->logicalWidth());
    int minLogicalWidth = computeReplacedLogicalWidthUsing(contentRenderStyle->logicalMinWidth());
    int maxLogicalWidth = !includeMaxWidth || contentRenderStyle->logicalMaxWidth().isUndefined() ? logicalWidth : computeReplacedLogicalWidthUsing(contentRenderStyle->logicalMaxWidth());
    int width = max(minLogicalWidth, min(logicalWidth, maxLogicalWidth));
    return width;
}

int RenderPart::computeEmbeddedDocumentReplacedHeight(RenderStyle* contentRenderStyle) const
{
    int logicalHeight = computeReplacedLogicalHeightUsing(contentRenderStyle->logicalHeight());
    int minLogicalHeight = computeReplacedLogicalHeightUsing(contentRenderStyle->logicalMinHeight());
    int maxLogicalHeight = contentRenderStyle->logicalMaxHeight().isUndefined() ? logicalHeight : computeReplacedLogicalHeightUsing(contentRenderStyle->logicalMaxHeight());
    int height = max(minLogicalHeight, min(logicalHeight, maxLogicalHeight));
    return height;
}

int RenderPart::computeReplacedLogicalWidth(bool includeMaxWidth) const
{
    RenderSVGRoot* contentRenderer = embeddedSVGContentRenderer();
    RenderStyle* contentRenderStyle = contentRenderer ? contentRenderer->style() : 0;

    if (!contentRenderer || !contentRenderStyle || style()->width().isSpecified())
        return RenderWidget::computeReplacedLogicalWidth(includeMaxWidth);

    // 10.3.2 Inline, replaced elements: http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-width
    if (style()->width().isAuto()) {
        bool heightIsAuto = style()->height().isAuto();
        bool hasIntrinsicWidth = contentRenderStyle->width().isFixed();

        // If 'height' and 'width' both have computed values of 'auto' and the element also has an intrinsic width, then that intrinsic width is the used value of 'width'.
        if (heightIsAuto && hasIntrinsicWidth)
            return computeEmbeddedDocumentReplacedWidth(includeMaxWidth, contentRenderStyle);
    
        bool hasIntrinsicHeight = contentRenderStyle->height().isFixed();
        FloatSize intrinsicRatio = contentRenderer->computeIntrinsicRatio();
        if (!intrinsicRatio.isEmpty()) {
            // If 'height' and 'width' both have computed values of 'auto' and the element has no intrinsic width, but does have an intrinsic height and intrinsic ratio;
            // or if 'width' has a computed value of 'auto', 'height' has some other computed value, and the element does have an intrinsic ratio; then the used value
            // of 'width' is: (used height) * (intrinsic ratio)
            if ((heightIsAuto && !hasIntrinsicWidth && hasIntrinsicHeight) || !heightIsAuto) {
                int logicalHeight = computeReplacedLogicalHeightUsing(heightIsAuto ? contentRenderStyle->logicalHeight() : style()->logicalHeight());
                return static_cast<int>(ceilf(logicalHeight * intrinsicRatio.width() / intrinsicRatio.height()));
            }

            // If 'height' and 'width' both have computed values of 'auto' and the element has an intrinsic ratio but no intrinsic height or width, then the used value of
            // 'width' is undefined in CSS 2.1. However, it is suggested that, if the containing block's width does not itself depend on the replaced element's width, then
            // the used value of 'width' is calculated from the constraint equation used for block-level, non-replaced elements in normal flow.
            // FIXME: Don't ignore padding/margin/border here.
            RenderBlock* containingBlock = this->containingBlock();
            if (heightIsAuto && !hasIntrinsicWidth && !hasIntrinsicHeight && containingBlock)
                return containingBlock->width();
        }

        // Otherwise, if 'width' has a computed value of 'auto', and the element has an intrinsic width, then that intrinsic width is the used value of 'width'.
        if (hasIntrinsicWidth)
            return computeEmbeddedDocumentReplacedWidth(includeMaxWidth, contentRenderStyle);
    }

    // Otherwise, if 'width' has a computed value of 'auto', but none of the conditions above are met, then the used value of 'width' becomes 300px. If 300px is too
    // wide to fit the device, UAs should use the width of the largest rectangle that has a 2:1 ratio and fits the device instead.
    return intrinsicLogicalWidth();
}

int RenderPart::computeReplacedLogicalHeight() const
{
    RenderSVGRoot* contentRenderer = embeddedSVGContentRenderer();
    RenderStyle* contentRenderStyle = contentRenderer ? contentRenderer->style() : 0;

    if (!contentRenderer || !contentRenderStyle || style()->height().isSpecified())
        return RenderWidget::computeReplacedLogicalHeight();

    // 10.6.2 Inline, replaced elements: http://www.w3.org/TR/CSS21/visudet.html#inline-replaced-height
    if (style()->height().isAuto()) {
        bool widthIsAuto = style()->width().isAuto();
        bool hasIntrinsicHeight = contentRenderStyle->height().isFixed();

        // If 'height' and 'width' both have computed values of 'auto' and the element also has an intrinsic height, then that intrinsic height is the used value of 'height'.
        if (widthIsAuto && hasIntrinsicHeight)
            return computeEmbeddedDocumentReplacedHeight(contentRenderStyle);
    
        // Otherwise, if 'height' has a computed value of 'auto', and the element has an intrinsic ratio then the used value of 'height' is:
        // (used width) / (intrinsic ratio)
        FloatSize intrinsicRatio = contentRenderer->computeIntrinsicRatio();
        if (!intrinsicRatio.isEmpty()) {
            int logicalWidth = computeReplacedLogicalWidthUsing(widthIsAuto ? contentRenderStyle->logicalWidth() : style()->logicalWidth());
            return static_cast<int>(ceilf(logicalWidth * intrinsicRatio.height() / intrinsicRatio.width()));
        }

        // Otherwise, if 'height' has a computed value of 'auto', and the element has an intrinsic height, then that intrinsic height is the used value of 'height'.
        if (hasIntrinsicHeight)
            return computeEmbeddedDocumentReplacedHeight(contentRenderStyle);
    }

    // Otherwise, if 'height' has a computed value of 'auto', but none of the conditions above are met, then the used value of 'height' must be set to the height
    // of the largest rectangle that has a 2:1 ratio, has a height not greater than 150px, and has a width not greater than the device width.
    return intrinsicLogicalHeight();
}

void RenderPart::layout()
{
    ASSERT(needsLayout());

    if ((style()->width().isAuto() || style()->height().isAuto()) && embeddedSVGContentRenderer())
        setPreferredLogicalWidthsDirty(true);

    RenderWidget::layout();
}
#endif

}
