/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "RenderModel.h"

#if ENABLE(MODEL_ELEMENT)

#include "HTMLAnchorElement.h"
#include "HTMLModelElement.h"
#include "NodeInlines.h"
#include "PaintInfo.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderObjectNode.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderStyle.h"
#include "RenderTheme.h"
#include "StyleDifference.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderModel);

RenderModel::RenderModel(HTMLModelElement& element, RenderStyle&& style)
    : RenderReplaced { Type::Model, element, WTF::move(style) }
{
    ASSERT(isRenderModel());
}

// Do not add any code to the destructor, instead, add it to willBeDestroyed().
RenderModel::~RenderModel() = default;

HTMLModelElement& NODELETE RenderModel::modelElement() const
{
    return downcast<HTMLModelElement>(nodeForNonAnonymous());
}

bool RenderModel::requiresLayer() const
{
    return true;
}

void RenderModel::updateFromElement()
{
    RenderReplaced::updateFromElement();
    update();
}

void RenderModel::styleDidChange(Style::Difference difference, const RenderStyle* oldStyle)
{
    RenderReplaced::styleDidChange(difference, oldStyle);

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
    if (!oldStyle || style().dynamicRangeLimit() != oldStyle->dynamicRangeLimit())
        protect(modelElement())->dynamicRangeLimitDidChange(style().dynamicRangeLimit().toPlatformDynamicRangeLimit());
#endif
}

void RenderModel::update()
{
    if (renderTreeBeingDestroyed())
        return;

    contentChanged(ContentChangeType::Model);
}

#if USE(SYSTEM_PREVIEW)
void RenderModel::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase != PaintPhase::Foreground)
        return;

    if (paintInfo.context().paintingDisabled())
        return;

    if (!modelElement().document().settings().systemPreviewEnabled())
        return;

    RefPtr anchor = dynamicDowncast<HTMLAnchorElement>(modelElement().parentElement());
    if (!anchor || !anchor->isSystemPreviewLink())
        return;

#if ENABLE(MODEL_PROCESS)
    // If the backing owns a dedicated badge layer (visionOS separated-portal case), it paints the badge itself.
    if (CheckedPtr layer = this->layer(); layer && layer->backing() && layer->backing()->systemPreviewBadgeLayer())
        return;
#endif

    LayoutRect contentRect = replacedContentRect();
    contentRect.moveBy(paintOffset);
    RefPtr document = modelElement().document();
    theme().paintSystemPreviewBadge(paintInfo, snapRectToDevicePixels(contentRect, document->deviceScaleFactor()));
}
#endif

}

#endif
