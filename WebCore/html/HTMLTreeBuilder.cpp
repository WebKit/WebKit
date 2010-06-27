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
#include "HTMLTreeBuilder.h"

#include "Comment.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLElementFactory.h"
#include "HTMLScriptElement.h"
#include "HTMLTokenizer.h"
#include "HTMLToken.h"
#include "HTMLDocument.h"
#include "HTMLHtmlElement.h"
#include "LegacyHTMLDocumentParser.h"
#include "HTMLNames.h"
#include "LegacyHTMLTreeBuilder.h"
#include "NotImplemented.h"
#include "ScriptController.h"
#include "Text.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

using namespace HTMLNames;

static const int uninitializedLineNumberValue = -1;

namespace {

inline bool isTreeBuilderWhiteSpace(UChar cc)
{
    return cc == '\t' || cc == '\x0A' || cc == '\x0C' || cc == '\x0D' || cc == ' ';
}

} // namespace

HTMLTreeBuilder::HTMLTreeBuilder(HTMLTokenizer* tokenizer, HTMLDocument* document, bool reportErrors)
    : m_framesetOk(true)
    , m_document(document)
    , m_reportErrors(reportErrors)
    , m_isPaused(false)
    , m_insertionMode(InitialMode)
    , m_originalInsertionMode(InitialMode)
    , m_tokenizer(tokenizer)
    , m_legacyTreeBuilder(new LegacyHTMLTreeBuilder(document, reportErrors))
    , m_lastScriptElementStartLine(uninitializedLineNumberValue)
    , m_scriptToProcessStartLine(uninitializedLineNumberValue)
    , m_fragmentScriptingPermission(FragmentScriptingAllowed)
{
}

// FIXME: Member variables should be grouped into self-initializing structs to
// minimize code duplication between these constructors.
HTMLTreeBuilder::HTMLTreeBuilder(HTMLTokenizer* tokenizer, DocumentFragment* fragment, FragmentScriptingPermission scriptingPermission)
    : m_framesetOk(true)
    , m_document(fragment->document())
    , m_reportErrors(false) // FIXME: Why not report errors in fragments?
    , m_isPaused(false)
    , m_insertionMode(InitialMode)
    , m_originalInsertionMode(InitialMode)
    , m_tokenizer(tokenizer)
    , m_legacyTreeBuilder(new LegacyHTMLTreeBuilder(fragment, scriptingPermission))
    , m_lastScriptElementStartLine(uninitializedLineNumberValue)
    , m_scriptToProcessStartLine(uninitializedLineNumberValue)
    , m_fragmentScriptingPermission(scriptingPermission)
{
}

HTMLTreeBuilder::~HTMLTreeBuilder()
{
}

static void convertToOldStyle(const AtomicHTMLToken& token, Token& oldStyleToken)
{
    switch (token.type()) {
    case HTMLToken::Uninitialized:
    case HTMLToken::DOCTYPE:
        ASSERT_NOT_REACHED();
        break;
    case HTMLToken::EndOfFile:
        ASSERT_NOT_REACHED();
        notImplemented();
        break;
    case HTMLToken::StartTag:
    case HTMLToken::EndTag: {
        oldStyleToken.beginTag = (token.type() == HTMLToken::StartTag);
        oldStyleToken.selfClosingTag = token.selfClosing();
        oldStyleToken.tagName = token.name();
        oldStyleToken.attrs = token.attributes();
        break;
    }
    case HTMLToken::Comment:
        oldStyleToken.tagName = commentAtom;
        oldStyleToken.text = token.comment().impl();
        break;
    case HTMLToken::Character:
        oldStyleToken.tagName = textAtom;
        oldStyleToken.text = token.characters().impl();
        break;
    }
}

void HTMLTreeBuilder::handleScriptStartTag()
{
    notImplemented(); // The HTML frgment case?
    m_tokenizer->setState(HTMLTokenizer::ScriptDataState);
    notImplemented(); // Save insertion mode.
}

void HTMLTreeBuilder::handleScriptEndTag(Element* scriptElement, int scriptStartLine)
{
    ASSERT(!m_scriptToProcess); // Caller never called takeScriptToProcess!
    ASSERT(m_scriptToProcessStartLine == uninitializedLineNumberValue); // Caller never called takeScriptToProcess!
    notImplemented(); // Save insertion mode and insertion point?

    // Pause ourselves so that parsing stops until the script can be processed by the caller.
    m_isPaused = true;
    m_scriptToProcess = scriptElement;
    // Lexer line numbers are 0-based, ScriptSourceCode expects 1-based lines,
    // so we convert here before passing the line number off to HTMLScriptRunner.
    m_scriptToProcessStartLine = scriptStartLine + 1;
}

