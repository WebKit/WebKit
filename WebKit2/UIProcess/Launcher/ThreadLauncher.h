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

#ifndef ThreadLauncher_h
#define ThreadLauncher_h

#include "Connection.h"
#include "PlatformProcessIdentifier.h"
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

#if PLATFORM(QT)
class QLocalSocket;
#endif

namespace WebKit {

class ThreadLauncher : public ThreadSafeShared<ThreadLauncher> {
public:
    class Client {
    public:
        virtual ~Client() { }
        virtual void didFinishLaunching(ThreadLauncher*, CoreIPC::Connection::Identifier) = 0;
    };
    
    static PassRefPtr<ThreadLauncher> create(Client* client)
    {
        return adoptRef(new ThreadLauncher(client));
    }

    bool isLaunching() const { return m_isLaunching; }

    void invalidate();

private:
    explicit ThreadLauncher(Client*);

    void launchThread();
    void didFinishLaunchingThread(CoreIPC::Connection::Identifier);

    static CoreIPC::Connection::Identifier createWebThread();

    bool m_isLaunching;
    Client* m_client;
};

} // namespace WebKit

#endif // ThreadLauncher_h
