/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "WebPageInspectorTarget.h"

#include "WebPage.h"
#include "WebPageInspectorTargetFrontendChannel.h"
#include <WebCore/InspectorController.h>
#include <WebCore/Page.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebPageInspectorTarget);

WebPageInspectorTarget::WebPageInspectorTarget(WebPage& page)
    : m_page(page)
{
}

WebPageInspectorTarget::~WebPageInspectorTarget() = default;

String WebPageInspectorTarget::identifier() const
{
    return toTargetID(m_page->identifier());
}

void WebPageInspectorTarget::connect(Inspector::FrontendChannel::ConnectionType connectionType)
{
    if (m_channel)
        return;
    Ref page = m_page.get();
    m_channel = makeUnique<WebPageInspectorTargetFrontendChannel>(page, identifier(), connectionType);
    if (RefPtr corePage = page->corePage())
        corePage->protectedInspectorController()->connectFrontend(*m_channel);
}

void WebPageInspectorTarget::disconnect()
{
    if (!m_channel)
        return;
    if (RefPtr corePage = m_page->corePage())
        corePage->protectedInspectorController()->disconnectFrontend(*m_channel);
    m_channel.reset();
}

void WebPageInspectorTarget::sendMessageToTargetBackend(const String& message)
{
    if (RefPtr corePage = m_page->corePage())
        corePage->protectedInspectorController()->dispatchMessageFromFrontend(message);
}

String WebPageInspectorTarget::toTargetID(WebCore::PageIdentifier pageID)
{
    return makeString("page-"_s, pageID.toUInt64());
}


} // namespace WebKit
