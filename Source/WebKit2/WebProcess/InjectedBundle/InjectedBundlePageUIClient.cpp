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
#include "InjectedBundlePageUIClient.h"

#include "InjectedBundleHitTestResult.h"
#include "WKAPICast.h"
#include "WebGraphicsContext.h"
#include "WKBundleAPICast.h"
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

void InjectedBundlePageUIClient::willAddMessageToConsole(WebPage* page, const String& message, int32_t lineNumber)
{
    if (m_client.willAddMessageToConsole)
        m_client.willAddMessageToConsole(toAPI(page), toAPI(message.impl()), lineNumber, m_client.clientInfo);
}

void InjectedBundlePageUIClient::willSetStatusbarText(WebPage* page, const String& statusbarText)
{
    if (m_client.willSetStatusbarText)
        m_client.willSetStatusbarText(toAPI(page), toAPI(statusbarText.impl()), m_client.clientInfo);
}

void InjectedBundlePageUIClient::willRunJavaScriptAlert(WebPage* page, const String& alertText, WebFrame* frame)
{
    if (m_client.willRunJavaScriptAlert)
        m_client.willRunJavaScriptAlert(toAPI(page), toAPI(alertText.impl()), toAPI(frame), m_client.clientInfo);
}

void InjectedBundlePageUIClient::willRunJavaScriptConfirm(WebPage* page, const String& message, WebFrame* frame)
{
    if (m_client.willRunJavaScriptConfirm)
        m_client.willRunJavaScriptConfirm(toAPI(page), toAPI(message.impl()), toAPI(frame), m_client.clientInfo);
}

void InjectedBundlePageUIClient::willRunJavaScriptPrompt(WebPage* page, const String& message, const String& defaultValue, WebFrame* frame)
{
    if (m_client.willRunJavaScriptPrompt)
        m_client.willRunJavaScriptPrompt(toAPI(page), toAPI(message.impl()), toAPI(defaultValue.impl()), toAPI(frame), m_client.clientInfo);
}

void InjectedBundlePageUIClient::mouseDidMoveOverElement(WebPage* page, const HitTestResult& coreHitTestResult, WebEvent::Modifiers modifiers, RefPtr<APIObject>& userData)
{
    if (!m_client.mouseDidMoveOverElement)
        return;

    RefPtr<InjectedBundleHitTestResult> hitTestResult = InjectedBundleHitTestResult::create(coreHitTestResult);

    WKTypeRef userDataToPass = 0;
    m_client.mouseDidMoveOverElement(toAPI(page), toAPI(hitTestResult.get()), toAPI(modifiers), &userDataToPass, m_client.clientInfo);
    userData = adoptRef(toImpl(userDataToPass));
}

void InjectedBundlePageUIClient::pageDidScroll(WebPage* page)
{
    if (!m_client.pageDidScroll)
        return;

    m_client.pageDidScroll(toAPI(page), m_client.clientInfo);
}

bool InjectedBundlePageUIClient::shouldPaintCustomOverhangArea()
{
    return m_client.paintCustomOverhangArea;
}

void InjectedBundlePageUIClient::paintCustomOverhangArea(WebPage* page, GraphicsContext* graphicsContext, const IntRect& horizontalOverhangArea, const IntRect& verticalOverhangArea, const IntRect& dirtyRect)
{
    ASSERT(shouldPaintCustomOverhangArea());

    RefPtr<WebGraphicsContext> context = WebGraphicsContext::create(graphicsContext);
    m_client.paintCustomOverhangArea(toAPI(page), toAPI(context.get()), toAPI(horizontalOverhangArea), toAPI(verticalOverhangArea), toAPI(dirtyRect), m_client.clientInfo);
}

String InjectedBundlePageUIClient::shouldGenerateFileForUpload(WebPage* page, const String& originalFilePath)
{
    if (!m_client.shouldGenerateFileForUpload)
        return String();
    RefPtr<WebString> generatedFilePath = adoptRef(toImpl(m_client.shouldGenerateFileForUpload(toAPI(page), toAPI(originalFilePath.impl()), m_client.clientInfo)));
    return generatedFilePath ? generatedFilePath->string() : String();
}

String InjectedBundlePageUIClient::generateFileForUpload(WebPage* page, const String& originalFilePath)
{
    if (!m_client.shouldGenerateFileForUpload)
        return String();
    RefPtr<WebString> generatedFilePath = adoptRef(toImpl(m_client.generateFileForUpload(toAPI(page), toAPI(originalFilePath.impl()), m_client.clientInfo)));
    return generatedFilePath ? generatedFilePath->string() : String();
}

} // namespace WebKit
