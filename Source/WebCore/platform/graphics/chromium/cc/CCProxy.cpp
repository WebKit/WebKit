/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "cc/CCProxy.h"

#include "TraceEvent.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCThreadTask.h"
#include <wtf/MainThread.h>

using namespace WTF;

namespace WebCore {

namespace {
#ifndef NDEBUG
bool implThreadIsOverridden = false;
ThreadIdentifier threadIDOverridenToBeImplThread;
#endif
CCThread* s_mainThread = 0;
CCThread* s_implThread = 0;
}

void CCProxy::setMainThread(CCThread* thread)
{
    s_mainThread = thread;
}

CCThread* CCProxy::mainThread()
{
    return s_mainThread;
}

bool CCProxy::hasImplThread()
{
    return s_implThread;
}

void CCProxy::setImplThread(CCThread* thread)
{
    s_implThread = thread;
}

CCThread* CCProxy::implThread()
{
    return s_implThread;
}

CCThread* CCProxy::currentThread()
{
    ThreadIdentifier currentThreadIdentifier = WTF::currentThread();
    if (s_mainThread && s_mainThread->threadID() == currentThreadIdentifier)
        return s_mainThread;
    if (s_implThread && s_implThread->threadID() == currentThreadIdentifier)
        return s_implThread;
    return 0;
}

#ifndef NDEBUG
bool CCProxy::isMainThread()
{
    ASSERT(s_mainThread);
    if (implThreadIsOverridden && WTF::currentThread() == threadIDOverridenToBeImplThread)
        return false;
    return WTF::currentThread() == s_mainThread->threadID();
}

bool CCProxy::isImplThread()
{
    WTF::ThreadIdentifier implThreadID = s_implThread ? s_implThread->threadID() : 0;
    if (implThreadIsOverridden && WTF::currentThread() == threadIDOverridenToBeImplThread)
        return true;
    return WTF::currentThread() == implThreadID;
}

void CCProxy::setCurrentThreadIsImplThread(bool isImplThread)
{
    implThreadIsOverridden = isImplThread;
    if (isImplThread)
        threadIDOverridenToBeImplThread = WTF::currentThread();
}
#endif

CCProxy::CCProxy()
{
    ASSERT(isMainThread());
}

CCProxy::~CCProxy()
{
    ASSERT(isMainThread());
}

}
