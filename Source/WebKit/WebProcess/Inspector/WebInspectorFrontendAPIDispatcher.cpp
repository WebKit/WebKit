/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
#include <WebCore/Frame.h>
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
    m_suspended = false;
    m_queue.clear();
}

void WebInspectorFrontendAPIDispatcher::frontendLoaded()
{
    m_frontendLoaded = true;

    evaluateQueuedExpressions();
}

void WebInspectorFrontendAPIDispatcher::suspend()
{
    ASSERT(m_frontendLoaded);
    ASSERT(!m_suspended);
    ASSERT(m_queue.isEmpty());

    m_suspended = true;
}

void WebInspectorFrontendAPIDispatcher::unsuspend()
{
    ASSERT(m_suspended);

    m_suspended = false;

    evaluateQueuedExpressions();
}

void WebInspectorFrontendAPIDispatcher::dispatchCommand(const String& command)
{
    evaluateOrQueueExpression(makeString("InspectorFrontendAPI.dispatch([\"", command, "\"])"));
}

void WebInspectorFrontendAPIDispatcher::dispatchCommand(const String& command, const String& argument)
{
    evaluateOrQueueExpression(makeString("InspectorFrontendAPI.dispatch([\"", command, "\", \"", argument, "\"])"));
}

void WebInspectorFrontendAPIDispatcher::dispatchCommand(const String& command, bool argument)
{
    evaluateOrQueueExpression(makeString("InspectorFrontendAPI.dispatch([\"", command, "\", ", argument ? "true" : "false", "])"));
}

void WebInspectorFrontendAPIDispatcher::dispatchMessageAsync(const String& message)
{
    evaluateOrQueueExpression(makeString("InspectorFrontendAPI.dispatchMessageAsync(", message, ")"));
}

void WebInspectorFrontendAPIDispatcher::evaluateOrQueueExpression(const String& expression)
{
    if (!m_frontendLoaded || m_suspended) {
        m_queue.append(expression);
        return;
    }

    ASSERT(m_queue.isEmpty());
    ASSERT(!m_page.corePage()->mainFrame().script().isPaused());
    m_page.corePage()->mainFrame().script().executeScriptIgnoringException(expression);
}

void WebInspectorFrontendAPIDispatcher::evaluateQueuedExpressions()
{
    if (m_queue.isEmpty())
        return;

    for (const String& expression : m_queue) {
        ASSERT(!m_page.corePage()->mainFrame().script().isPaused());
        m_page.corePage()->mainFrame().script().executeScriptIgnoringException(expression);
    }

    m_queue.clear();
}

void WebInspectorFrontendAPIDispatcher::evaluateExpressionForTesting(const String& expression)
{
    evaluateOrQueueExpression(expression);
}

} // namespace WebKit
