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

#include "WebPageNamespace.h"

#include "WebProcessManager.h"
#include "WebProcessProxy.h"

#include "WKContextPrivate.h"

#ifndef NDEBUG
#include <wtf/RefCountedLeakCounter.h>
#endif

namespace WebKit {

#ifndef NDEBUG
static WTF::RefCountedLeakCounter webPageNamespaceCounter("WebPageNamespace");
#endif

WebPageNamespace::WebPageNamespace(WebContext* context)
    : m_context(context)
{
#ifndef NDEBUG
    webPageNamespaceCounter.increment();
#endif
}

WebPageNamespace::~WebPageNamespace()
{
    ASSERT(m_context);
    m_context->pageNamespaceWasDestroyed(this);

#ifndef NDEBUG
    webPageNamespaceCounter.decrement();
#endif
}

WebPageProxy* WebPageNamespace::createWebPage()
{
    ensureWebProcess();

    return m_process->createWebPage(this);
}

void WebPageNamespace::ensureWebProcess()
{
    if (m_process && m_process->isValid())
        return;

    m_process = WebProcessManager::shared().getWebProcess(m_context->processModel());
}

void WebPageNamespace::reviveIfNecessary()
{
    if (m_process->isValid())
        return;

    // FIXME: The WebContext should hand us the new ProcessProxy based on its process model.
    m_process = WebProcessManager::shared().getWebProcess(m_context->processModel());
}

void WebPageNamespace::preferencesDidChange()
{
    for (WebProcessProxy::pages_const_iterator it = m_process->pages_begin(), end = m_process->pages_end(); it != end; ++it)
        (*it)->preferencesDidChange();
}

void WebPageNamespace::getStatistics(WKContextStatistics* statistics)
{
    if (!m_process)
        return;

    statistics->numberOfWKPages += m_process->numberOfPages();

    for (WebProcessProxy::pages_const_iterator it = m_process->pages_begin(), end = m_process->pages_end(); it != end; ++it)
        (*it)->getStatistics(statistics);
}

} // namespace WebKit
