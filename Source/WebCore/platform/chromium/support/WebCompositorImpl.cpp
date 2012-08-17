/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "WebCompositorImpl.h"

#include "CCLayerTreeHost.h"
#include "CCProxy.h"
#include "CCSettings.h"
#include "CCThreadImpl.h"
#include <public/Platform.h>
#include <wtf/ThreadingPrimitives.h>

using namespace WebCore;

namespace WebKit {

bool WebCompositorImpl::s_initialized = false;
CCThread* WebCompositorImpl::s_mainThread = 0;
CCThread* WebCompositorImpl::s_implThread = 0;

void WebCompositor::initialize(WebThread* implThread)
{
    WebCompositorImpl::initialize(implThread);
}

bool WebCompositor::threadingEnabled()
{
    return WebCompositorImpl::threadingEnabled();
}

void WebCompositor::shutdown()
{
    WebCompositorImpl::shutdown();
    CCSettings::reset();
}

void WebCompositor::setPerTilePaintingEnabled(bool enabled)
{
    ASSERT(!WebCompositorImpl::initialized());
    CCSettings::setPerTilePaintingEnabled(enabled);
}

void WebCompositor::setPartialSwapEnabled(bool enabled)
{
    ASSERT(!WebCompositorImpl::initialized());
    CCSettings::setPartialSwapEnabled(enabled);
}

void WebCompositor::setAcceleratedAnimationEnabled(bool enabled)
{
    ASSERT(!WebCompositorImpl::initialized());
    CCSettings::setAcceleratedAnimationEnabled(enabled);
}

void WebCompositorImpl::initialize(WebThread* implThread)
{
    ASSERT(!s_initialized);
    s_initialized = true;

    s_mainThread = CCThreadImpl::create(WebKit::Platform::current()->currentThread()).leakPtr();
    CCProxy::setMainThread(s_mainThread);
    if (implThread) {
        s_implThread = CCThreadImpl::create(implThread).leakPtr();
        CCProxy::setImplThread(s_implThread);
    } else
        CCProxy::setImplThread(0);
}

bool WebCompositorImpl::threadingEnabled()
{
    return s_implThread;
}

bool WebCompositorImpl::initialized()
{
    return s_initialized;
}

void WebCompositorImpl::shutdown()
{
    ASSERT(s_initialized);
    ASSERT(!CCLayerTreeHost::anyLayerTreeHostInstanceExists());

    if (s_implThread) {
        delete s_implThread;
        s_implThread = 0;
    }
    delete s_mainThread;
    s_mainThread = 0;
    CCProxy::setImplThread(0);
    CCProxy::setMainThread(0);
    s_initialized = false;
}

}
