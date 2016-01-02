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

#include "config.h"
#include "WebPageGroupProxy.h"

#include "InjectedBundle.h"
#include "WebCompiledContentExtension.h"
#include "WebProcess.h"
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/PageGroup.h>
#include <WebCore/UserContentController.h>

namespace WebKit {

PassRefPtr<WebPageGroupProxy> WebPageGroupProxy::create(const WebPageGroupData& data)
{
    RefPtr<WebPageGroupProxy> pageGroup = adoptRef(new WebPageGroupProxy(data));
    
    if (pageGroup->isVisibleToInjectedBundle() && WebProcess::singleton().injectedBundle())
        WebProcess::singleton().injectedBundle()->didInitializePageGroup(pageGroup.get());

    return pageGroup.release();
}

WebPageGroupProxy::WebPageGroupProxy(const WebPageGroupData& data)
    : m_data(data)
    , m_pageGroup(WebCore::PageGroup::pageGroup(m_data.identifier))
{
    for (const auto& userStyleSheet : data.userStyleSheets)
        addUserStyleSheet(userStyleSheet);
    for (const auto& userScript : data.userScripts)
        addUserScript(userScript);

#if ENABLE(CONTENT_EXTENSIONS)
    for (auto& slot : data.userContentExtensions)
        addUserContentExtension(slot.key, slot.value);
#endif
}

WebPageGroupProxy::~WebPageGroupProxy()
{
}

WebCore::UserContentController& WebPageGroupProxy::userContentController()
{
    if (!m_userContentController)
        m_userContentController = WebCore::UserContentController::create();

    return *m_userContentController;
}

void WebPageGroupProxy::addUserStyleSheet(const WebCore::UserStyleSheet& userStyleSheet)
{
    userContentController().addUserStyleSheet(WebCore::mainThreadNormalWorld(), std::make_unique<WebCore::UserStyleSheet>(userStyleSheet), WebCore::InjectInExistingDocuments);
}

void WebPageGroupProxy::addUserScript(const WebCore::UserScript& userScript)
{
    userContentController().addUserScript(WebCore::mainThreadNormalWorld(), std::make_unique<WebCore::UserScript>(userScript));
}

void WebPageGroupProxy::removeAllUserStyleSheets()
{
    userContentController().removeUserStyleSheets(WebCore::mainThreadNormalWorld());
}

void WebPageGroupProxy::removeAllUserScripts()
{
    userContentController().removeUserScripts(WebCore::mainThreadNormalWorld());
}

void WebPageGroupProxy::removeAllUserContent()
{
    userContentController().removeAllUserContent();
}

#if ENABLE(CONTENT_EXTENSIONS)
void WebPageGroupProxy::addUserContentExtension(const String& name, WebCompiledContentExtensionData contentExtensionData)
{
    RefPtr<WebCompiledContentExtension> compiledContentExtension = WebCompiledContentExtension::create(WTFMove(contentExtensionData));
    userContentController().addUserContentExtension(name, compiledContentExtension);
}

void WebPageGroupProxy::removeUserContentExtension(const String& name)
{
    userContentController().removeUserContentExtension(name);
}

void WebPageGroupProxy::removeAllUserContentExtensions()
{
    userContentController().removeAllUserContentExtensions();    
}
#endif

} // namespace WebKit