PassRefPtr<Element> HTMLTreeBuilder::takeScriptToProcess(int& scriptStartLine)
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

HTMLTokenizer::State HTMLTreeBuilder::adjustedLexerState(HTMLTokenizer::State state, const AtomicString& tagName, Frame* frame)
{
    if (tagName == textareaTag || tagName == titleTag)
        return HTMLTokenizer::RCDATAState;

    if (tagName == styleTag || tagName == iframeTag || tagName == xmpTag || tagName == noembedTag
        || tagName == noframesTag || (tagName == noscriptTag && isScriptingFlagEnabled(frame)))
        return HTMLTokenizer::RAWTEXTState;

    if (tagName == plaintextTag)
        return HTMLTokenizer::PLAINTEXTState;

    return state;
}

void HTMLTreeBuilder::passTokenToLegacyParser(HTMLToken& token)
{
    if (token.type() == HTMLToken::DOCTYPE) {
        DoctypeToken doctypeToken;
        doctypeToken.m_name.append(token.name().data(), token.name().size());
        doctypeToken.m_publicID = token.publicIdentifier();
        doctypeToken.m_systemID = token.systemIdentifier();
        doctypeToken.m_forceQuirks = token.forceQuirks();

        m_legacyTreeBuilder->parseDoctypeToken(&doctypeToken);
        return;
    }

    // For now, we translate into an old-style token for testing.
    Token oldStyleToken;
    AtomicHTMLToken atomicToken(token);
    convertToOldStyle(atomicToken, oldStyleToken);

    RefPtr<Node> result =  m_legacyTreeBuilder->parseToken(&oldStyleToken);
    if (token.type() == HTMLToken::StartTag) {
        // This work is supposed to be done by the parser, but
        // when using the old parser for we have to do this manually.
        if (oldStyleToken.tagName == scriptTag) {
            handleScriptStartTag();
            m_lastScriptElement = static_pointer_cast<Element>(result);
            m_lastScriptElementStartLine = m_tokenizer->lineNumber();
        } else if (oldStyleToken.tagName == preTag || oldStyleToken.tagName == listingTag)
            m_tokenizer->skipLeadingNewLineForListing();
        else
            m_tokenizer->setState(adjustedLexerState(m_tokenizer->state(), oldStyleToken.tagName, m_document->frame()));
    } else if (token.type() == HTMLToken::EndTag) {
        if (oldStyleToken.tagName == scriptTag) {
            if (m_lastScriptElement) {
                ASSERT(m_lastScriptElementStartLine != uninitializedLineNumberValue);
                if (m_fragmentScriptingPermission == FragmentScriptingNotAllowed) {
                    // FIXME: This is a horrible hack for platform/Pasteboard.
                    // Clear the <script> tag when using the Parser to create
                    // a DocumentFragment for pasting so that javascript content
                    // does not show up in pasted HTML.
                    m_lastScriptElement->removeChildren();
                } else if (insertionMode() != AfterFramesetMode)
                    handleScriptEndTag(m_lastScriptElement.get(), m_lastScriptElementStartLine);
                m_lastScriptElement = 0;
                m_lastScriptElementStartLine = uninitializedLineNumberValue;
            }
        } else if (oldStyleToken.tagName == framesetTag)
            setInsertionMode(AfterFramesetMode);
    }
}

void HTMLTreeBuilder::constructTreeFromToken(HTMLToken& rawToken)
{
    // Make MSVC ignore our unreachable code for now.
    if (true) {
        passTokenToLegacyParser(rawToken);
        return;
    }

    AtomicHTMLToken token(rawToken);
    processToken(token);
}

void HTMLTreeBuilder::processToken(AtomicHTMLToken& token)
{
    switch (token.type()) {
    case HTMLToken::Uninitialized:
        ASSERT_NOT_REACHED();
        break;
    case HTMLToken::DOCTYPE:
        processDoctypeToken(token);
        break;
    case HTMLToken::StartTag:
        processStartTag(token);
        break;
    case HTMLToken::EndTag:
        processEndTag(token);
        break;
    case HTMLToken::Comment:
        processComment(token);
        return;
    case HTMLToken::Character:
        processCharacter(token);
        break;
    case HTMLToken::EndOfFile:
        processEndOfFile(token);
        break;
    }
}

