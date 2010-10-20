/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "FindPageOverlay.h"

#include "FindController.h"
#include "WebPage.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/Page.h>

using namespace WebCore;

namespace WebKit {

static const float shadowOffsetX = 0.0;
static const float shadowOffsetY = 1.0;
static const float shadowBlurRadius = 2.0;
static const float whiteFrameThickness = 1.0;

static const int overlayBackgroundRed = 25;
static const int overlayBackgroundGreen = 25;
static const int overlayBackgroundBlue = 25;
static const int overlayBackgroundAlpha = 63;

PassOwnPtr<FindPageOverlay> FindPageOverlay::create(FindController* findController)
{
    return adoptPtr(new FindPageOverlay(findController));
}

FindPageOverlay::FindPageOverlay(FindController* findController)
    : m_findController(findController)
{
}

FindPageOverlay::~FindPageOverlay()
{
    m_findController->findPageOverlayDestroyed();
}

Vector<IntRect> FindPageOverlay::rectsForTextMatches()
{
    Vector<IntRect> rects;

    for (Frame* frame = webPage()->corePage()->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        Document* document = frame->document();
        if (!document)
            continue;

        IntRect visibleRect = frame->view()->visibleContentRect();
        Vector<IntRect> frameRects = document->markers()->renderedRectsForMarkers(DocumentMarker::TextMatch);
        IntPoint frameOffset(-frame->view()->scrollOffset().width(), -frame->view()->scrollOffset().height());
        frameOffset = frame->view()->convertToContainingWindow(frameOffset);

        for (Vector<IntRect>::iterator it = frameRects.begin(), end = frameRects.end(); it != end; ++it) {
            it->intersect(visibleRect);
            it->move(frameOffset.x(), frameOffset.y());
            rects.append(*it);
        }
    }

    return rects;
}

static Color overlayBackgroundColor()
{
    return Color(overlayBackgroundRed, overlayBackgroundGreen, overlayBackgroundBlue, overlayBackgroundAlpha);
}

void FindPageOverlay::drawRect(GraphicsContext& graphicsContext, const IntRect& dirtyRect)
{
    Vector<IntRect> rects = rectsForTextMatches();
    ASSERT(!rects.isEmpty());

    FrameView* frameView = webPage()->corePage()->mainFrame()->view();

    int width = frameView->width();
    if (frameView->verticalScrollbar())
        width -= frameView->verticalScrollbar()->width();
    int height = frameView->height();
    if (frameView->horizontalScrollbar())
        height -= frameView->horizontalScrollbar()->height();
    
    IntRect paintRect = intersection(dirtyRect, IntRect(0, 0, width, height));
    if (paintRect.isEmpty())
        return;

    graphicsContext.beginTransparencyLayer(1);
    graphicsContext.setCompositeOperation(CompositeCopy);

    // Draw the background.
    graphicsContext.fillRect(paintRect, overlayBackgroundColor(), ColorSpaceSRGB);

    graphicsContext.save();
    graphicsContext.setShadow(FloatSize(shadowOffsetX, shadowOffsetY), shadowBlurRadius, Color::black, ColorSpaceSRGB);

    graphicsContext.setFillColor(Color::white, ColorSpaceSRGB);

    // Draw white frames around the holes.
    for (size_t i = 0; i < rects.size(); ++i) {
        IntRect whiteFrameRect = rects[i];
        whiteFrameRect.inflate(1);

        graphicsContext.fillRect(whiteFrameRect);
    }

    graphicsContext.restore();

    graphicsContext.save();
    graphicsContext.setFillColor(Color::transparent, ColorSpaceSRGB);

    // Clear out the holes.
    for (size_t i = 0; i < rects.size(); ++i)
        graphicsContext.fillRect(rects[i]);

    graphicsContext.restore();
    graphicsContext.endTransparencyLayer();
}

bool FindPageOverlay::mouseEvent(const WebMouseEvent& event)
{
    if (event.type() == WebEvent::MouseDown) {
        // Dismiss the overlay.
        m_findController->hideFindUI();
        return false;
    }

    return false;
}

} // namespace WebKit
