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
#include "WebInspectorClient.h"

#if ENABLE(INSPECTOR)

#include "WebInspectorFrontendClient.h"
#include "WebInspector.h"
#include "WebPage.h"
#include <WebCore/InspectorController.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>

using namespace WebCore;

namespace WebKit {

void WebInspectorClient::inspectorDestroyed()
{
    delete this;
}

void WebInspectorClient::openInspectorFrontend(InspectorController*)
{
    WebPage* inspectorPage = m_page->inspector()->createInspectorPage();
    ASSERT(inspectorPage);
    if (!inspectorPage)
        return;

    inspectorPage->corePage()->inspectorController()->setInspectorFrontendClient(adoptPtr(new WebInspectorFrontendClient(m_page, inspectorPage)));
}

void WebInspectorClient::highlight(Node*)
{
    notImplemented();
}

void WebInspectorClient::hideHighlight()
{
    notImplemented();
}

void WebInspectorClient::populateSetting(const String& key, String*)
{
    notImplemented();
}

void WebInspectorClient::storeSetting(const String&, const String&)
{
    notImplemented();
}

bool WebInspectorClient::sendMessageToFrontend(const String& message)
{
    WebInspector* inspector = m_page->inspector();
    if (!inspector)
        return false;
    WebPage* inspectorPage = inspector->inspectorPage();
    if (!inspectorPage)
        return false;
    return doDispatchMessageOnFrontendPage(inspectorPage->corePage(), message);
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR)