void HTMLTreeBuilder::processDoctypeToken(AtomicHTMLToken& token)
{
    if (insertionMode() == InitialMode) {
        insertDoctype(token);
        return;
    }
    parseError(token);
}

void HTMLTreeBuilder::insertHTMLStartTagBeforeHTML(AtomicHTMLToken&)
{
    RefPtr<Element> element = HTMLHtmlElement::create(m_document);
    // FIXME: Add attributes to |element|.
    m_document->addChild(element);
    m_openElements.push(element.release());
}

void HTMLTreeBuilder::insertHTMLStartTagInBody(AtomicHTMLToken&)
{
    notImplemented();
}

void HTMLTreeBuilder::processStartTag(AtomicHTMLToken& token)
{
    switch (insertionMode()) {
    case InitialMode:
        ASSERT(insertionMode() == InitialMode);
        processDefaultForInitialMode(token);
        // Fall through.
    case BeforeHTMLMode:
        ASSERT(insertionMode() == BeforeHTMLMode);
        if (token.name() == htmlTag) {
            insertHTMLStartTagBeforeHTML(token);
            setInsertionMode(BeforeHeadMode);
            return;
        }
        processDefaultForBeforeHTMLMode(token);
        // Fall through.
    case BeforeHeadMode:
        ASSERT(insertionMode() == BeforeHeadMode);
        if (token.name() == htmlTag) {
            insertHTMLStartTagInBody(token);
            return;
        }
        if (token.name() == headTag) {
            insertElement(token);
            m_headElement = currentElement();
            setInsertionMode(InHeadMode);
            return;
        }
        processDefaultForBeforeHeadMode(token);
        // Fall through.
    case InHeadMode:
        ASSERT(insertionMode() == InHeadMode);
        if (token.name() == htmlTag) {
            insertHTMLStartTagInBody(token);
            return;
        }
        // FIXME: Atomize "command".
        if (token.name() == baseTag || token.name() == "command" || token.name() == linkTag) {
            insertElement(token);
            m_openElements.pop();
            notImplemented();
            return;
        }
        if (token.name() == metaTag) {
            insertElement(token);
            m_openElements.pop();
            notImplemented();
            return;
        }
        if (token.name() == titleTag) {
            insertGenericRCDATAElement(token);
            return;
        }
        if (token.name() == noscriptTag) {
            if (isScriptingFlagEnabled(m_document->frame())) {
                insertGenericRawTextElement(token);
                return;
            }
            insertElement(token);
            setInsertionMode(InHeadNoscriptMode);
            return;
        }
        if (token.name() == noframesTag || token.name() == styleTag) {
            insertGenericRawTextElement(token);
            return;
        }
        if (token.name() == scriptTag) {
            insertScriptElement(token);
            return;
        }
        if (token.name() == headTag) {
            notImplemented();
            return;
        }
        processDefaultForInHeadMode(token);
        // Fall through.
    case AfterHeadMode:
        ASSERT(insertionMode() == AfterHeadMode);
        if (token.name() == htmlTag) {
            insertHTMLStartTagInBody(token);
            return;
        }
        if (token.name() == bodyTag) {
            m_framesetOk = false;
            insertElement(token);
            return;
        }
        if (token.name() == framesetTag) {
            insertElement(token);
            setInsertionMode(InFramesetMode);
            return;
        }
        if (token.name() == baseTag || token.name() == linkTag || token.name() == metaTag || token.name() == noframesTag || token.name() == scriptTag || token.name() == styleTag || token.name() == titleTag) {
            parseError(token);
            ASSERT(m_headElement);
            m_openElements.push(m_headElement);
            notImplemented();
            m_openElements.remove(m_headElement.get());
            return;
        }
        if (token.name() == headTag) {
            parseError(token);
            return;
        }
        processDefaultForAfterHeadMode(token);
        // Fall through
    case InBodyMode:
        ASSERT(insertionMode() == InBodyMode);
        notImplemented();
        break;
    case InHeadNoscriptMode:
        ASSERT(insertionMode() == InHeadNoscriptMode);
        if (token.name() == htmlTag) {
            insertHTMLStartTagInBody(token);
            return;
        }
        if (token.name() == linkTag || token.name() == metaTag || token.name() == noframesTag || token.name() == styleTag) {
            notImplemented();
            return;
        }
        if (token.name() == htmlTag || token.name() == noscriptTag) {
            parseError(token);
            return;
        }
        processDefaultForInHeadNoscriptMode(token);
        processToken(token);
    default:
        notImplemented();
    }
}

