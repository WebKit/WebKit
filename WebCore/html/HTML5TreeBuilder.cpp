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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "HTML5TreeBuilder.h"

#include "DocumentFragment.h"
#include "Element.h"
#include "Frame.h"
#include "HTML5Lexer.h"
#include "HTML5Token.h"
#include "HTMLDocument.h"
#include "LegacyHTMLDocumentParser.h"
#include "HTMLNames.h"
#include "LegacyHTMLTreeConstructor.h"
#include "NotImplemented.h"
#include "ScriptController.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

using namespace HTMLNames;

static const int uninitializedLineNumberValue = -1;

HTML5TreeBuilder::HTML5TreeBuilder(HTML5Lexer* lexer, HTMLDocument* document, bool reportErrors)
    : m_document(document)
    , m_reportErrors(reportErrors)
    , m_isPaused(false)
    , m_insertionMode(Initial)
    , m_lexer(lexer)
    , m_legacyTreeConstructor(new LegacyHTMLTreeConstructor(document, reportErrors))
    , m_lastScriptElementStartLine(uninitializedLineNumberValue)
    , m_scriptToProcessStartLine(uninitializedLineNumberValue)
    , m_fragmentScriptingPermission(FragmentScriptingAllowed)
{
}

// FIXME: Member variables should be grouped into self-initializing structs to
// minimize code duplication between these constructors.
HTML5TreeBuilder::HTML5TreeBuilder(HTML5Lexer* lexer, DocumentFragment* fragment, FragmentScriptingPermission scriptingPermission)
    : m_document(fragment->document())
    , m_reportErrors(false) // FIXME: Why not report errors in fragments?
    , m_isPaused(false)
    , m_insertionMode(Initial)
    , m_lexer(lexer)
    , m_legacyTreeConstructor(new LegacyHTMLTreeConstructor(fragment, scriptingPermission))
    , m_lastScriptElementStartLine(uninitializedLineNumberValue)
    , m_scriptToProcessStartLine(uninitializedLineNumberValue)
    , m_fragmentScriptingPermission(scriptingPermission)
{
}

HTML5TreeBuilder::~HTML5TreeBuilder()
{
}

static void convertToOldStyle(HTML5Token& token, Token& oldStyleToken)
{
    switch (token.type()) {
    case HTML5Token::Uninitialized:
    case HTML5Token::DOCTYPE:
        ASSERT_NOT_REACHED();
        break;
    case HTML5Token::EndOfFile:
        ASSERT_NOT_REACHED();
        notImplemented();
        break;
    case HTML5Token::StartTag:
    case HTML5Token::EndTag: {
        oldStyleToken.beginTag = (token.type() == HTML5Token::StartTag);
        oldStyleToken.selfClosingTag = token.selfClosing();
        oldStyleToken.tagName = AtomicString(token.name().data(), token.name().size());
        const HTML5Token::AttributeList& attributes = token.attributes();
        for (HTML5Token::AttributeList::const_iterator iter = attributes.begin();
             iter != attributes.end(); ++iter) {
            if (!iter->m_name.isEmpty()) {
                String name(iter->m_name.data(), iter->m_name.size());
                String value(iter->m_value.data(), iter->m_value.size());
                RefPtr<Attribute> mappedAttribute = Attribute::createMapped(name, value);
                if (!oldStyleToken.attrs)
                    oldStyleToken.attrs = NamedNodeMap::create();
                oldStyleToken.attrs->insertAttribute(mappedAttribute.release(), false);
            }
        }
        break;
    }
    case HTML5Token::Comment:
        oldStyleToken.tagName = commentAtom;
        oldStyleToken.text = StringImpl::create(token.comment().data(), token.comment().size());
        break;
    case HTML5Token::Character:
        oldStyleToken.tagName = textAtom;
        oldStyleToken.text = StringImpl::create(token.characters().data(), token.characters().size());
        break;
    }
}

void HTML5TreeBuilder::handleScriptStartTag()
{
    notImplemented(); // The HTML frgment case?
    m_lexer->setState(HTML5Lexer::ScriptDataState);
    notImplemented(); // Save insertion mode.
}

void HTML5TreeBuilder::handleScriptEndTag(Element* scriptElement, int scriptStartLine)
{
    ASSERT(!m_scriptToProcess); // Caller never called takeScriptToProcess!
    ASSERT(m_scriptToProcessStartLine == uninitializedLineNumberValue); // Caller never called takeScriptToProcess!
    notImplemented(); // Save insertion mode and insertion point?

    // Pause ourselves so that parsing stops until the script can be processed by the caller.
    m_isPaused = true;
    m_scriptToProcess = scriptElement;
    // Lexer line numbers are 0-based, ScriptSourceCode expects 1-based lines,
    // so we convert here before passing the line number off to HTML5ScriptRunner.
    m_scriptToProcessStartLine = scriptStartLine + 1;
}

