/*
 * Copyright (C) 2015, 2020 Igalia S.L.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ProgressBarAdwaita.h"

#include "GraphicsContextStateSaver.h"
#include "ProgressBarPart.h"

#if USE(THEME_ADWAITA)

namespace WebCore {
using namespace WebCore::Adwaita;

ProgressBarAdwaita::ProgressBarAdwaita(ControlPart& part, ControlFactoryAdwaita& controlFactory)
    : ControlAdwaita(part, controlFactory)
{
}

static double currentAnimationProgress(Seconds animationStartTime)
{
    auto duration = progressAnimationDuration;
    return fmod((MonotonicTime::now().secondsSinceEpoch() - animationStartTime).seconds(), duration.seconds()) / duration.seconds();
}

void ProgressBarAdwaita::draw(GraphicsContext& graphicsContext, const FloatRoundedRect& borderRect, float /*deviceScaleFactor*/, const ControlStyle& style)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    SRGBA<uint8_t> progressBarBackgroundColor;

    if (style.states.contains(ControlStyle::State::DarkAppearance))
        progressBarBackgroundColor = progressBarBackgroundColorDark;
    else
        progressBarBackgroundColor = progressBarBackgroundColorLight;

    FloatRect fieldRect = borderRect.rect();
    FloatSize corner(3, 3);
    Path path;

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    graphicsContext.setFillColor(progressBarBackgroundColor);
    graphicsContext.fillPath(path);
    path.clear();

    auto& progressBarPart = owningProgressBarPart();
    bool isDeterminate = progressBarPart.position() >= 0;
    if (isDeterminate) {
        auto progressWidth = fieldRect.width() * progressBarPart.position();
        if (style.states.contains(ControlStyle::State::InlineFlippedWritingMode))
            fieldRect.move(fieldRect.width() - progressWidth, 0);
        fieldRect.setWidth(progressWidth);
    } else {
        double animationProgress = currentAnimationProgress(progressBarPart.animationStartTime());

        // Never let the progress rect shrink smaller than 2 pixels.
        fieldRect.setWidth(std::max<float>(2, fieldRect.width() / progressActivityBlocks));
        auto movableWidth = borderRect.rect().width() - fieldRect.width();

        // We want the first 0.5 units of the animation progress to represent the
        // forward motion and the second 0.5 units to represent the backward motion,
        // thus we multiply by two here to get the full sweep of the progress bar with
        // each direction.
        if (animationProgress < 0.5)
            fieldRect.move(animationProgress * 2 * movableWidth, 0);
        else
            fieldRect.move((1.0 - animationProgress) * 2 * movableWidth, 0);
    }

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);

    graphicsContext.setFillColor(accentColor(style));
    graphicsContext.fillPath(path);
}

} // namespace WebCore

#endif // USE(THEME_ADWAITA)
