/*
* Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Forward.h>
#include <wtf/Threading.h>

namespace WKQCA {

class CVDisplayLinkClient;

// This class is a stand-in for CoreVideo's CVDisplayLinkRef until that class is usable on Windows.
// FIXME 8769181: Replace this with CVDisplayLinkRef
class CVDisplayLink : public ThreadSafeRefCounted<CVDisplayLink> {
public:
    static Ref<CVDisplayLink> create(CVDisplayLinkClient*);
    ~CVDisplayLink();

    void start();
    void stop();
    void setPaused(bool);

private:
    CVDisplayLink(CVDisplayLinkClient*);

    void runIOThread();

    CVDisplayLinkClient* m_client;
    HANDLE m_wakeupEvent;
    RefPtr<Thread> m_ioThread;
    volatile bool m_isRunning { false };
    volatile bool m_isPaused { false };
};

} // namespace WKQCA
