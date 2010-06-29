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
#include "Settings.h"
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

bool shouldUseLegacyTreeBuilder(Document* document)
{
    return !document->settings() || !document->settings()->html5TreeBuilderEnabled();
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
    , m_legacyTreeBuilder(shouldUseLegacyTreeBuilder(document) ? new LegacyHTMLTreeBuilder(document, reportErrors) : 0)
    , m_lastScriptElementStartLine(uninitializedLineNumberValue)
    , m_scriptToProcessStartLine(uninitializedLineNumberValue)
    , m_fragmentScriptingPermission(FragmentScriptingAllowed)
    , m_isParsingFragment(false)
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
    , m_isParsingFragment(true)
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
    if (m_legacyTreeBuilder) {
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

void HTMLTreeBuilder::insertHTMLStartTagBeforeHTML(AtomicHTMLToken& token)
{
    RefPtr<Element> element = HTMLHtmlElement::create(m_document);
    element->setAttributeMap(token.attributes(), m_fragmentScriptingPermission);
    m_openElements.push(attach(m_document, element.release()));
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
        if (processStartTagForInHead(token))
            return;
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
            m_insertionMode = InBodyMode;
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
        if (token.name() == htmlTag) {
            insertHTMLStartTagInBody(token);
            return;
        }
        if (token.name() == baseTag || token.name() == "command" || token.name() == linkTag || token.name() == metaTag || token.name() == noframesTag || token.name() == scriptTag || token.name() == styleTag || token.name() == titleTag) {
            bool didProcess = processStartTagForInHead(token);
            ASSERT_UNUSED(didProcess, didProcess);
            return;
        }
        if (token.name() == bodyTag) {
            parseError(token);
            notImplemented();
            return;
        }
        if (token.name() == framesetTag) {
            notImplemented();
            return;
        }
        if (token.name() == addressTag || token.name() == articleTag || token.name() == asideTag || token.name() == blockquoteTag || token.name() == centerTag || token.name() == "details" || token.name() == dirTag || token.name() == divTag || token.name() == dlTag || token.name() == fieldsetTag || token.name() == "figure" || token.name() == footerTag || token.name() == headerTag || token.name() == hgroupTag || token.name() == menuTag || token.name() == navTag || token.name() == olTag || token.name() == pTag || token.name() == sectionTag || token.name() == ulTag) {
            notImplemented();
            insertElement(token);
            return;
        }
        if (token.name() == h1Tag || token.name() == h2Tag || token.name() == h3Tag || token.name() == h4Tag || token.name() == h5Tag || token.name() == h6Tag) {
            notImplemented();
            insertElement(token);
            return;
        }
        if (token.name() == preTag || token.name() == listingTag) {
            notImplemented();
            insertElement(token);
            m_tokenizer->skipLeadingNewLineForListing();
            m_framesetOk = false;
            return;
        }
        if (token.name() == formTag) {
            notImplemented();
            insertElement(token);
            m_formElement = currentElement();
            return;
        }
        if (token.name() == liTag) {
            notImplemented();
            insertElement(token);
            return;
        }
        if (token.name() == ddTag || token.name() == dtTag) {
            notImplemented();
            insertElement(token);
            return;
        }
        if (token.name() == plaintextTag) {
            notImplemented();
            insertElement(token);
            m_tokenizer->setState(HTMLTokenizer::PLAINTEXTState);
            return;
        }
        if (token.name() == buttonTag) {
            notImplemented();
            reconstructTheActiveFormattingElements();
            insertElement(token);
            m_framesetOk = false;
            return;
        }
        if (token.name() == aTag) {
            notImplemented();
            reconstructTheActiveFormattingElements();
            insertFormattingElement(token);
            return;
        }
        if (token.name() == bTag || token.name() == bigTag || token.name() == codeTag || token.name() == emTag || token.name() == fontTag || token.name() == iTag || token.name() == sTag || token.name() == smallTag || token.name() == strikeTag || token.name() == strongTag || token.name() == ttTag || token.name() == uTag) {
            reconstructTheActiveFormattingElements();
            insertFormattingElement(token);
            return;
        }
        if (token.name() == nobrTag) {
            reconstructTheActiveFormattingElements();
            notImplemented();
            insertFormattingElement(token);
            return;
        }
        if (token.name() == appletTag || token.name() == marqueeTag || token.name() == objectTag) {
            reconstructTheActiveFormattingElements();
            insertElement(token);
            notImplemented();
            m_framesetOk = false;
            return;
        }
        if (token.name() == tableTag) {
            notImplemented();
            insertElement(token);
            m_framesetOk = false;
            m_insertionMode = InTableMode;
            return;
        }
        if (token.name() == areaTag || token.name() == basefontTag || token.name() == "bgsound" || token.name() == brTag || token.name() == embedTag || token.name() == imgTag || token.name() == inputTag || token.name() == keygenTag || token.name() == wbrTag) {
            reconstructTheActiveFormattingElements();
            insertSelfClosingElement(token);
            m_framesetOk = false;
            return;
        }
        if (token.name() == paramTag || token.name() == sourceTag || token.name() == "track") {
            insertSelfClosingElement(token);
            return;
        }
        if (token.name() == hrTag) {
            notImplemented();
            insertSelfClosingElement(token);
            m_framesetOk = false;
            return;
        }
        if (token.name() == imageTag) {
            parseError(token);
            notImplemented();
            // Apparently we're not supposed to ask.
            return;
        }
        if (token.name() == isindexTag) {
            parseError(token);
            notImplemented();
            return;
        }
        if (token.name() == textareaTag) {
            insertElement(token);
            m_tokenizer->skipLeadingNewLineForListing();
            m_tokenizer->setState(HTMLTokenizer::RCDATAState);
            m_originalInsertionMode = m_insertionMode;
            m_framesetOk = false;
            m_insertionMode = TextMode;
            return;
        }
        if (token.name() == xmpTag) {
            notImplemented();
            reconstructTheActiveFormattingElements();
            m_framesetOk = false;
            insertGenericRawTextElement(token);
            return;
        }
        if (token.name() == iframeTag) {
            m_framesetOk = false;
            insertGenericRawTextElement(token);
            return;
        }
        if (token.name() == noembedTag) {
            insertGenericRawTextElement(token);
            return;
        }
        if (token.name() == noscriptTag && isScriptingFlagEnabled(m_document->frame())) {
            insertGenericRawTextElement(token);
            return;
        }
        if (token.name() == selectTag) {
            reconstructTheActiveFormattingElements();
            insertElement(token);
            m_framesetOk = false;
            if (m_insertionMode == InTableMode || m_insertionMode == InCaptionMode || m_insertionMode == InColumnGroupMode || m_insertionMode == InTableBodyMode || m_insertionMode == InRowMode || m_insertionMode == InCellMode)
                m_insertionMode = InSelectInTableMode;
            else
                m_insertionMode = InSelectMode;
            return;
        }
        if (token.name() == optgroupTag || token.name() == optionTag) {
            notImplemented();
            reconstructTheActiveFormattingElements();
            insertElement(token);
            return;
        }
        if (token.name() == rpTag || token.name() == rtTag) {
            notImplemented();
            insertElement(token);
            return;
        }
        if (token.name() == "math") {
            // This is the MathML foreign content branch point.
            notImplemented();
        }
        if (token.name() == "svg") {
            // This is the SVG foreign content branch point.
            notImplemented();
        }
        if (token.name() == captionTag || token.name() == colTag || token.name() == colgroupTag || token.name() == frameTag || token.name() == headTag || token.name() == tbodyTag || token.name() == tdTag || token.name() == tfootTag || token.name() == thTag || token.name() == theadTag || token.name() == trTag) {
            parseError(token);
            return;
        }
        reconstructTheActiveFormattingElements();
        insertElement(token);
        break;
    case InHeadNoscriptMode:
        ASSERT(insertionMode() == InHeadNoscriptMode);
        if (token.name() == htmlTag) {
            insertHTMLStartTagInBody(token);
            return;
        }
        if (token.name() == linkTag || token.name() == metaTag || token.name() == noframesTag || token.name() == styleTag) {
            bool didProcess = processStartTagForInHead(token);
            ASSERT_UNUSED(didProcess, didProcess);
            return;
        }
        if (token.name() == htmlTag || token.name() == noscriptTag) {
            parseError(token);
            return;
        }
        processDefaultForInHeadNoscriptMode(token);
        processToken(token);
        break;
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
        // FIXME: This implementation is a completely wrong but useful for testing.
        m_openElements.pop();
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
        break;
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
        m_openElements.pop();
        m_insertionMode = m_originalInsertionMode;
        break;
    default:
        notImplemented();
    }
}

void HTMLTreeBuilder::processComment(AtomicHTMLToken& token)
{
    if (m_insertionMode == InitialMode || m_insertionMode == BeforeHTMLMode) {
        insertCommentOnDocument(token);
        return;
    }
    insertComment(token);
}

void HTMLTreeBuilder::processCharacter(AtomicHTMLToken& token)
{
    // FIXME: We need to figure out how to handle each character individually.
    switch (insertionMode()) {
    case InitialMode:
        ASSERT(insertionMode() == InitialMode);
        notImplemented();
        processDefaultForInitialMode(token);
        // Fall through.
    case BeforeHTMLMode:
        ASSERT(insertionMode() == BeforeHTMLMode);
        notImplemented();
        processDefaultForBeforeHTMLMode(token);
        // Fall through.
    case BeforeHeadMode:
        ASSERT(insertionMode() == BeforeHeadMode);
        notImplemented();
        processDefaultForBeforeHeadMode(token);
        // Fall through.
    case InHeadMode:
        ASSERT(insertionMode() == InHeadMode);
        notImplemented();
        processDefaultForInHeadMode(token);
        // Fall through.
    case AfterHeadMode:
        ASSERT(insertionMode() == AfterHeadMode);
        notImplemented();
        processDefaultForAfterHeadMode(token);
        // Fall through
    case InBodyMode:
        ASSERT(insertionMode() == InBodyMode);
        notImplemented();
        insertTextNode(token);
        break;
    case TextMode:
        notImplemented();
        insertTextNode(token);
        break;
    case InHeadNoscriptMode:
        ASSERT(insertionMode() == InHeadNoscriptMode);
        processDefaultForInHeadNoscriptMode(token);
        processToken(token);
        break;
    default:
        notImplemented();
    }
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
        break;
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

bool HTMLTreeBuilder::processStartTagForInHead(AtomicHTMLToken& token)
{
    if (token.name() == htmlTag) {
        insertHTMLStartTagInBody(token);
        return true;
    }
    // FIXME: Atomize "command".
    if (token.name() == baseTag || token.name() == "command" || token.name() == linkTag || token.name() == metaTag) {
        insertSelfClosingElement(token);
        // Note: The custom processing for the <meta> tag is done in HTMLMetaElement::process().
        return true;
    }
    if (token.name() == titleTag) {
        insertGenericRCDATAElement(token);
        return true;
    }
    if (token.name() == noscriptTag) {
        if (isScriptingFlagEnabled(m_document->frame())) {
            insertGenericRawTextElement(token);
            return true;
        }
        insertElement(token);
        setInsertionMode(InHeadNoscriptMode);
        return true;
    }
    if (token.name() == noframesTag || token.name() == styleTag) {
        insertGenericRawTextElement(token);
        return true;
    }
    if (token.name() == scriptTag) {
        insertScriptElement(token);
        return true;
    }
    if (token.name() == headTag) {
        parseError(token);
        return true;
    }
    return false;
}

void HTMLTreeBuilder::insertDoctype(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::DOCTYPE);
    attach(m_document, DocumentType::create(m_document, token.name(), String::adopt(token.publicIdentifier()), String::adopt(token.systemIdentifier())));
    // FIXME: Move quirks mode detection from DocumentType element to here.
    notImplemented();
    if (token.forceQuirks())
        m_document->setParseMode(Document::Compat);
}

void HTMLTreeBuilder::insertComment(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    attach(currentElement(), Comment::create(m_document, token.comment()));
}

void HTMLTreeBuilder::insertCommentOnDocument(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    attach(m_document, Comment::create(m_document, token.comment()));
}

void HTMLTreeBuilder::insertElement(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    m_openElements.push(attach(currentElement(), createElement(token)));
}

void HTMLTreeBuilder::insertSelfClosingElement(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    attach(currentElement(), createElement(token));
    // FIXME: Do we want to acknowledge the token's self-closing flag?
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#acknowledge-self-closing-flag
}

void HTMLTreeBuilder::insertFormattingElement(AtomicHTMLToken& token)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#the-stack-of-open-elements
    // Possible active formatting elements include:
    // a, b, big, code, em, font, i, nobr, s, small, strike, strong, tt, and u.
    insertElement(token);
    m_activeFormattingElements.append(currentElement());
}

