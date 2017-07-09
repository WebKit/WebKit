/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * All rights reserved.
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

#ifndef ResourceHandleManager_h
#define ResourceHandleManager_h

#include "CurlContext.h"
#include "Frame.h"
#include "Timer.h"
#include "ResourceHandleClient.h"

#if PLATFORM(WIN)
#include <winsock2.h>
#include <windows.h>
#endif

#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ResourceHandleManager {
public:
    static ResourceHandleManager* sharedInstance();
    void add(ResourceHandle*);
    void cancel(ResourceHandle*);

private:
    ResourceHandleManager();
    ~ResourceHandleManager();
    void downloadTimerCallback();
    void removeFromCurl(ResourceHandle*);
    bool removeScheduledJob(ResourceHandle*);
    void startJob(ResourceHandle*);
    bool startScheduledJobs();

    Timer m_downloadTimer;
    Vector<ResourceHandle*> m_resourceHandleList;
    int m_runningJobs;

    CurlMultiHandle m_curlMultiHandle;
};

}

#endif
