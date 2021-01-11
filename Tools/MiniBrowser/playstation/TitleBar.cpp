/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include "TitleBar.h"

using namespace toolkitten;

void TitleBar::setTitle(const std::string& title)
{
    if (m_title == title)
        return;

    m_title = title;
    update();
}

void TitleBar::setProgress(double progress)
{
    if (m_progress == progress)
        return;

    m_progress = progress;
    update();
}

void TitleBar::update()
{
    auto widgetSize = size();
    int progressWidth = widgetSize.w * m_progress;

    IntRect rect { { 0, 0 }, widgetSize };
    IntRect progressRect = rect;
    progressRect.size.w = progressWidth;
    if (!progressRect.isEmpty())
        fillRect(progressRect, Color { 0.0, 0.0, 0.5, 1.0 });

    IntRect remainingRect = rect;
    remainingRect.size.w -= progressWidth;
    remainingRect.position.x += progressWidth;
    if (!remainingRect.isEmpty())
        fillRect(remainingRect, Color { 0.2, 0.2, 0.7, 1.0 });

    strokeRect(rect, 4, Color { 0.7, 0.7, 0.7, 1.0 });

    const int fontSize = 32;
    drawText(m_title.c_str(), { 2, 0 }, fontSize, WHITE);
}