void HTMLTreeBuilder::insertGenericRCDATAElement(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    insertElement(token);
    m_tokenizer->setState(HTMLTokenizer::RCDATAState);
    m_originalInsertionMode = m_insertionMode;
    m_insertionMode = TextMode;
}

void HTMLTreeBuilder::insertGenericRawTextElement(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    insertElement(token);
    m_tokenizer->setState(HTMLTokenizer::RAWTEXTState);
    m_originalInsertionMode = m_insertionMode;
    m_insertionMode = TextMode;
}

void HTMLTreeBuilder::insertScriptElement(AtomicHTMLToken& token)
{
    ASSERT_UNUSED(token, token.type() == HTMLToken::StartTag);
    RefPtr<HTMLScriptElement> element = HTMLScriptElement::create(scriptTag, m_document, true);
    element->setAttributeMap(token.attributes(), m_fragmentScriptingPermission);
    m_openElements.push(attach(currentElement(), element.release()));
    m_tokenizer->setState(HTMLTokenizer::ScriptDataState);
    m_originalInsertionMode = m_insertionMode;
    m_insertionMode = TextMode;
}

void HTMLTreeBuilder::insertTextNode(AtomicHTMLToken& token)
{
    attach(currentElement(), Text::create(m_document, token.characters()));
}
    
