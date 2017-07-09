/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "CurlContext.h"

#include <wtf/Lock.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

#if PLATFORM(WIN)
#include <windows.h>
#include <winsock2.h>
#endif


namespace WebCore {

enum class CurlJobAction { None, Finished };

class CurlJob {
public:
    virtual CurlJobAction handleCurlMsg(CURLMsg*) = 0;
};

class CurlJobManager {
public:
    static CurlJobManager& singleton()
    {
        // Since it's a static variable, if the class has already been created,
        // It won't be created again.
        // And it **is** thread-safe in C++11.

        static CurlJobManager shared;
        return shared;
    }

    CurlJobManager();
    virtual ~CurlJobManager();

    bool add(CURL* curlHandle);
    bool remove(CURL* curlHandle);

    int getActiveCount() const;
    int getPendingCount() const;

private:
    void startThreadIfNeeded();
    void stopThread();
    void stopThreadIfIdle();

    void updateHandleList();

    bool runThread() const { LockHolder locker(m_mutex); return m_runThread; }
    void setRunThread(bool runThread) { LockHolder locker(m_mutex); m_runThread = runThread; }

    bool addToCurl(CURL* curlHandle);
    bool removeFromCurl(CURL* curlHandle);

    void workerThread();

    RefPtr<Thread> m_thread;
    Vector<CURL*> m_pendingHandleList;
    Vector<CURL*> m_activeHandleList;
    Vector<CURL*> m_removedHandleList;
    mutable Lock m_mutex;
    bool m_runThread { false };

    CurlMultiHandle m_curlMultiHandle;

    friend class CurlJob;
};

}
