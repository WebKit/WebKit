/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PageInspectorTargetProxy.h"

#include "InspectorTargetProxy.h"
#include "MessageSenderInlines.h"
#include "PageInspectorTarget.h"
#include "ProvisionalPageProxy.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <JavaScriptCore/InspectorTarget.h>
#include <memory>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PageInspectorTargetProxy);

std::unique_ptr<PageInspectorTargetProxy> PageInspectorTargetProxy::create(WebPageProxy& page, const String& targetId, Inspector::InspectorTargetType type)
{
    return makeUnique<PageInspectorTargetProxy>(page, targetId, type);
}

std::unique_ptr<PageInspectorTargetProxy> PageInspectorTargetProxy::create(ProvisionalPageProxy& provisionalPage, const String& targetId, Inspector::InspectorTargetType type)
{
    RefPtr page = provisionalPage.page();
    if (!page)
        return nullptr;

    std::unique_ptr target = PageInspectorTargetProxy::create(*page, targetId, type);
    target->m_provisionalPage = provisionalPage;
    return target;
}

PageInspectorTargetProxy::PageInspectorTargetProxy(WebPageProxy& page, const String& targetId, Inspector::InspectorTargetType type)
    : InspectorTargetProxy(targetId, type)
    , m_page(page)
{
}

void PageInspectorTargetProxy::connect(Inspector::FrontendChannel::ConnectionType connectionType)
{
    if (RefPtr provisionalPage = m_provisionalPage.get()) {
        provisionalPage->send(Messages::WebPage::ConnectInspector(connectionType));
        return;
    }

    Ref page = m_page.get();
    if (page->hasRunningProcess())
        protect(page->legacyMainFrameProcess())->send(Messages::WebPage::ConnectInspector(connectionType), page->webPageIDInMainFrameProcess());
}

void PageInspectorTargetProxy::disconnect()
{
    if (isPaused())
        resume();

    if (RefPtr provisionalPage = m_provisionalPage.get()) {
        provisionalPage->send(Messages::WebPage::DisconnectInspector());
        return;
    }

    Ref page = m_page.get();
    if (page->hasRunningProcess())
        protect(page->legacyMainFrameProcess())->send(Messages::WebPage::DisconnectInspector(), page->webPageIDInMainFrameProcess());
}

void PageInspectorTargetProxy::sendMessageToTargetBackend(const String& message)
{
    if (RefPtr provisionalPage = m_provisionalPage.get()) {
        provisionalPage->send(Messages::WebPage::SendMessageToTargetBackend(message));
        return;
    }

    Ref page = m_page.get();
    if (page->hasRunningProcess())
        protect(page->legacyMainFrameProcess())->send(Messages::WebPage::SendMessageToTargetBackend(message), page->webPageIDInMainFrameProcess());
}

void PageInspectorTargetProxy::didCommitProvisionalTarget()
{
    m_provisionalPage = nullptr;
}

bool PageInspectorTargetProxy::isProvisional() const
{
    return !!m_provisionalPage;
}

} // namespace WebKit
