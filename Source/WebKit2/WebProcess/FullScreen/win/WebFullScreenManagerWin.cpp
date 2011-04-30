/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WebFullScreenManagerWin.h"

#if ENABLE(FULLSCREEN_API)

#include "MessageID.h"
#include "WebFullScreenManagerProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebKitSystemInterface/WebKitSystemInterface.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebFullScreenManager> WebFullScreenManager::create(WebPage* page)
{
    return WebFullScreenManagerWin::create(page);
}

PassRefPtr<WebFullScreenManagerWin> WebFullScreenManagerWin::create(WebPage* page)
{
    return adoptRef(new WebFullScreenManagerWin(page));
}

WebFullScreenManagerWin::WebFullScreenManagerWin(WebPage* page)
    : WebFullScreenManager(page)
{
}

WebFullScreenManagerWin::~WebFullScreenManagerWin()
{
}

void WebFullScreenManagerWin::setRootFullScreenLayer(WebCore::GraphicsLayer*)
{
    // FIXME: Implement
}

void WebFullScreenManagerWin::beginEnterFullScreenAnimation(float)
{
    // FIXME: Implement
}

void WebFullScreenManagerWin::beginExitFullScreenAnimation(float)
{
    // FIXME: Implement
}

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
