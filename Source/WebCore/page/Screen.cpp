/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Screen.h"

#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "PlatformScreen.h"
#include "ResourceLoadObserver.h"
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

Screen::Screen(DOMWindow& window)
    : DOMWindowProperty(&window)
{
}

unsigned Screen::height() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logScreenAPIAccessed(*frame->document(), ResourceLoadStatistics::ScreenAPI::Height);
    long height = static_cast<long>(screenRect(frame->view()).height());
    return static_cast<unsigned>(height);
}

unsigned Screen::width() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logScreenAPIAccessed(*frame->document(), ResourceLoadStatistics::ScreenAPI::Width);
    long width = static_cast<long>(screenRect(frame->view()).width());
    return static_cast<unsigned>(width);
}

unsigned Screen::colorDepth() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logScreenAPIAccessed(*frame->document(), ResourceLoadStatistics::ScreenAPI::ColorDepth);
    return static_cast<unsigned>(screenDepth(frame->view()));
}

unsigned Screen::pixelDepth() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logScreenAPIAccessed(*frame->document(), ResourceLoadStatistics::ScreenAPI::PixelDepth);
    return static_cast<unsigned>(screenDepth(frame->view()));
}

int Screen::availLeft() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logScreenAPIAccessed(*frame->document(), ResourceLoadStatistics::ScreenAPI::AvailLeft);
    return static_cast<int>(screenAvailableRect(frame->view()).x());
}

int Screen::availTop() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logScreenAPIAccessed(*frame->document(), ResourceLoadStatistics::ScreenAPI::AvailTop);
    return static_cast<int>(screenAvailableRect(frame->view()).y());
}

unsigned Screen::availHeight() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logScreenAPIAccessed(*frame->document(), ResourceLoadStatistics::ScreenAPI::AvailHeight);
    return static_cast<unsigned>(screenAvailableRect(frame->view()).height());
}

unsigned Screen::availWidth() const
{
    auto* frame = this->frame();
    if (!frame)
        return 0;
    if (RuntimeEnabledFeatures::sharedFeatures().webAPIStatisticsEnabled())
        ResourceLoadObserver::shared().logScreenAPIAccessed(*frame->document(), ResourceLoadStatistics::ScreenAPI::AvailWidth);
    return static_cast<unsigned>(screenAvailableRect(frame->view()).width());
}

} // namespace WebCore
