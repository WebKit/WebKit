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

#include "config.h"
#include "NewXMLDocumentParser.h"

#include "DocumentFragment.h"
#include "ScriptElement.h"
#include "ScriptSourceCode.h"
#include "SegmentedString.h"
#include "XMLTreeBuilder.h"
#include "XMLTreeViewer.h"

namespace WebCore {

NewXMLDocumentParser::NewXMLDocumentParser(Document* document)
    : ScriptableDocumentParser(document)
    , m_tokenizer(XMLTokenizer::create())
    , m_parserPaused(false)
    , m_finishWasCalled(false)
    , m_pendingScript(0)
    , m_scriptElement(0)
    , m_treeBuilder(XMLTreeBuilder::create(this, document))
{
}

NewXMLDocumentParser::NewXMLDocumentParser(DocumentFragment* fragment, Element* parent, FragmentScriptingPermission)
    : ScriptableDocumentParser(fragment->document())
    , m_tokenizer(XMLTokenizer::create())
    , m_parserPaused(false)
    , m_finishWasCalled(false)
    , m_pendingScript(0)
    , m_scriptElement(0)
    , m_treeBuilder(XMLTreeBuilder::create(this, fragment, parent))
{
}

bool NewXMLDocumentParser::parseDocumentFragment(const String& chunk, DocumentFragment* fragment, Element* contextElement, FragmentScriptingPermission scriptingPermission)
{
    if (!chunk.length())
        return true;

    RefPtr<NewXMLDocumentParser> parser = NewXMLDocumentParser::create(fragment, contextElement, scriptingPermission);
    parser->append(SegmentedString(chunk));
    // Do not call finish(). Current finish() implementation touches the main Document/loader
    // and can cause crashes in the fragment case.
    parser->detach(); // Allows ~DocumentParser to assert it was detached before destruction.

    // FIXME: return false if not well-formed
    return true;
}

NewXMLDocumentParser::~NewXMLDocumentParser()
{
}

void NewXMLDocumentParser::resumeParsing()
{
    m_parserPaused = false;
    append(m_input);
}

void NewXMLDocumentParser::processScript(ScriptElement* scriptElement)
{
    if (scriptElement->prepareScript(TextPosition(), ScriptElement::AllowLegacyTypeInTypeAttribute)) {
        if (scriptElement->readyToBeParserExecuted())
            scriptElement->executeScript(ScriptSourceCode(scriptElement->scriptContent(), document()->url(), TextPosition()));
        else if (scriptElement->willBeParserExecuted()) {
            m_pendingScript = scriptElement->cachedScript();
            m_scriptElement = scriptElement->element();
            m_pendingScript->addClient(this);

            // m_pendingScript will be 0 if script was already loaded and addClient() executed it.
            if (m_pendingScript)
                pauseParsing();
        } else
            m_scriptElement = 0;
    }
}

TextPosition NewXMLDocumentParser::textPosition() const
{
    return TextPosition::minimumPosition();
}

OrdinalNumber NewXMLDocumentParser::lineNumber() const
{
    return OrdinalNumber::first();
}

void NewXMLDocumentParser::insert(const SegmentedString&)
{
    ASSERT_NOT_REACHED();
}

void NewXMLDocumentParser::append(const SegmentedString& string)
{
    m_input = string;
    while (!m_input.isEmpty() && isParsing() && !m_parserPaused) {
        if (!m_tokenizer->nextToken(m_input, m_token))
            continue;

#ifndef NDEBUG
        m_token.print();
#endif

        AtomicXMLToken token(m_token);
        m_treeBuilder->processToken(token);

        if (m_token.type() == XMLTokenTypes::EndOfFile)
            break;

        m_token.clear();
        ASSERT(m_token.isUninitialized());
    }
}

void NewXMLDocumentParser::finish()
{
    ASSERT(!m_finishWasCalled);

    if (m_parserPaused)
        return;

    m_treeBuilder->finish();

    m_finishWasCalled = true;
    if (isParsing()) {
#if ENABLE(XSLT)
        XMLTreeViewer xmlTreeViewer(document());
        if (xmlTreeViewer.hasNoStyleInformation())
            xmlTreeViewer.transformDocumentToTreeView();
#endif // ENABLE(XSLT)

        prepareToStopParsing();
    }
    document()->setReadyState(Document::Interactive);
    document()->finishedParsing();
}

bool NewXMLDocumentParser::hasInsertionPoint()
{
    return false;
}

bool NewXMLDocumentParser::finishWasCalled()
{
    return m_finishWasCalled;
}

bool NewXMLDocumentParser::isWaitingForScripts() const
{
    return false;
}

bool NewXMLDocumentParser::isExecutingScript() const
{
    return false;
}

void NewXMLDocumentParser::executeScriptsWaitingForStylesheets()
{
}

void NewXMLDocumentParser::notifyFinished(CachedResource* unusedResource)
{
    ASSERT_UNUSED(unusedResource, unusedResource == m_pendingScript);
    ASSERT(m_pendingScript->accessCount() > 0);

    ScriptSourceCode sourceCode(m_pendingScript.get());
    bool errorOccurred = m_pendingScript->errorOccurred();
    bool wasCanceled = m_pendingScript->wasCanceled();

    m_pendingScript->removeClient(this);
    m_pendingScript = 0;

    RefPtr<Element> element = m_scriptElement;
    ScriptElement* scriptElement = toScriptElement(m_scriptElement.get());
    m_scriptElement = 0;

    ASSERT(scriptElement);

    // JavaScript can detach this parser, make sure it's kept alive even if detached.
    RefPtr<NewXMLDocumentParser> protect(this);

    if (errorOccurred)
        scriptElement->dispatchErrorEvent();
    else if (!wasCanceled) {
        scriptElement->executeScript(sourceCode);
        scriptElement->dispatchLoadEvent();
    }

    if (!isDetached() && m_parserPaused)
        resumeParsing();
}

}
