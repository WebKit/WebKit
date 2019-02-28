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

#include "config.h"
#include "WebProcessLifetimeObserver.h"

#include "WebPageProxy.h"
#include "WebProcessProxy.h"

namespace WebKit {

WebProcessLifetimeObserver::WebProcessLifetimeObserver()
{
}

WebProcessLifetimeObserver::~WebProcessLifetimeObserver()
{
}

void WebProcessLifetimeObserver::addWebPage(WebPageProxy& webPageProxy, WebProcessProxy& process)
{
    ASSERT(process.state() == WebProcessProxy::State::Running);

    if (m_processes.add(&process).isNewEntry)
        webProcessWillOpenConnection(process, *process.connection());

    webPageWillOpenConnection(webPageProxy, *process.connection());
}

void WebProcessLifetimeObserver::removeWebPage(WebPageProxy& webPageProxy, WebProcessProxy& process)
{
    // FIXME: This should assert that the page is either closed or that the process is no longer running,
    // but we have to make sure that removeWebPage is called after the connection has been removed in that case.
    ASSERT(m_processes.contains(&process));

    webPageDidCloseConnection(webPageProxy, *process.connection());

    if (m_processes.remove(&process))
        webProcessDidCloseConnection(process, *process.connection());
}

WTF::IteratorRange<HashCountedSet<WebProcessProxy*>::const_iterator::Keys> WebProcessLifetimeObserver::processes() const
{
    ASSERT(std::all_of(m_processes.begin().keys(), m_processes.end().keys(), [](WebProcessProxy* process) {
        return process->state() == WebProcessProxy::State::Running;
    }));

    return makeIteratorRange(m_processes.begin().keys(), m_processes.end().keys());
}

}
