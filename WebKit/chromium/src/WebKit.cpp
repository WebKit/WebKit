/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebKit.h"

#include "AtomicString.h"
#include "DOMTimer.h"
#include "Logging.h"
#include "Page.h"
#include "RuntimeEnabledFeatures.h"
#include "TextEncoding.h"
#include "WebMediaPlayerClientImpl.h"
#include "WebSocket.h"
#include "WorkerContextExecutionProxy.h"

#include <wtf/Assertions.h>
#include <wtf/Threading.h>

namespace WebKit {

static WebKitClient* s_webKitClient = 0;
static bool s_layoutTestMode = false;

void initialize(WebKitClient* webKitClient)
{
    ASSERT(webKitClient);
    ASSERT(!s_webKitClient);
    s_webKitClient = webKitClient;

    WTF::initializeThreading();
    WTF::initializeMainThread();
    WebCore::AtomicString::init();

    // Chromium sets the minimum interval timeout to 4ms, overriding the
    // default of 10ms.  We'd like to go lower, however there are poorly
    // coded websites out there which do create CPU-spinning loops.  Using
    // 4ms prevents the CPU from spinning too busily and provides a balance
    // between CPU spinning and the smallest possible interval timer.
    WebCore::DOMTimer::setMinTimerInterval(0.004);

    // There are some code paths (for example, running WebKit in the browser
    // process and calling into LocalStorage before anything else) where the
    // UTF8 string encoding tables are used on a background thread before
    // they're set up.  This is a problem because their set up routines assert
    // they're running on the main WebKitThread.  It might be possible to make
    // the initialization thread-safe, but given that so many code paths use
    // this, initializing this lazily probably doesn't buy us much.
    WebCore::UTF8Encoding();
}

void shutdown()
{
    s_webKitClient = 0;
}

WebKitClient* webKitClient()
{
    return s_webKitClient;
}

void setLayoutTestMode(bool value)
{
    s_layoutTestMode = value;
}

bool layoutTestMode()
{
    return s_layoutTestMode;
}

void enableLogChannel(const char* name)
{
    WTFLogChannel* channel = WebCore::getChannelFromName(name);
    if (channel)
        channel->state = WTFLogChannelOn;
}

void resetPluginCache(bool reloadPages)
{
    WebCore::Page::refreshPlugins(reloadPages);
}

} // namespace WebKit
