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

#include "Logging.h"
#include "Page.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "TextEncoding.h"
#include "V8Binding.h"
#include "V8RecursionScope.h"
#include "WebKitMutationObserver.h"
#include "WebMediaPlayerClientImpl.h"
#include "WebSocket.h"
#include "WorkerContextExecutionProxy.h"
#include "platform/WebKitPlatformSupport.h"
#include "platform/WebThread.h"
#include "v8.h"
#include <public/Platform.h>
#include <public/WebPrerenderingSupport.h>
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>
#include <wtf/Threading.h>
#include <wtf/text/AtomicString.h>

#if OS(DARWIN)
#include "WebSystemInterface.h"
#endif

namespace WebKit {

#if ENABLE(MUTATION_OBSERVERS)
namespace {

class EndOfTaskRunner : public WebThread::TaskObserver {
public:
    virtual void didProcessTask()
    {
        WebCore::WebKitMutationObserver::deliverAllMutations();
    }
};

} // namespace

static WebThread::TaskObserver* s_endOfTaskRunner = 0;
#endif // ENABLE(MUTATION_OBSERVERS)

// Make sure we are not re-initialized in the same address space.
// Doing so may cause hard to reproduce crashes.
static bool s_webKitInitialized = false;

static WebKitPlatformSupport* s_webKitPlatformSupport = 0;
static bool s_layoutTestMode = false;

static bool generateEntropy(unsigned char* buffer, size_t length)
{
    if (s_webKitPlatformSupport) {
        s_webKitPlatformSupport->cryptographicallyRandomValues(buffer, length);
        return true;
    }
    return false;
}

#ifndef NDEBUG
static void assertV8RecursionScope()
{
    ASSERT(!isMainThread() || WebCore::V8RecursionScope::properlyUsed());
}
#endif

void initialize(WebKitPlatformSupport* webKitPlatformSupport)
{
    initializeWithoutV8(webKitPlatformSupport);

    v8::V8::SetEntropySource(&generateEntropy);
    v8::V8::Initialize();
    WebCore::V8BindingPerIsolateData::ensureInitialized(v8::Isolate::GetCurrent());

#if ENABLE(MUTATION_OBSERVERS)
    // currentThread will always be non-null in production, but can be null in Chromium unit tests.
    if (WebThread* currentThread = webKitPlatformSupport->currentThread()) {
#ifndef NDEBUG
        v8::V8::AddCallCompletedCallback(&assertV8RecursionScope);
#endif
        ASSERT(!s_endOfTaskRunner);
        s_endOfTaskRunner = new EndOfTaskRunner;
        currentThread->addTaskObserver(s_endOfTaskRunner);
    }
#endif
}

void initializeWithoutV8(WebKitPlatformSupport* webKitPlatformSupport)
{
    ASSERT(!s_webKitInitialized);
    s_webKitInitialized = true;

#if OS(DARWIN)
    InitWebCoreSystemInterface();
#endif

    ASSERT(webKitPlatformSupport);
    ASSERT(!s_webKitPlatformSupport);
    s_webKitPlatformSupport = webKitPlatformSupport;
    Platform::initialize(s_webKitPlatformSupport);

    WTF::initializeThreading();
    WTF::initializeMainThread();
    WTF::AtomicString::init();

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
    // WebKit might have been initialized without V8, so be careful not to invoke
    // V8 specific functions, if V8 was not properly initialized.
#if ENABLE(MUTATION_OBSERVERS)
    if (s_endOfTaskRunner) {
#ifndef NDEBUG
        v8::V8::RemoveCallCompletedCallback(&assertV8RecursionScope);
#endif
        ASSERT(s_webKitPlatformSupport->currentThread());
        s_webKitPlatformSupport->currentThread()->removeTaskObserver(s_endOfTaskRunner);
        delete s_endOfTaskRunner;
        s_endOfTaskRunner = 0;
    }
#endif
    s_webKitPlatformSupport = 0;
    Platform::shutdown();
    WebPrerenderingSupport::shutdown();
}

WebKitPlatformSupport* webKitPlatformSupport()
{
    return s_webKitPlatformSupport;
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
