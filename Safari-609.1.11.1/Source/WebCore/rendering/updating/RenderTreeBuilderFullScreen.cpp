/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(FULLSCREEN_API)
#include "RenderTreeBuilderFullScreen.h"

#include "RenderFullScreen.h"

namespace WebCore {

RenderTreeBuilder::FullScreen::FullScreen(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

void RenderTreeBuilder::FullScreen::createPlaceholder(RenderFullScreen& renderer, std::unique_ptr<RenderStyle> style, const LayoutRect& frameRect)
{
    if (style->width().isAuto())
        style->setWidth(Length(frameRect.width(), Fixed));
    if (style->height().isAuto())
        style->setHeight(Length(frameRect.height(), Fixed));

    if (auto* placeHolder = renderer.placeholder()) {
        placeHolder->setStyle(WTFMove(*style));
        return;
    }

    if (!renderer.parent())
        return;

    auto newPlaceholder = createRenderer<RenderFullScreenPlaceholder>(renderer.document(), WTFMove(*style));
    newPlaceholder->initializeStyle();

    renderer.setPlaceholder(*newPlaceholder);

    m_builder.attach(*renderer.parent(), WTFMove(newPlaceholder), &renderer);
    renderer.parent()->setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderTreeBuilder::FullScreen::cleanupOnDestroy(RenderFullScreen& fullScreenRenderer)
{
    if (!fullScreenRenderer.placeholder())
        return;
    m_builder.destroy(*fullScreenRenderer.placeholder());
}

}

#endif
