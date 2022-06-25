/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
#include "ScriptableDocumentParser.h"

#include "Document.h"
#include "Settings.h"
#include "StyleScope.h"

namespace WebCore {

ScriptableDocumentParser::ScriptableDocumentParser(Document& document, ParserContentPolicy parserContentPolicy)
    : DecodedDataDocumentParser(document)
    , m_wasCreatedByScript(false)
    , m_parserContentPolicy(parserContentPolicy)
    , m_scriptsWaitingForStylesheetsExecutionTimer(*this, &ScriptableDocumentParser::scriptsWaitingForStylesheetsExecutionTimerFired)
{
    if (!pluginContentIsAllowed(m_parserContentPolicy))
        m_parserContentPolicy = allowPluginContent(m_parserContentPolicy);

    if (scriptingContentIsAllowed(m_parserContentPolicy) && !document.allowsContentJavaScript())
        m_parserContentPolicy = disallowScriptingContent(m_parserContentPolicy);
}

void ScriptableDocumentParser::executeScriptsWaitingForStylesheetsSoon()
{
    ASSERT(!document()->styleScope().hasPendingSheets());

    if (m_scriptsWaitingForStylesheetsExecutionTimer.isActive())
        return;
    if (!hasScriptsWaitingForStylesheets())
        return;

    m_scriptsWaitingForStylesheetsExecutionTimer.startOneShot(0_s);
}

void ScriptableDocumentParser::scriptsWaitingForStylesheetsExecutionTimerFired()
{
    ASSERT(!isDetached());

    Ref<ScriptableDocumentParser> protectedThis(*this);

    if (!document()->styleScope().hasPendingSheets())
        executeScriptsWaitingForStylesheets();

    if (!isDetached())
        document()->checkCompleted();
}

void ScriptableDocumentParser::detach()
{
    m_scriptsWaitingForStylesheetsExecutionTimer.stop();

    DecodedDataDocumentParser::detach();
}

};
