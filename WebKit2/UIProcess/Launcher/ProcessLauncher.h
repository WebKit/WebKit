/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebProcessLauncher_h
#define WebProcessLauncher_h

#include "Connection.h"
#include "PlatformProcessIdentifier.h"
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

#if PLATFORM(QT)
class QLocalSocket;
#endif

namespace WebKit {

class ProcessLauncher : public ThreadSafeShared<ProcessLauncher> {
public:
    class Client {
    public:
        virtual ~Client() { }
        
        virtual void didFinishLaunching(ProcessLauncher*, const CoreIPC::Connection::Identifier&) = 0;
    };
    
    static PassRefPtr<ProcessLauncher> create(Client* client)
    {
        return adoptRef(new ProcessLauncher(client));
    }

    bool isLaunching() const { return m_isLaunching; }
    PlatformProcessIdentifier processIdentifier() const { return m_processIdentifier; }

    void terminateProcess();
    void invalidate();

    static CoreIPC::Connection::Identifier createWebThread();

#if PLATFORM(QT)
    friend class ProcessLauncherHelper;
    static QLocalSocket* takePendingConnection();
#endif

private:
    explicit ProcessLauncher(Client*);

    void launchProcess();
    void didFinishLaunchingProcess(PlatformProcessIdentifier, CoreIPC::Connection::Identifier);

    Client* m_client;

    bool m_isLaunching;
    PlatformProcessIdentifier m_processIdentifier;
};

} // namespace WebKit

#endif // WebProcessLauncher_h
