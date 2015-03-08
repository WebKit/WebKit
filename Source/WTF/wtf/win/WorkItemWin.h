/*
* Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
* Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#ifndef WorkItemWin_h
#define WorkItemWin_h

#include <Windows.h>
#include <functional>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WTF {

class WorkQueue;

class WorkItemWin : public ThreadSafeRefCounted<WorkItemWin> {
public:
    static RefPtr<WorkItemWin> create(std::function<void()>, WorkQueue*);
    virtual ~WorkItemWin();

    std::function<void()>& function() { return m_function; }
    WorkQueue* queue() const { return m_queue.get(); }

protected:
    WorkItemWin(std::function<void()>, WorkQueue*);

private:
    std::function<void()> m_function;
    RefPtr<WorkQueue> m_queue;
};

class HandleWorkItem : public WorkItemWin {
public:
    static RefPtr<HandleWorkItem> createByAdoptingHandle(HANDLE, const std::function<void()>&, WorkQueue*);
    virtual ~HandleWorkItem();

    void setWaitHandle(HANDLE waitHandle) { m_waitHandle = waitHandle; }
    HANDLE waitHandle() const { return m_waitHandle; }

private:
    HandleWorkItem(HANDLE, const std::function<void()>&, WorkQueue*);

    HANDLE m_handle;
    HANDLE m_waitHandle;
};

}

#endif
