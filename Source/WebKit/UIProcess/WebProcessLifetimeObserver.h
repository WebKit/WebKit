/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef WebProcessLifetimeObserver_h
#define WebProcessLifetimeObserver_h

#include <wtf/HashCountedSet.h>
#include <wtf/IteratorRange.h>

namespace IPC {
class Connection;
}

namespace WebKit {

class WebPageProxy;
class WebProcessProxy;

class WebProcessLifetimeObserver {
public:
    WebProcessLifetimeObserver();
    virtual ~WebProcessLifetimeObserver();

    void addWebPage(WebPageProxy&, WebProcessProxy&);
    void removeWebPage(WebPageProxy&, WebProcessProxy&);

    WTF::IteratorRange<HashCountedSet<WebProcessProxy*>::const_iterator::Keys> processes() const;

private:
    friend class WebProcessLifetimeTracker;

    virtual void webPageWasAdded(WebPageProxy&) { }
    virtual void webProcessWillOpenConnection(WebProcessProxy&, IPC::Connection&) { }
    virtual void webPageWillOpenConnection(WebPageProxy&, IPC::Connection&) { }
    virtual void webPageDidCloseConnection(WebPageProxy&, IPC::Connection&) { }
    virtual void webProcessDidCloseConnection(WebProcessProxy&, IPC::Connection&) { }
    virtual void webPageWasInvalidated(WebPageProxy&) { }

    HashCountedSet<WebProcessProxy*> m_processes;
};

}

#endif // WebProcessLifetimeObserver_h
