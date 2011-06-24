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

#include "CCThread.h"

#include "LayerRendererChromium.h"
#include <wtf/CurrentTime.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/ThreadingPrimitives.h>

namespace WebCore {

using namespace WTF;

CCThread::CCThread()
{
    MutexLocker lock(m_threadCreationMutex);
    m_threadID = createThread(CCThread::compositorThreadStart, this, "Chromium Compositor");
}

CCThread::~CCThread()
{
    m_queue.kill();

    // Stop thread.
    void* exitCode;
    waitForThreadCompletion(m_threadID, &exitCode);
    m_threadID = 0;
}

void CCThread::postTask(PassOwnPtr<Task> task)
{
    m_queue.append(task);
}

void* CCThread::compositorThreadStart(void* userdata)
{
    CCThread* ccThread = static_cast<CCThread*>(userdata);
    return ccThread->runLoop();
}

void* CCThread::runLoop()
{
    {
        // Wait for CCThread::start() to complete to have m_threadID
        // established before starting the main loop.
        MutexLocker lock(m_threadCreationMutex);
    }

    while (OwnPtr<Task> task = m_queue.waitForMessage())
        task->performTask();

    return 0;
}

}
