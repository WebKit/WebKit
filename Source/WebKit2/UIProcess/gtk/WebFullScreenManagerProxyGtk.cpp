/*
 *  Copyright (C) 2011 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebFullScreenManagerProxy.h"

#if ENABLE(FULLSCREEN_API)

#include "WebContext.h"
#include "WebFullScreenManagerMessages.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebProcess.h"

#include <WebCore/NotImplemented.h>

namespace WebKit {

void WebFullScreenManagerProxy::invalidate()
{
    m_webView = 0;
}

void WebFullScreenManagerProxy::enterFullScreen()
{
    notImplemented();
}

void WebFullScreenManagerProxy::exitFullScreen()
{
    notImplemented();
}

void WebFullScreenManagerProxy::beganEnterFullScreenAnimation()
{
    notImplemented();
}

void WebFullScreenManagerProxy::finishedEnterFullScreenAnimation(bool completed)
{
    notImplemented();
}

void WebFullScreenManagerProxy::beganExitFullScreenAnimation()
{
    notImplemented();
}

void WebFullScreenManagerProxy::finishedExitFullScreenAnimation(bool completed)
{
    notImplemented();
}

void WebFullScreenManagerProxy::enterAcceleratedCompositingMode(const LayerTreeContext& context)
{
    notImplemented();
}

void WebFullScreenManagerProxy::exitAcceleratedCompositingMode()
{
    notImplemented();
}

void WebFullScreenManagerProxy::getFullScreenRect(WebCore::IntRect& rect)
{
    notImplemented();
}

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
