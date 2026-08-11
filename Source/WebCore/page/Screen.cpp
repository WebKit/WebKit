/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
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

#include "DocumentLoader.h"
#include "DocumentPage.h"
#include "DocumentQuirks.h"
#include "DocumentView.h"
#include "FloatRect.h"
#include "LocalDOMWindow.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "PlatformScreen.h"
#include "ResourceLoadObserver.h"
#include "ScreenOrientation.h"
#include "ScriptWrappableInlines.h"
#include "Settings.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Screen);

Screen::Screen(LocalDOMWindow& window)
    : LocalDOMWindowProperty(&window)
{
}

Screen::~Screen() = default;

static bool shouldApplyScreenFingerprintingProtections(const LocalFrame& frame)
{
    RefPtr page = frame.page();
    if (!page)
        return false;

    RefPtr document = frame.document();
    if (!document)
        return false;

    return page->shouldApplyScreenFingerprintingProtections(*document);
}

static bool shouldFlipScreenDimensions(const LocalFrame& frame)
{
    RefPtr document = frame.document();
    return document && document->quirks().shouldFlipScreenDimensions();
}

int Screen::height() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;
    if (frame->settings().webAPIStatisticsEnabled())
        ResourceLoadObserver::singleton().logScreenAPIAccessed(*protect(frame->document()), ScreenAPIsAccessed::Height);

    if (shouldFlipScreenDimensions(*frame))
        return static_cast<int>(frame->screenSize().width());

    return static_cast<int>(frame->screenSize().height());
}

int Screen::width() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;
    if (frame->settings().webAPIStatisticsEnabled())
        ResourceLoadObserver::singleton().logScreenAPIAccessed(*protect(frame->document()), ScreenAPIsAccessed::Width);

    if (shouldFlipScreenDimensions(*frame))
        return static_cast<int>(frame->screenSize().height());

    return static_cast<int>(frame->screenSize().width());
}

unsigned Screen::colorDepth() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 24;
    if (frame->settings().webAPIStatisticsEnabled())
        ResourceLoadObserver::singleton().logScreenAPIAccessed(*protect(frame->document()), ScreenAPIsAccessed::ColorDepth);
    return static_cast<unsigned>(screenDepth(protect(frame->view()).get()));
}

int Screen::availLeft() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    if (frame->settings().webAPIStatisticsEnabled())
        ResourceLoadObserver::singleton().logScreenAPIAccessed(*protect(frame->document()), ScreenAPIsAccessed::AvailLeft);

    if (shouldApplyScreenFingerprintingProtections(*frame))
        return 0;

    return static_cast<int>(screenAvailableRect(protect(frame->view()).get()).x());
}

int Screen::availTop() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    if (frame->settings().webAPIStatisticsEnabled())
        ResourceLoadObserver::singleton().logScreenAPIAccessed(*protect(frame->document()), ScreenAPIsAccessed::AvailTop);

    if (shouldApplyScreenFingerprintingProtections(*frame))
        return 0;

    return static_cast<int>(screenAvailableRect(protect(frame->view()).get()).y());
}

int Screen::availHeight() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    if (frame->settings().webAPIStatisticsEnabled())
        ResourceLoadObserver::singleton().logScreenAPIAccessed(*protect(frame->document()), ScreenAPIsAccessed::AvailHeight);

    if (shouldApplyScreenFingerprintingProtections(*frame))
        return static_cast<int>(frame->screenSize().height());

    return static_cast<int>(screenAvailableRect(protect(frame->view()).get()).height());
}

int Screen::availWidth() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;

    if (frame->settings().webAPIStatisticsEnabled())
        ResourceLoadObserver::singleton().logScreenAPIAccessed(*protect(frame->document()), ScreenAPIsAccessed::AvailWidth);

    if (shouldApplyScreenFingerprintingProtections(*frame))
        return static_cast<int>(frame->screenSize().width());

    return static_cast<int>(screenAvailableRect(protect(frame->view()).get()).width());
}

ScreenOrientation& Screen::orientation()
{
    if (!m_screenOrientation)
        m_screenOrientation = ScreenOrientation::create(window() ? protect(window()->document()).get() : nullptr);
    return *m_screenOrientation;
}

} // namespace WebCore
