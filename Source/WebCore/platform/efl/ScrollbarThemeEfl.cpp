/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
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
#include "ScrollbarThemeEfl.h"

#include "Frame.h"
#include "FrameView.h"
#include "NotImplemented.h"
#include "Page.h"
#include "RenderThemeEfl.h"
#include "Settings.h"

#include <wtf/NeverDestroyed.h>

namespace WebCore {

ScrollbarTheme& ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeEfl theme;
    return theme;
}

static const int defaultThickness = 10;
static const int scrollbarThumbMin = 2;

typedef HashSet<Scrollbar*> ScrollbarMap;
static ScrollbarMap& scrollbarMap()
{
    static NeverDestroyed<ScrollbarMap> map;
    return map;
}

ScrollbarThemeEfl::~ScrollbarThemeEfl()
{
}

int ScrollbarThemeEfl::scrollbarThickness(ScrollbarControlSize)
{
    return defaultThickness;
}

bool ScrollbarThemeEfl::usesOverlayScrollbars() const
{
    return Settings::usesOverlayScrollbars();
}

bool ScrollbarThemeEfl::hasThumb(Scrollbar& scrollbar)
{
    return thumbLength(scrollbar) > 0;
}

IntRect ScrollbarThemeEfl::backButtonRect(Scrollbar&, ScrollbarPart, bool)
{
    notImplemented();
    return IntRect();
}

IntRect ScrollbarThemeEfl::forwardButtonRect(Scrollbar&, ScrollbarPart, bool)
{
    notImplemented();
    return IntRect();
}

IntRect ScrollbarThemeEfl::trackRect(Scrollbar& scrollbar, bool)
{
    return scrollbar.frameRect();
}

int ScrollbarThemeEfl::minimumThumbLength(Scrollbar&)
{
    return scrollbarThumbMin;
}

void ScrollbarThemeEfl::paintTrackBackground(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect)
{
    loadThemeIfNeeded(scrollbar);
    m_theme->paintThemePart(context, (scrollbar.orientation() == HorizontalScrollbar) ? ScrollbarHorizontalTrackBackground : ScrollbarVerticalTrackBackground, rect);
}

void ScrollbarThemeEfl::paintThumb(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& thumbRect)
{
    loadThemeIfNeeded(scrollbar);
    m_theme->paintThemePart(context, (scrollbar.orientation() == HorizontalScrollbar) ? ScrollbarHorizontalThumb : ScrollbarVerticalThumb, thumbRect);
}

void ScrollbarThemeEfl::registerScrollbar(Scrollbar& scrollbar)
{
    scrollbar.setEnabled(true);
    scrollbarMap().add(&scrollbar);
}

void ScrollbarThemeEfl::unregisterScrollbar(Scrollbar& scrollbar)
{
    scrollbarMap().remove(&scrollbar);
}

void ScrollbarThemeEfl::loadThemeIfNeeded(Scrollbar& scrollbar)
{
    if (m_theme)
        return;

    ScrollView* view = scrollbar.parent();
    if (!view)
        return;

    if (!is<FrameView>(view))
        return;

    Page* page = downcast<FrameView>(view)->frame().page();
    if (!page)
        return;

    m_theme = static_cast<RenderThemeEfl*>(&page->theme());
}

}