void HTMLTreeBuilder::processEndTag(AtomicHTMLToken& token)
{
    switch (insertionMode()) {
    case InitialMode:
        ASSERT(insertionMode() == InitialMode);
        processDefaultForInitialMode(token);
        // Fall through.
    case BeforeHTMLMode:
        ASSERT(insertionMode() == BeforeHTMLMode);
        if (token.name() != headTag && token.name() != bodyTag && token.name() != htmlTag && token.name() != brTag) {
            parseError(token);
            return;
        }
        processDefaultForBeforeHTMLMode(token);
        // Fall through.
    case BeforeHeadMode:
        ASSERT(insertionMode() == BeforeHeadMode);
        if (token.name() != headTag && token.name() != bodyTag && token.name() != brTag) {
            parseError(token);
            return;
        }
        processDefaultForBeforeHeadMode(token);
        // Fall through.
    case InHeadMode:
        ASSERT(insertionMode() == InHeadMode);
        if (token.name() == headTag) {
            ASSERT(m_openElements.top()->tagQName() == headTag);
            m_openElements.pop();
            setInsertionMode(AfterHeadMode);
            return;
        }
        if (token.name() != bodyTag && token.name() != htmlTag && token.name() != brTag) {
            parseError(token);
            return;
        }
        processDefaultForInHeadMode(token);
        // Fall through.
    case AfterHeadMode:
        ASSERT(insertionMode() == AfterHeadMode);
        if (token.name() != bodyTag && token.name() != htmlTag && token.name() != brTag) {
            parseError(token);
            return;
        }
        processDefaultForAfterHeadMode(token);
        // Fall through
    case InBodyMode:
        ASSERT(insertionMode() == InBodyMode);
        notImplemented();
        break;
    case InHeadNoscriptMode:
        ASSERT(insertionMode() == InHeadNoscriptMode);
        if (token.name() == noscriptTag) {
            ASSERT(currentElement()->tagQName() == noscriptTag);
            m_openElements.pop();
            ASSERT(currentElement()->tagQName() == headTag);
            setInsertionMode(InHeadMode);
            return;
        }
        if (token.name() != brTag) {
            parseError(token);
            return;
        }
        processDefaultForInHeadNoscriptMode(token);
        processToken(token);
    case TextMode:
        if (token.name() == scriptTag) {
            // Pause ourselves so that parsing stops until the script can be processed by the caller.
            m_isPaused = true;
            ASSERT(currentElement()->tagQName() == scriptTag);
            m_scriptToProcess = currentElement();
            m_openElements.pop();
            m_insertionMode = m_originalInsertionMode;
            return;
        }
        notImplemented();
        break;
    default:
        notImplemented();
    }
}

void HTMLTreeBuilder::processComment(AtomicHTMLToken& token)
{
    if (insertionMode() == InHeadNoscriptMode) {
        notImplemented();
        return;
    }
    insertComment(token);
}

void HTMLTreeBuilder::processCharacter(AtomicHTMLToken& token)
{
    if (insertionMode() == TextMode) {
        currentElement()->addChild(Text::create(m_document, token.characters()));
        return;
    }
    // FIXME: We need to figure out how to handle each character individually.
    notImplemented();
}

void HTMLTreeBuilder::processEndOfFile(AtomicHTMLToken& token)
{
    switch (insertionMode()) {
    case InitialMode:
        ASSERT(insertionMode() == InitialMode);
        processDefaultForInitialMode(token);
        // Fall through.
    case BeforeHTMLMode:
        ASSERT(insertionMode() == BeforeHTMLMode);
        processDefaultForBeforeHTMLMode(token);
        // Fall through.
    case BeforeHeadMode:
        ASSERT(insertionMode() == BeforeHeadMode);
        processDefaultForBeforeHeadMode(token);
        // Fall through.
    case InHeadMode:
        ASSERT(insertionMode() == InHeadMode);
        processDefaultForInHeadMode(token);
        // Fall through.
    case AfterHeadMode:
        ASSERT(insertionMode() == AfterHeadMode);
        processDefaultForAfterHeadMode(token);
        // Fall through
    case InBodyMode:
        ASSERT(insertionMode() == InBodyMode);
        notImplemented();
        break;
    case InHeadNoscriptMode:
        ASSERT(insertionMode() == InHeadNoscriptMode);
        processDefaultForInHeadNoscriptMode(token);
        processToken(token);
    default:
        notImplemented();
    }
}

