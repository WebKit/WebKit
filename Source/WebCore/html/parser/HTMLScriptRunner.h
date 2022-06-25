/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#include "PendingScript.h"
#include <wtf/Deque.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/TextPosition.h>

namespace WebCore {

class Document;
class Frame;
class HTMLScriptRunnerHost;
class ScriptSourceCode;

class HTMLScriptRunner {
    WTF_MAKE_FAST_ALLOCATED;
public:
    HTMLScriptRunner(Document&, HTMLScriptRunnerHost&);
    ~HTMLScriptRunner();

    void detach();

    // Processes the passed in script and any pending scripts if possible.
    void execute(Ref<ScriptElement>&&, const TextPosition& scriptStartPosition);

    void executeScriptsWaitingForLoad(PendingScript&);
    bool hasScriptsWaitingForStylesheets() const { return m_hasScriptsWaitingForStylesheets; }
    void executeScriptsWaitingForStylesheets();
    bool executeScriptsWaitingForParsing();

    bool hasParserBlockingScript() const;
    bool isExecutingScript() const { return !!m_scriptNestingLevel; }

private:
    Frame* frame() const;

    void executePendingScriptAndDispatchEvent(PendingScript&);
    void executeParsingBlockingScripts();

    void requestParsingBlockingScript(ScriptElement&);
    void requestDeferredScript(ScriptElement&);

    void runScript(ScriptElement&, const TextPosition& scriptStartPosition);

    void watchForLoad(PendingScript&);
    void stopWatchingForLoad(PendingScript&);
    bool isPendingScriptReady(const PendingScript&);

    WeakPtr<Document> m_document;
    HTMLScriptRunnerHost& m_host;
    RefPtr<PendingScript> m_parserBlockingScript;
    Deque<Ref<PendingScript>> m_scriptsToExecuteAfterParsing; // http://www.whatwg.org/specs/web-apps/current-work/#list-of-scripts-that-will-execute-when-the-document-has-finished-parsing
    unsigned m_scriptNestingLevel;

    // We only want stylesheet loads to trigger script execution if script
    // execution is currently stopped due to stylesheet loads, otherwise we'd
    // cause nested script execution when parsing <style> tags since </style>
    // tags can cause Document to call executeScriptsWaitingForStylesheets.
    bool m_hasScriptsWaitingForStylesheets;
};

} // namespace WebCore
