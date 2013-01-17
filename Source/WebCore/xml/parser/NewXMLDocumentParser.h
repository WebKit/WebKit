/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef NewXMLDocumentParser_h
#define NewXMLDocumentParser_h

#include "CachedResourceClient.h"
#include "CachedResourceHandle.h"
#include "CachedScript.h"
#include "FragmentScriptingPermission.h"
#include "ScriptableDocumentParser.h"
#include "XMLToken.h"
#include "XMLTokenizer.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class ContainerNode;
class Document;
class DocumentFragment;
class ScriptElement;
class XMLTreeBuilder;

class NewXMLDocumentParser : public ScriptableDocumentParser, public CachedResourceClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<NewXMLDocumentParser> create(Document* document)
    {
        return adoptRef(new NewXMLDocumentParser(document));
    }

    static PassRefPtr<NewXMLDocumentParser> create(DocumentFragment* fragment, Element* parent, FragmentScriptingPermission scriptingPermission)
    {
        return adoptRef(new NewXMLDocumentParser(fragment, parent, scriptingPermission));
    }

    static bool parseDocumentFragment(const String&, DocumentFragment*, Element* parent = 0, FragmentScriptingPermission = AllowScriptingContent);

    void pauseParsing() { m_parserPaused = true; }
    void resumeParsing();
    void processScript(ScriptElement*);

    virtual TextPosition textPosition() const;
    virtual OrdinalNumber lineNumber() const;

    // DocumentParser
    virtual bool hasInsertionPoint();
    virtual bool isWaitingForScripts() const;
    virtual bool isExecutingScript() const;
    virtual void executeScriptsWaitingForStylesheets();

    // CachedResourceClient
    virtual void notifyFinished(CachedResource*);

protected:
    virtual void insert(const SegmentedString&);
    virtual void append(const SegmentedString&);
    virtual void finish();

private:
    NewXMLDocumentParser(Document*);
    NewXMLDocumentParser(DocumentFragment*, Element* parent, FragmentScriptingPermission);
    virtual ~NewXMLDocumentParser();

    SegmentedString m_input;
    OwnPtr<XMLTokenizer> m_tokenizer;
    XMLToken m_token;

    bool m_parserPaused;
#ifndef NDEBUG
    bool m_finishWasCalled;
#endif

    CachedResourceHandle<CachedScript> m_pendingScript;
    RefPtr<Element> m_scriptElement;

    OwnPtr<XMLTreeBuilder> m_treeBuilder;
};

}

#endif
