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

#ifndef HTML5ScriptRunner_h
#define HTML5ScriptRunner_h

#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class CachedResourceClient;
class CachedScript;
class Document;
class Element;
class Frame;
class HTML5ScriptRunnerHost;
class ScriptSourceCode;

class HTML5ScriptRunner : public Noncopyable {
public:
    HTML5ScriptRunner(Document*, HTML5ScriptRunnerHost*);
    ~HTML5ScriptRunner();

    // Processes the passed in script and any pending scripts if possible.
    bool execute(PassRefPtr<Element> scriptToProcess, int scriptStartLine);
    // Processes any pending scripts.
    bool executeScriptsWaitingForLoad(CachedResource*);
    bool hasScriptsWaitingForStylesheets() const { return m_hasScriptsWaitingForStylesheets; }
    bool executeScriptsWaitingForStylesheets();

    bool inScriptExecution() { return !!m_scriptNestingLevel; }

private:
    struct PendingScript {
        PendingScript()
            : watchingForLoad(false)
            , startingLineNumber(0)
        {
        }

        RefPtr<Element> element;
        CachedResourceHandle<CachedScript> cachedScript;
        bool watchingForLoad; // Did we pass the cachedScript to the HTML5ScriptRunnerHost.
        int startingLineNumber; // Only used for inline script tags.
        // HTML5 has an isReady parameter, however isReady ends up equivalent to
        // m_document->haveStylesheetsLoaded() && cachedScript->isLoaded()
    };

    Frame* frame() const;

    bool haveParsingBlockingScript() const;
    bool executeParsingBlockingScripts();
    void executePendingScript();

    void requestScript(Element*);
    void runScript(Element*, int startingLineNumber);

    // Helpers for dealing with HTML5ScriptRunnerHost
    void watchForLoad(PendingScript&);
    void stopWatchingForLoad(PendingScript&);
    void executeScript(Element*, const ScriptSourceCode&);

    bool isPendingScriptReady(const PendingScript&);
    ScriptSourceCode sourceFromPendingScript(const PendingScript&, bool& errorOccurred);

    Document* m_document;
    HTML5ScriptRunnerHost* m_host;
    PendingScript m_parsingBlockingScript;
    unsigned m_scriptNestingLevel;

    // We only want stylesheet loads to trigger script execution if script
    // execution is currently stopped due to stylesheet loads, otherwise we'd
    // cause nested sript execution when parsing <style> tags since </style>
    // tags can cause Document to call executeScriptsWaitingForStylesheets.
    bool m_hasScriptsWaitingForStylesheets;
};

}

#endif
