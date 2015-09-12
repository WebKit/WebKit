/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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
#include "WebInspectorFrontendAPIDispatcher.h"

#include "WebPage.h"
#include <JavaScriptCore/ScriptValue.h>
#include <WebCore/MainFrame.h>
#include <WebCore/ScriptController.h>
#include <WebCore/ScriptState.h>

namespace WebKit {

WebInspectorFrontendAPIDispatcher::WebInspectorFrontendAPIDispatcher(WebPage& page)
    : m_page(page)
{
}

void WebInspectorFrontendAPIDispatcher::reset()
{
    m_frontendLoaded = false;

    m_queue.clear();
}

void WebInspectorFrontendAPIDispatcher::frontendLoaded()
{
    m_frontendLoaded = true;

    for (const String& expression : m_queue)
        m_page.corePage()->mainFrame().script().executeScript(expression);

    m_queue.clear();
}

void WebInspectorFrontendAPIDispatcher::dispatchCommand(const String& command)
{
    evaluateExpressionOnLoad(makeString("InspectorFrontendAPI.dispatch([\"", command, "\"])"));
}

void WebInspectorFrontendAPIDispatcher::dispatchCommand(const String& command, const String& argument)
{
    evaluateExpressionOnLoad(makeString("InspectorFrontendAPI.dispatch([\"", command, "\", \"", argument, "\"])"));
}

void WebInspectorFrontendAPIDispatcher::dispatchCommand(const String& command, bool argument)
{
    evaluateExpressionOnLoad(makeString("InspectorFrontendAPI.dispatch([\"", command, "\", ", argument ? "true" : "false", "])"));
}

void WebInspectorFrontendAPIDispatcher::dispatchMessageAsync(const String& message)
{
    evaluateExpressionOnLoad(makeString("InspectorFrontendAPI.dispatchMessageAsync(", message, ")"));
}

void WebInspectorFrontendAPIDispatcher::evaluateExpressionOnLoad(const String& expression)
{
    if (m_frontendLoaded) {
        ASSERT(m_queue.isEmpty());
        m_page.corePage()->mainFrame().script().executeScript(expression);
        return;
    }

    m_queue.append(expression);
}

} // namespace WebKit