PassRefPtr<Element> HTML5TreeBuilder::takeScriptToProcess(int& scriptStartLine)
{
    // Unpause ourselves, callers may pause us again when processing the script.
    // The HTML5 spec is written as though scripts are executed inside the tree
    // builder.  We pause the parser to exit the tree builder, and then resume
    // before running scripts.
    m_isPaused = false;
    scriptStartLine = m_scriptToProcessStartLine;
    m_scriptToProcessStartLine = uninitializedLineNumberValue;
    return m_scriptToProcess.release();
}

HTML5Lexer::State HTML5TreeBuilder::adjustedLexerState(HTML5Lexer::State state, const AtomicString& tagName, Frame* frame)
{
    if (tagName == textareaTag || tagName == titleTag)
        return HTML5Lexer::RCDATAState;

    if (tagName == styleTag || tagName == iframeTag || tagName == xmpTag || tagName == noembedTag
        || tagName == noframesTag || (tagName == noscriptTag && isScriptingFlagEnabled(frame)))
        return HTML5Lexer::RAWTEXTState;

    if (tagName == plaintextTag)
        return HTML5Lexer::PLAINTEXTState;

    return state;
}

PassRefPtr<Node> HTML5TreeBuilder::passTokenToLegacyParser(HTML5Token& token)
{
    if (token.type() == HTML5Token::DOCTYPE) {
        DoctypeToken doctypeToken;
        doctypeToken.m_name.append(token.name().data(), token.name().size());
        doctypeToken.m_publicID = token.publicIdentifier();
        doctypeToken.m_systemID = token.systemIdentifier();
        doctypeToken.m_forceQuirks = token.forceQuirks();

        m_legacyTreeConstructor->parseDoctypeToken(&doctypeToken);
        return 0;
    }

    // For now, we translate into an old-style token for testing.
    Token oldStyleToken;
    convertToOldStyle(token, oldStyleToken);

    RefPtr<Node> result =  m_legacyTreeConstructor->parseToken(&oldStyleToken);
    if (token.type() == HTML5Token::StartTag) {
        // This work is supposed to be done by the parser, but
        // when using the old parser for we have to do this manually.
        if (oldStyleToken.tagName == scriptTag) {
            handleScriptStartTag();
            m_lastScriptElement = static_pointer_cast<Element>(result);
            m_lastScriptElementStartLine = m_lexer->lineNumber();
        } else if (oldStyleToken.tagName == preTag || oldStyleToken.tagName == listingTag)
            m_lexer->skipLeadingNewLineForListing();
        else
            m_lexer->setState(adjustedLexerState(m_lexer->state(), oldStyleToken.tagName, m_document->frame()));
    } else if (token.type() == HTML5Token::EndTag) {
        if (oldStyleToken.tagName == scriptTag) {
            if (m_lastScriptElement) {
                ASSERT(m_lastScriptElementStartLine != uninitializedLineNumberValue);
                if (m_fragmentScriptingPermission == FragmentScriptingNotAllowed) {
                    // FIXME: This is a horrible hack for platform/Pasteboard.
                    // Clear the <script> tag when using the Parser to create
                    // a DocumentFragment for pasting so that javascript content
                    // does not show up in pasted HTML.
                    m_lastScriptElement->removeChildren();
                } else if (insertionMode() != AfterFrameset)
                    handleScriptEndTag(m_lastScriptElement.get(), m_lastScriptElementStartLine);
                m_lastScriptElement = 0;
                m_lastScriptElementStartLine = uninitializedLineNumberValue;
            }
        } else if (oldStyleToken.tagName == framesetTag)
            setInsertionMode(AfterFrameset);
    }
    return result.release();
}

PassRefPtr<Node> HTML5TreeBuilder::constructTreeFromToken(HTML5Token& token)
{
    // Make MSVC ignore our unreachable code for now.
    if (true)
        return passTokenToLegacyParser(token);

    // HTML5 expects the tokenizer to call the parser every time a character is
    // emitted.  We instead collect characters and call the parser with a batch.
    // In order to make our first-pass parser code simple, processToken matches
    // the spec in only handling one character at a time.
    if (token.type() == HTML5Token::Character) {
        HTML5Token::DataVector characters = token.characters();
        HTML5Token::DataVector::const_iterator itr = characters.begin();
        for (;itr; ++itr)
            processToken(token, *itr);
        return 0; // FIXME: Should we be returning the Text node?
    }
    return processToken(token);
}

PassRefPtr<Node> HTML5TreeBuilder::processToken(HTML5Token& token, UChar currentCharacter)
{
    UNUSED_PARAM(token);
    UNUSED_PARAM(currentCharacter);
    // Implementation coming in the next patch.
    return 0;
}

void HTML5TreeBuilder::finished()
{
    // We should call m_document->finishedParsing() here, except
    // m_legacyTreeConstructor->finished() does it for us.
    m_legacyTreeConstructor->finished();
}

bool HTML5TreeBuilder::isScriptingFlagEnabled(Frame* frame)
{
    if (!frame)
        return false;
    if (ScriptController* scriptController = frame->script())
        return scriptController->canExecuteScripts(NotAboutToExecuteScript);
    return false;
}

}
