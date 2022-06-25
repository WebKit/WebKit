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
 
#pragma once

#include "DecodedDataDocumentParser.h"
#include "FragmentScriptingPermission.h"
#include "Timer.h"
#include <wtf/text/TextPosition.h>

namespace WebCore {

class ScriptableDocumentParser : public DecodedDataDocumentParser {
public:
    // Only used by Document::open for deciding if its safe to act on a
    // JavaScript document.open() call right now, or it should be ignored.
    virtual bool isExecutingScript() const { return false; }

    virtual TextPosition textPosition() const = 0;

    virtual bool hasScriptsWaitingForStylesheets() const { return false; }

    void executeScriptsWaitingForStylesheetsSoon();

    // Returns true if the parser didn't yield or pause or synchronously execute a script,
    // so calls to PageConsoleClient should be associated with the parser's text position.
    virtual bool shouldAssociateConsoleMessagesWithTextPosition() const = 0;

    void setWasCreatedByScript(bool wasCreatedByScript) { m_wasCreatedByScript = wasCreatedByScript; }
    bool wasCreatedByScript() const { return m_wasCreatedByScript; }

    ParserContentPolicy parserContentPolicy() { return m_parserContentPolicy; }

protected:
    explicit ScriptableDocumentParser(Document&, ParserContentPolicy = AllowScriptingContent);

    virtual void executeScriptsWaitingForStylesheets() { }

    void detach() override;

private:
    ScriptableDocumentParser* asScriptableDocumentParser() final { return this; }

    void scriptsWaitingForStylesheetsExecutionTimerFired();

    // http://www.whatwg.org/specs/web-apps/current-work/#script-created-parser
    bool m_wasCreatedByScript;
    ParserContentPolicy m_parserContentPolicy;
    Timer m_scriptsWaitingForStylesheetsExecutionTimer;
};

} // namespace WebCore
