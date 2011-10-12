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
#include "cc/CCMainThread.h"

#include "WebKit.h"
#include "WebKitPlatformSupport.h"
#include "WebThread.h"
#include "cc/CCProxy.h"
#include <wtf/MainThread.h>
#include <wtf/OwnPtr.h>

using namespace WTF;

namespace {

bool s_initializedThread = false;
WebKit::WebThread* s_clientThread = 0;

class TaskWrapper : public WebKit::WebThread::Task {
public:
    TaskWrapper(PassOwnPtr<WebCore::CCMainThread::Task> task) : m_task(task) { }
    virtual ~TaskWrapper() { }

    virtual void run()
    {
        m_task->performTask();
    }

private:
    OwnPtr<WebCore::CCMainThread::Task> m_task;
};

} // anonymous namespace

namespace WebCore {

void CCMainThread::initialize()
{
    if (s_initializedThread)
        return;
    ASSERT(WebKit::webKitPlatformSupport());
#ifndef NDEBUG
    WebCore::CCProxy::setMainThread(currentThread());
#endif
    s_clientThread = WebKit::webKitPlatformSupport()->currentThread();
    s_initializedThread = true;
}

void CCMainThread::postTask(PassOwnPtr<Task> task)
{
    ASSERT(s_initializedThread);
    if (s_clientThread)
        s_clientThread->postTask(new TaskWrapper(task));
    else
        WTF::callOnMainThread(performTask, task.leakPtr());
}

} // namespace WebCore
