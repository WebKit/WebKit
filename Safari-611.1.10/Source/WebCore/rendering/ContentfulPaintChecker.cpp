/*
 * Copyright (C) 2020 WikiMedia Inc. All rights reserved.
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
#include "ContentfulPaintChecker.h"

#include "FrameView.h"
#include "GraphicsContext.h"
#include "RenderView.h"

namespace WebCore {

bool ContentfulPaintChecker::qualifiesForContentfulPaint(FrameView& frameView)
{
    ASSERT(!frameView.needsLayout());
    ASSERT(frameView.renderView());

    auto oldPaintBehavior = frameView.paintBehavior();
    auto oldEntireContents = frameView.paintsEntireContents();

    frameView.setPaintBehavior(PaintBehavior::FlattenCompositingLayers);
    frameView.setPaintsEntireContents(true);

    GraphicsContext checkerContext(GraphicsContext::PaintInvalidationReasons::DetectingContentfulPaint);
    frameView.paint(checkerContext, frameView.renderView()->documentRect());

    frameView.setPaintsEntireContents(oldEntireContents);
    frameView.setPaintBehavior(oldPaintBehavior);

    return checkerContext.contenfulPaintDetected();
}

}