void HTMLTreeBuilder::processDefaultForInitialMode(AtomicHTMLToken& token)
{
    notImplemented();
    parseError(token);
    setInsertionMode(BeforeHTMLMode);
}

void HTMLTreeBuilder::processDefaultForBeforeHTMLMode(AtomicHTMLToken&)
{
    AtomicHTMLToken startHTML(HTMLToken::StartTag, htmlTag.localName());
    insertHTMLStartTagBeforeHTML(startHTML);
    setInsertionMode(BeforeHeadMode);
}

void HTMLTreeBuilder::processDefaultForBeforeHeadMode(AtomicHTMLToken&)
{
    AtomicHTMLToken startHead(HTMLToken::StartTag, headTag.localName());
    processStartTag(startHead);
}

void HTMLTreeBuilder::processDefaultForInHeadMode(AtomicHTMLToken&)
{
    AtomicHTMLToken endHead(HTMLToken::EndTag, headTag.localName());
    processEndTag(endHead);
}

void HTMLTreeBuilder::processDefaultForInHeadNoscriptMode(AtomicHTMLToken&)
{
    AtomicHTMLToken endNoscript(HTMLToken::EndTag, noscriptTag.localName());
    processEndTag(endNoscript);
}

void HTMLTreeBuilder::processDefaultForAfterHeadMode(AtomicHTMLToken&)
{
    AtomicHTMLToken startBody(HTMLToken::StartTag, bodyTag.localName());
    processStartTag(startBody);
    m_framesetOk = true;
}

void HTMLTreeBuilder::insertDoctype(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::DOCTYPE);
    m_document->addChild(DocumentType::create(m_document, token.name(), String::adopt(token.publicIdentifier()), String::adopt(token.systemIdentifier())));
    // FIXME: Move quirks mode detection from DocumentType element to here.
    notImplemented();
    if (token.forceQuirks())
        m_document->setParseMode(Document::Compat);
}

void HTMLTreeBuilder::insertComment(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    RefPtr<Node> element = Comment::create(m_document, token.comment());
    currentElement()->addChild(element);
}

void HTMLTreeBuilder::insertElement(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    RefPtr<Element> element = HTMLElementFactory::createHTMLElement(QualifiedName(nullAtom, token.name(), xhtmlNamespaceURI), m_document, 0);
    currentElement()->addChild(element);
    m_openElements.push(element.release());
}

void HTMLTreeBuilder::insertCharacter(UChar cc)
{
    ASSERT_UNUSED(cc, cc);
}

void HTMLTreeBuilder::insertGenericRCDATAElement(AtomicHTMLToken& token)
{
    ASSERT_UNUSED(token, token.type() == HTMLToken::StartTag);
}

void HTMLTreeBuilder::insertGenericRawTextElement(AtomicHTMLToken& token)
{
    ASSERT_UNUSED(token, token.type() == HTMLToken::StartTag);
}

void HTMLTreeBuilder::insertScriptElement(AtomicHTMLToken& token)
{
    ASSERT_UNUSED(token, token.type() == HTMLToken::StartTag);
    RefPtr<HTMLScriptElement> element = HTMLScriptElement::create(scriptTag, m_document, true);
    currentElement()->addChild(element);
    m_openElements.push(element.release());
    m_tokenizer->setState(HTMLTokenizer::ScriptDataState);
    m_originalInsertionMode = m_insertionMode;
    m_insertionMode = TextMode;
}

void HTMLTreeBuilder::finished()
{
    // We should call m_document->finishedParsing() here, except
    // m_legacyTreeBuilder->finished() does it for us.
    m_legacyTreeBuilder->finished();
}

bool HTMLTreeBuilder::isScriptingFlagEnabled(Frame* frame)
{
    if (!frame)
        return false;
    if (ScriptController* scriptController = frame->script())
        return scriptController->canExecuteScripts(NotAboutToExecuteScript);
    return false;
}

}
