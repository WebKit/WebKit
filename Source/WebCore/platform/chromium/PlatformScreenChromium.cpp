/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformScreen.h"

#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "HostWindow.h"
#include "Page.h"
#include "ScrollView.h"
#include "Settings.h"
#include "Widget.h"
#include <public/Platform.h>
#include <public/WebScreenInfo.h>

namespace WebCore {

static PlatformPageClient toPlatformPageClient(Widget* widget)
{
    if (!widget)
        return 0;
    ScrollView* root = widget->root();
    if (!root)
        return 0;
    HostWindow* hostWindow = root->hostWindow();
    if (!hostWindow)
        return 0;
    return hostWindow->platformPageClient();
}

int screenDepth(Widget* widget)
{
    PlatformPageClient client = toPlatformPageClient(widget);
    if (!client)
        return 0;
    return client->screenInfo().depth;
}

int screenDepthPerComponent(Widget* widget)
{
    PlatformPageClient client = toPlatformPageClient(widget);
    if (!client)
        return 0;
    return client->screenInfo().depthPerComponent;
}

bool screenIsMonochrome(Widget* widget)
{
    PlatformPageClient client = toPlatformPageClient(widget);
    if (!client)
        return false;
    return client->screenInfo().isMonochrome;
}

// On Chrome for Android, the screenInfo rects are in physical screen pixels
// instead of density independent (UI) pixels, and must be scaled down.
static FloatRect toUserSpace(FloatRect rect, Widget* widget)
{
    if (widget->isFrameView()) {
        Page* page = static_cast<FrameView*>(widget)->frame()->page();
        if (page && !page->settings()->applyDeviceScaleFactorInCompositor())
            rect.scale(1 / page->deviceScaleFactor());
    }
    return rect;
}

FloatRect screenRect(Widget* widget)
{
    PlatformPageClient client = toPlatformPageClient(widget);
    if (!client)
        return FloatRect();
    return toUserSpace(IntRect(client->screenInfo().rect), widget);
}

FloatRect screenAvailableRect(Widget* widget)
{
    PlatformPageClient client = toPlatformPageClient(widget);
    if (!client)
        return FloatRect();
    return toUserSpace(IntRect(client->screenInfo().availableRect), widget);
}

void screenColorProfile(ColorProfile& toProfile)
{
    WebKit::WebVector<char> profile;
    WebKit::Platform::current()->screenColorProfile(&profile);
    toProfile.append(profile.data(), profile.size());
}

} // namespace WebCore