PassRefPtr<Element> HTMLTreeBuilder::createElement(AtomicHTMLToken& token)
{
    RefPtr<Element> element = HTMLElementFactory::createHTMLElement(QualifiedName(nullAtom, token.name(), xhtmlNamespaceURI), m_document, 0);
    element->setAttributeMap(token.attributes(), m_fragmentScriptingPermission);
    return element.release();
}

unsigned HTMLTreeBuilder::indexOfLastOpenFormattingElementOrMarker() const
{
    ASSERT(!m_activeFormattingElements.isEmpty());
    for (int index = m_activeFormattingElements.size() - 1; index >= 0; --index) {
        const FormattingElementEntry& entry = m_activeFormattingElements[index];
        if (entry.isMarker() || m_openElements.contains(entry.element()))
            return index;
    }
    return -1; // No elements are open!
}

void HTMLTreeBuilder::reopenFormattingElementsAfterIndex(unsigned lastOpenElementIndex)
{
    Vector<FormattingElementEntry>& stack = m_activeFormattingElements;
    unsigned unopenEntryIndex = lastOpenElementIndex + 1;
    ASSERT(unopenEntryIndex < stack.size());
    for (; unopenEntryIndex < stack.size(); ++unopenEntryIndex) {
        FormattingElementEntry& unopenedEntry = stack[unopenEntryIndex];
        // FIXME: We're supposed to save the original token in the entry.
        AtomicHTMLToken fakeToken(HTMLToken::StartTag, unopenedEntry.element()->localName());
        insertElement(fakeToken);
        unopenedEntry.replaceElement(currentElement());
    }
}

void HTMLTreeBuilder::reconstructTheActiveFormattingElements()
{
    if (m_activeFormattingElements.isEmpty())
        return;

    unsigned lastOpenElementIndex = indexOfLastOpenFormattingElementOrMarker();
    if (lastOpenElementIndex < m_activeFormattingElements.size() - 1)
        reopenFormattingElementsAfterIndex(lastOpenElementIndex);
}

void HTMLTreeBuilder::finished()
{
    // We should call m_document->finishedParsing() here, except
    // m_legacyTreeBuilder->finished() does it for us.
    if (m_legacyTreeBuilder) {
        m_legacyTreeBuilder->finished();
        return;
    }

    AtomicHTMLToken eofToken(HTMLToken::EndOfFile, nullAtom);
    processToken(eofToken);

    // Warning, this may delete the parser, so don't try to do anything else after this.
    if (!m_isParsingFragment)
        m_document->finishedParsing();
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
