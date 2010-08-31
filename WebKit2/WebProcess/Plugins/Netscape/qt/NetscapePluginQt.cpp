/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 University of Szeged
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

#include "NetscapePlugin.h"

#include "NotImplemented.h"
#include "WebEvent.h"
#include <WebCore/GraphicsContext.h>

using namespace WebCore;

namespace WebKit {

bool NetscapePlugin::platformPostInitialize()
{
    notImplemented();
    return true;
}

void NetscapePlugin::platformPaint(GraphicsContext* context, const IntRect& dirtyRect)
{
    notImplemented();
}

NPEvent toNP(const WebMouseEvent& event)
{
    NPEvent npEvent = NPEvent();

    notImplemented();

    return npEvent;
}

bool NetscapePlugin::platformHandleMouseEvent(const WebMouseEvent& event)
{
    if (m_isWindowed)
        return false;

    NPEvent npEvent = toNP(event);
    NPP_HandleEvent(&npEvent);
    return true;
}

bool NetscapePlugin::platformHandleWheelEvent(const WebWheelEvent&)
{
    notImplemented();
    return false;
}

void NetscapePlugin::platformSetFocus(bool)
{
    notImplemented();
}

bool NetscapePlugin::platformHandleMouseEnterEvent(const WebMouseEvent& event)
{
    if (m_isWindowed)
        return false;

    NPEvent npEvent = toNP(event);
    NPP_HandleEvent(&npEvent);
    return true;
}

bool NetscapePlugin::platformHandleMouseLeaveEvent(const WebMouseEvent& event)
{
    if (m_isWindowed)
        return false;

    NPEvent npEvent = toNP(event);
    NPP_HandleEvent(&npEvent);
    return true;
}

} // namespace WebKit
