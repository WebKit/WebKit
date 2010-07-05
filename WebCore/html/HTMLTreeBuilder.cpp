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
#include "HTMLDocument.h"
#include "HTMLElementFactory.h"
#include "HTMLHtmlElement.h"
#include "HTMLNames.h"
#include "HTMLScriptElement.h"
#include "HTMLToken.h"
#include "HTMLTokenizer.h"
#include "LegacyHTMLDocumentParser.h"
#include "LegacyHTMLTreeBuilder.h"
#include "NotImplemented.h"
#if ENABLE(SVG)
#include "SVGNames.h"
#endif
#include "ScriptController.h"
#include "Settings.h"
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

bool isNumberedHeaderTag(const AtomicString& tagName)
{
    return tagName == h1Tag
        || tagName == h2Tag
        || tagName == h3Tag
        || tagName == h4Tag
        || tagName == h5Tag
        || tagName == h6Tag;
}

bool isTableBodyContextTag(const AtomicString& tagName)
{
    return tagName == tbodyTag
        || tagName == tfootTag
        || tagName == theadTag;
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#special
bool isSpecialTag(const AtomicString& tagName)
{
    return tagName == addressTag
        || tagName == articleTag
        || tagName == asideTag
        || tagName == baseTag
        || tagName == basefontTag
        || tagName == "bgsound"
        || tagName == blockquoteTag
        || tagName == bodyTag
        || tagName == brTag
        || tagName == buttonTag
        || tagName == centerTag
        || tagName == colTag
        || tagName == colgroupTag
        || tagName == "command"
        || tagName == ddTag
        || tagName == "details"
        || tagName == dirTag
        || tagName == divTag
        || tagName == dlTag
        || tagName == dtTag
        || tagName == embedTag
        || tagName == fieldsetTag
        || tagName == "figure"
        || tagName == footerTag
        || tagName == formTag
        || tagName == frameTag
        || tagName == framesetTag
        || isNumberedHeaderTag(tagName)
        || tagName == headTag
        || tagName == headerTag
        || tagName == hgroupTag
        || tagName == hrTag
        || tagName == iframeTag
        || tagName == imgTag
        || tagName == inputTag
        || tagName == isindexTag
        || tagName == liTag
        || tagName == linkTag
        || tagName == listingTag
        || tagName == menuTag
        || tagName == metaTag
        || tagName == navTag
        || tagName == noembedTag
        || tagName == noframesTag
        || tagName == noscriptTag
        || tagName == olTag
        || tagName == pTag
        || tagName == paramTag
        || tagName == plaintextTag
        || tagName == preTag
        || tagName == scriptTag
        || tagName == sectionTag
        || tagName == selectTag
        || tagName == styleTag
        || isTableBodyContextTag(tagName)
        || tagName == textareaTag
        || tagName == titleTag
        || tagName == trTag
        || tagName == ulTag
        || tagName == wbrTag
        || tagName == xmpTag;
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#scoping
// Same as isScopingTag in LegacyHTMLTreeBuilder.cpp
// and isScopeMarker in HTMLElementStack.cpp
bool isScopingTag(const AtomicString& tagName)
{
    return tagName == appletTag
        || tagName == buttonTag
        || tagName == captionTag
#if ENABLE(SVG_FOREIGN_OBJECT)
        || tagName == SVGNames::foreignObjectTag
#endif
        || tagName == htmlTag
        || tagName == marqueeTag
        || tagName == objectTag
        || tagName == tableTag
        || tagName == tdTag
        || tagName == thTag;
}

bool isNonAnchorFormattingTag(const AtomicString& tagName)
{
    return tagName == bTag
        || tagName == bigTag
        || tagName == codeTag
        || tagName == emTag
        || tagName == fontTag
        || tagName == iTag
        || tagName == nobrTag
        || tagName == sTag
        || tagName == smallTag
        || tagName == strikeTag
        || tagName == strongTag
        || tagName == ttTag
        || tagName == uTag;
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#formatting
bool isFormattingTag(const AtomicString& tagName)
{
    return tagName == aTag || isNonAnchorFormattingTag(tagName);
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#phrasing
bool isPhrasingTag(const AtomicString& tagName)
{
    return !isSpecialTag(tagName) && !isScopingTag(tagName) && !isFormattingTag(tagName);
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

    if (token.type() == HTMLToken::EndOfFile)
        return;

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
    m_openElements.pushHTMLHtmlElement(attach(m_document, element.release()));
}

void HTMLTreeBuilder::mergeAttributesFromTokenIntoElement(AtomicHTMLToken& token, Element* element)
{
    if (!token.attributes())
        return;

    NamedNodeMap* attributes = element->attributes(false);
    for (unsigned i = 0; i < token.attributes()->length(); ++i) {
        Attribute* attribute = token.attributes()->attributeItem(i);
        if (!attributes->getAttributeItem(attribute->name()))
            element->setAttribute(attribute->name(), attribute->value());
    }
}

void HTMLTreeBuilder::insertHTMLStartTagInBody(AtomicHTMLToken& token)
{
    parseError(token);
    mergeAttributesFromTokenIntoElement(token, m_openElements.htmlElement());
}

void HTMLTreeBuilder::processFakePEndTagIfPInScope()
{
    if (!m_openElements.inScope(pTag.localName()))
        return;
    AtomicHTMLToken endP(HTMLToken::EndTag, pTag.localName());
    processEndTag(endP);
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
            insertHTMLHeadElement(token);
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
            insertHTMLBodyElement(token);
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
            m_openElements.pushHTMLHeadElement(m_headElement);
            processStartTagForInHead(token);
            m_openElements.removeHTMLHeadElement(m_headElement.get());
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
            notImplemented(); // fragment case
            mergeAttributesFromTokenIntoElement(token, m_openElements.bodyElement());
            return;
        }
        if (token.name() == framesetTag) {
            parseError(token);
            notImplemented(); // fragment case
            if (!m_framesetOk)
                return;
            ExceptionCode ec = 0;
            m_openElements.bodyElement()->remove(ec);
            ASSERT(!ec);
            m_openElements.popUntil(m_openElements.bodyElement());
            m_openElements.popHTMLBodyElement();
            ASSERT(m_openElements.top() == m_openElements.htmlElement());
            insertElement(token);
            m_insertionMode = InFramesetMode;
            return;
        }
        if (token.name() == addressTag || token.name() == articleTag || token.name() == asideTag || token.name() == blockquoteTag || token.name() == centerTag || token.name() == "details" || token.name() == dirTag || token.name() == divTag || token.name() == dlTag || token.name() == fieldsetTag || token.name() == "figure" || token.name() == footerTag || token.name() == headerTag || token.name() == hgroupTag || token.name() == menuTag || token.name() == navTag || token.name() == olTag || token.name() == pTag || token.name() == sectionTag || token.name() == ulTag) {
            processFakePEndTagIfPInScope();
            insertElement(token);
            return;
        }
        if (isNumberedHeaderTag(token.name())) {
            processFakePEndTagIfPInScope();
            notImplemented();
            insertElement(token);
            return;
        }
        if (token.name() == preTag || token.name() == listingTag) {
            processFakePEndTagIfPInScope();
            insertElement(token);
            m_tokenizer->skipLeadingNewLineForListing();
            m_framesetOk = false;
            return;
        }
        if (token.name() == formTag) {
            notImplemented();
            processFakePEndTagIfPInScope();
            insertElement(token);
            m_formElement = currentElement();
            return;
        }
        if (token.name() == liTag) {
            notImplemented();
            processFakePEndTagIfPInScope();
            insertElement(token);
            return;
        }
        if (token.name() == ddTag || token.name() == dtTag) {
            notImplemented();
            processFakePEndTagIfPInScope();
            insertElement(token);
            return;
        }
        if (token.name() == plaintextTag) {
            processFakePEndTagIfPInScope();
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
        if (isNonAnchorFormattingTag(token.name())) {
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
        if (token.name() == imageTag) {
            parseError(token);
            // Apparently we're not supposed to ask.
            token.setName(imgTag.localName());
            // Note the fall through to the imgTag handling below!
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
            processFakePEndTagIfPInScope();
            insertSelfClosingElement(token);
            m_framesetOk = false;
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
            processFakePEndTagIfPInScope();
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
            if (m_openElements.inScope(optionTag.localName())) {
                AtomicHTMLToken endOption(HTMLToken::EndTag, optionTag.localName());
                processEndTag(endOption);
            }
            reconstructTheActiveFormattingElements();
            insertElement(token);
            return;
        }
        if (token.name() == rpTag || token.name() == rtTag) {
            if (m_openElements.inScope(rubyTag.localName())) {
                generateImpliedEndTags();
                if (!currentElement()->hasTagName(rubyTag)) {
                    parseError(token);
                    m_openElements.popUntil(rubyTag.localName());
                }
            }
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
    case InTableMode:
        ASSERT(insertionMode() == InTableMode);
        if (token.name() == captionTag) {
            m_openElements.popUntilTableScopeMarker();
            m_activeFormattingElements.appendMarker();
            insertElement(token);
            m_insertionMode = InCaptionMode;
            return;
        }
        if (token.name() == colgroupTag) {
            m_openElements.popUntilTableScopeMarker();
            insertElement(token);
            m_insertionMode = InColumnGroupMode;
            return;
        }
        if (token.name() == colTag) {
            AtomicHTMLToken fakeToken(HTMLToken::StartTag, colgroupTag.localName());
            processStartTag(fakeToken);
            ASSERT(InColumnGroupMode);
            processStartTag(token);
            return;
        }
        if (isTableBodyContextTag(token.name())) {
            m_openElements.popUntilTableScopeMarker();
            insertElement(token);
            m_insertionMode = InTableBodyMode;
            return;
        }
        if (token.name() == tdTag || token.name() == thTag || token.name() == trTag) {
            AtomicHTMLToken fakeToken(HTMLToken::StartTag, tbodyTag.localName());
            processStartTag(fakeToken);
            ASSERT(insertionMode() == InTableBodyMode);
            processStartTag(token);
            return;
        }
        if (token.name() == tableTag) {
            notImplemented();
            return;
        }
        if (token.name() == styleTag || token.name() == scriptTag) {
            processStartTagForInHead(token);
            return;
        }
        if (token.name() == inputTag) {
            notImplemented();
            return;
        }
        if (token.name() == formTag) {
            parseError(token);
            if (m_formElement)
                return;
            insertSelfClosingElement(token);
            return;
        }
        parseError(token);
        notImplemented();
        break;
    case InColumnGroupMode:
        if (token.name() == htmlTag) {
            insertHTMLStartTagInBody(token);
            return;
        }
        if (token.name() == colTag) {
            insertSelfClosingElement(token);
            return;
        }
        notImplemented();
        break;
    case InTableBodyMode:
        ASSERT(insertionMode() == InTableBodyMode);
        if (token.name() == trTag) {
            m_openElements.popUntilTableBodyScopeMarker(); // How is there ever anything to pop?
            insertElement(token);
            m_insertionMode = InRowMode;
            return;
        }
        if (token.name() == thTag || token.name() == tdTag) {
            parseError(token);
            AtomicHTMLToken fakeToken(HTMLToken::StartTag, trTag.localName());
            processStartTag(fakeToken);
            ASSERT(insertionMode() == InRowMode);
            processStartTag(token);
            return;
        }
        if (token.name() == captionTag || token.name() == colTag || token.name() == colgroupTag || isTableBodyContextTag(token.name())) {
            notImplemented();
            return;
        }
        notImplemented(); // process using "in table" rules
        break;
    case InRowMode:
        if (token.name() == thTag || token.name() == tdTag) {
            m_openElements.popUntilTableRowScopeMarker();
            insertElement(token);
            m_insertionMode = InCellMode;
            m_activeFormattingElements.appendMarker();
        }
        if (token.name() == captionTag || token.name() == colTag || token.name() == colgroupTag || isTableBodyContextTag(token.name())) {
            notImplemented();
            return;
        }
        notImplemented();
        break;
    case AfterBodyMode:
    case AfterAfterBodyMode:
        ASSERT(insertionMode() == AfterBodyMode || insertionMode() == AfterAfterBodyMode);
        if (token.name() == htmlTag) {
            insertHTMLStartTagInBody(token);
            return;
        }
        m_insertionMode = InBodyMode;
        processStartTag(token);
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
    case InFramesetMode:
        ASSERT(insertionMode() == InFramesetMode);
        if (token.name() == htmlTag) {
            insertHTMLStartTagInBody(token);
            return;
        }
        if (token.name() == framesetTag) {
            insertElement(token);
            return;
        }
        if (token.name() == frameTag) {
            insertSelfClosingElement(token);
            return;
        }
        if (token.name() == noframesTag) {
            processStartTagForInHead(token);
            return;
        }
        parseError(token);
        break;
    case AfterFramesetMode:
    case AfterAfterFramesetMode:
        ASSERT(insertionMode() == AfterFramesetMode || insertionMode() == AfterAfterFramesetMode);
        if (token.name() == htmlTag) {
            insertHTMLStartTagInBody(token);
            return;
        }
        if (token.name() == noframesTag) {
            processStartTagForInHead(token);
            return;
        }
        parseError(token);
        break;
    default:
        notImplemented();
    }
}

bool HTMLTreeBuilder::processBodyEndTagForInBody(AtomicHTMLToken& token)
{
    if (!m_openElements.inScope(bodyTag.localName())) {
        parseError(token);
        return false;
    }
    notImplemented();
    m_insertionMode = AfterBodyMode;
    return true;
}

void HTMLTreeBuilder::processAnyOtherEndTagForInBody(AtomicHTMLToken& token)
{
    HTMLElementStack::ElementRecord* record = m_openElements.topRecord();
    while (1) {
        Element* node = record->element();
        if (node->hasLocalName(token.name())) {
            generateImpliedEndTags();
            if (!currentElement()->hasLocalName(token.name())) {
                parseError(token);
                // FIXME: This is either a bug in the spec, or a bug in our
                // implementation.  Filed a bug with HTML5:
                // http://www.w3.org/Bugs/Public/show_bug.cgi?id=10080
                // We might have already popped the node for the token in
                // generateImpliedEndTags, just abort.
                if (!m_openElements.contains(node))
                    return;
            }
            m_openElements.popUntil(node);
            m_openElements.pop();
            return;
        }
        // !phrasing && !formatting == scoping || special
        const AtomicString& tagName = node->localName();
        if (isScopingTag(tagName) || isSpecialTag(tagName)) {
            parseError(token);
            return;
        }
        record = record->next();
    }
}

// FIXME: This probably belongs on HTMLElementStack.
HTMLElementStack::ElementRecord* HTMLTreeBuilder::furthestBlockForFormattingElement(Element* formattingElement)
{
    HTMLElementStack::ElementRecord* furthestBlock = 0;
    HTMLElementStack::ElementRecord* record = m_openElements.topRecord();
    for (; record; record = record->next()) {
        if (record->element() == formattingElement)
            return furthestBlock;
        const AtomicString& tagName = record->element()->localName();
        // !phrasing && !formatting == scoping || special
        if (isScopingTag(tagName) || isSpecialTag(tagName))
            furthestBlock = record;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void HTMLTreeBuilder::findFosterParentFor(Element* element)
{
    Element* fosterParentElement = 0;
    HTMLElementStack::ElementRecord* lastTableElementRecord = m_openElements.topmost(tableTag.localName());
    if (lastTableElementRecord) {
        Element* lastTableElement = lastTableElementRecord->element();
        if (lastTableElement->parent()) {
            // FIXME: We need an insertElement which does not send mutation events.
            ExceptionCode ec = 0;
            lastTableElement->parent()->insertBefore(element, lastTableElement, ec);
            ASSERT(!ec);
            return;
        }
        fosterParentElement = lastTableElementRecord->next()->element();
    } else {
        ASSERT(m_isParsingFragment);
        fosterParentElement = m_openElements.bottom(); // <html> element
    }

    fosterParentElement->parserAddChild(element);
}

// FIXME: This should have a whitty name.
// FIXME: This must be implemented in many other places in WebCore.
void HTMLTreeBuilder::reparentChildren(Element* oldParent, Element* newParent)
{
    Node* child = oldParent->firstChild();
    while (child) {
        Node* nextChild = child->nextSibling();
        ExceptionCode ec;
        newParent->appendChild(child, ec);
        ASSERT(!ec);
        child = nextChild;
    }
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#parsing-main-inbody
void HTMLTreeBuilder::callTheAdoptionAgency(AtomicHTMLToken& token)
{
    while (1) {
        // 1.
        Element* formattingElement = m_activeFormattingElements.closestElementInScopeWithName(token.name());
        if (!formattingElement || !m_openElements.inScope(formattingElement)) {
            parseError(token);
            notImplemented(); // Check the stack of open elements for a more specific parse error.
            return;
        }
        HTMLElementStack::ElementRecord* formattingElementRecord = m_openElements.find(formattingElement);
        if (!formattingElementRecord) {
            parseError(token);
            m_activeFormattingElements.remove(formattingElement);
            return;
        }
        if (formattingElement != currentElement())
            parseError(token);
        // 2.
        HTMLElementStack::ElementRecord* furthestBlock = furthestBlockForFormattingElement(formattingElement);
        // 3.
        if (!furthestBlock) {
            m_openElements.popUntil(formattingElement);
            m_openElements.pop();
            m_activeFormattingElements.remove(formattingElement);
            return;
        }
        // 4.
        ASSERT(furthestBlock->isAbove(formattingElementRecord));
        Element* commonAncestor = formattingElementRecord->next()->element();
        // 5.
        notImplemented(); // bookmark?
        // 6.
        HTMLElementStack::ElementRecord* node = furthestBlock;
        HTMLElementStack::ElementRecord* nextNode = node->next();
        HTMLElementStack::ElementRecord* lastNode = furthestBlock;
        while (1) {
            // 6.1
            node = nextNode;
            ASSERT(node);
            nextNode = node->next(); // Save node->next() for the next iteration in case node is deleted in 6.2.
            // 6.2
            if (!m_activeFormattingElements.contains(node->element())) {
                m_openElements.remove(node->element());
                node = 0;
                continue;
            }
            // 6.3
            if (node == formattingElementRecord)
                break;
            // 6.4
            if (lastNode == furthestBlock)
                notImplemented(); // move bookmark.
            // 6.5
            // FIXME: We're supposed to save the original token in the entry.
            AtomicHTMLToken fakeToken(HTMLToken::StartTag, node->element()->localName());
            // Is createElement correct? (instead of insertElement)
            // Does this code ever leave newElement unattached?
            RefPtr<Element> newElement = createElement(fakeToken);
            HTMLFormattingElementList::Entry* nodeEntry = m_activeFormattingElements.find(node->element());
            nodeEntry->replaceElement(newElement.get());
            node->replaceElement(newElement.release());
            // 6.6
            // Use appendChild instead of parserAddChild to handle possible reparenting.
            ExceptionCode ec;
            node->element()->appendChild(lastNode->element(), ec);
            ASSERT(!ec);
            // 6.7
            lastNode = node;
        }
        // 7
        const AtomicString& commonAncestorTag = commonAncestor->localName();
        if (commonAncestorTag == tableTag
            || commonAncestorTag == trTag
            || isTableBodyContextTag(commonAncestorTag))
            findFosterParentFor(lastNode->element());
        else {
            ExceptionCode ec;
            commonAncestor->appendChild(lastNode->element(), ec);
            ASSERT(!ec);
        }
        // 8
        // FIXME: We're supposed to save the original token in the entry.
        AtomicHTMLToken fakeToken(HTMLToken::StartTag, formattingElement->localName());
        RefPtr<Element> newElement = createElement(fakeToken);
        // 9
        reparentChildren(furthestBlock->element(), newElement.get());
        // 10
        furthestBlock->element()->parserAddChild(newElement);
        // 11
        m_activeFormattingElements.remove(formattingElement);
        notImplemented(); // insert new element at bookmark
        // 12
        m_openElements.remove(formattingElement);
        m_openElements.insertAbove(newElement, furthestBlock);
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
            m_openElements.popHTMLHeadElement();
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
        if (token.name() == bodyTag) {
            processBodyEndTagForInBody(token);
            return;
        }
        if (token.name() == htmlTag) {
            if (processBodyEndTagForInBody(token))
                processEndTag(token);
            return;
        }
        if (token.name() == addressTag || token.name() == articleTag || token.name() == asideTag || token.name() == blockquoteTag || token.name() == buttonTag || token.name() == centerTag || token.name() == "details" || token.name() == dirTag || token.name() == divTag || token.name() == dlTag || token.name() == fieldsetTag || token.name() == "figure" || token.name() == footerTag || token.name() == headerTag || token.name() == hgroupTag || token.name() == listingTag || token.name() == menuTag || token.name() == navTag || token.name() == olTag || token.name() == preTag || token.name() == sectionTag || token.name() == ulTag) {
            if (!m_openElements.inScope(token.name())) {
                parseError(token);
                return;
            }
            generateImpliedEndTags();
            if (currentElement()->tagQName() != token.name())
                parseError(token);
            m_openElements.popUntil(token.name());
            m_openElements.pop();
        }
        if (token.name() == formTag) {
            RefPtr<Element> node = m_formElement.release();
            if (!node || !m_openElements.inScope(node.get())) {
                parseError(token);
                return;
            }
            generateImpliedEndTags();
            if (currentElement() != node.get())
                parseError(token);
            m_openElements.remove(node.get());
        }
        if (token.name() == pTag) {
            if (!m_openElements.inScope(token.name())) {
                parseError(token);
                notImplemented();
                return;
            }
            generateImpliedEndTagsWithExclusion(token.name());
            if (!currentElement()->hasLocalName(token.name()))
                parseError(token);
            m_openElements.popUntil(token.name());
            m_openElements.pop();
            return;
        }
        if (token.name() == liTag) {
            if (!m_openElements.inListItemScope(token.name())) {
                parseError(token);
                return;
            }
            generateImpliedEndTagsWithExclusion(token.name());
            if (!currentElement()->hasLocalName(token.name()))
                parseError(token);
            m_openElements.popUntil(token.name());
            m_openElements.pop();
            return;
        }
        if (token.name() == ddTag || token.name() == dtTag) {
            if (!m_openElements.inScope(token.name())) {
                parseError(token);
                return;
            }
            generateImpliedEndTagsWithExclusion(token.name());
            if (!currentElement()->hasLocalName(token.name()))
                parseError(token);
            m_openElements.popUntil(token.name());
            m_openElements.pop();
            return;
        }
        if (isNumberedHeaderTag(token.name())) {
            if (!m_openElements.inScope(token.name())) {
                parseError(token);
                return;
            }
            generateImpliedEndTags();
            if (!currentElement()->hasLocalName(token.name()))
                parseError(token);
            m_openElements.popUntil(token.name());
            m_openElements.pop();
            return;
        }
        if (token.name() == "sarcasm") {
            notImplemented(); // Take a deep breath.
            return;
        }
        if (isFormattingTag(token.name())) {
            callTheAdoptionAgency(token);
            return;
        }
        if (token.name() == appletTag || token.name() == marqueeTag || token.name() == objectTag) {
            if (!m_openElements.inScope(token.name())) {
                parseError(token);
                return;
            }
            generateImpliedEndTags();
            if (currentElement()->tagQName() != token.name())
                parseError(token);
            m_openElements.popUntil(token.name());
            m_openElements.pop();
            m_activeFormattingElements.clearToLastMarker();
            return;
        }
        if (token.name() == brTag) {
            parseError(token);
            reconstructTheActiveFormattingElements();
            // Notice that we lose the attributes.
            AtomicHTMLToken startBr(HTMLToken::StartTag, token.name());
            insertSelfClosingElement(startBr);
            m_framesetOk = false;
            return;
        }
        processAnyOtherEndTagForInBody(token);
        break;
    case AfterBodyMode:
        ASSERT(insertionMode() == AfterBodyMode);
        if (token.name() == htmlTag) {
            if (m_isParsingFragment) {
                parseError(token);
                return;
            }
            m_insertionMode = AfterAfterBodyMode;
            return;
        }
        // Fall through.
    case AfterAfterBodyMode:
        ASSERT(insertionMode() == AfterBodyMode || insertionMode() == AfterAfterBodyMode);
        parseError(token);
        m_insertionMode = InBodyMode;
        processEndTag(token);
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
    case InFramesetMode:
        ASSERT(insertionMode() == InFramesetMode);
        if (token.name() == framesetTag) {
            if (currentElement() == m_openElements.htmlElement()) {
                parseError(token);
                return;
            }
            m_openElements.pop();
            if (!m_isParsingFragment && !currentElement()->hasTagName(framesetTag))
                m_insertionMode = AfterFramesetMode;
            return;
        }
        break;
    case AfterFramesetMode:
        ASSERT(insertionMode() == AfterFramesetMode);
        if (token.name() == htmlTag) {
            m_insertionMode = AfterAfterFramesetMode;
            return;
        }
        // Fall through.
    case AfterAfterFramesetMode:
        ASSERT(insertionMode() == AfterFramesetMode || insertionMode() == AfterAfterFramesetMode);
        parseError(token);
        break;
    default:
        notImplemented();
    }
}

void HTMLTreeBuilder::processComment(AtomicHTMLToken& token)
{
    if (m_insertionMode == InitialMode || m_insertionMode == BeforeHTMLMode || m_insertionMode == AfterAfterBodyMode || m_insertionMode == AfterAfterFramesetMode) {
        insertCommentOnDocument(token);
        return;
    }
    if (m_insertionMode == AfterBodyMode) {
        insertCommentOnHTMLHtmlElement(token);
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
    case AfterBodyMode:
    case AfterAfterBodyMode:
        ASSERT(insertionMode() == AfterBodyMode || insertionMode() == AfterAfterBodyMode);
        parseError(token);
        m_insertionMode = InBodyMode;
        processCharacter(token);
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
    case InFramesetMode:
    case AfterFramesetMode:
    case AfterAfterFramesetMode:
        ASSERT(insertionMode() == InFramesetMode || insertionMode() == AfterFramesetMode || insertionMode() == AfterAfterFramesetMode);
        parseError(token);
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
    case AfterBodyMode:
    case AfterAfterBodyMode:
        ASSERT(insertionMode() == AfterBodyMode || insertionMode() == AfterAfterBodyMode);
        notImplemented();
        break;
    case InHeadNoscriptMode:
        ASSERT(insertionMode() == InHeadNoscriptMode);
        processDefaultForInHeadNoscriptMode(token);
        processToken(token);
        break;
    case InFramesetMode:
        ASSERT(insertionMode() == InFramesetMode);
        if (currentElement() != m_openElements.htmlElement())
            parseError(token);
        break;
    case AfterFramesetMode:
    case AfterAfterFramesetMode:
        ASSERT(insertionMode() == AfterFramesetMode || insertionMode() == AfterAfterFramesetMode);
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

void HTMLTreeBuilder::insertCommentOnHTMLHtmlElement(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    attach(m_openElements.htmlElement(), Comment::create(m_document, token.comment()));
}

PassRefPtr<Element> HTMLTreeBuilder::createElementAndAttachToCurrent(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    return attach(currentElement(), createElement(token));
}

void HTMLTreeBuilder::insertHTMLHtmlElement(AtomicHTMLToken& token)
{
    m_openElements.pushHTMLHtmlElement(createElementAndAttachToCurrent(token));
}

void HTMLTreeBuilder::insertHTMLHeadElement(AtomicHTMLToken& token)
{
    m_headElement = createElementAndAttachToCurrent(token);
    m_openElements.pushHTMLHeadElement(m_headElement);
}

void HTMLTreeBuilder::insertHTMLBodyElement(AtomicHTMLToken& token)
{
    m_openElements.pushHTMLBodyElement(createElementAndAttachToCurrent(token));
}

void HTMLTreeBuilder::insertElement(AtomicHTMLToken& token)
{
    m_openElements.push(createElementAndAttachToCurrent(token));
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
    insertElement(token);
    m_tokenizer->setState(HTMLTokenizer::RCDATAState);
    m_originalInsertionMode = m_insertionMode;
    m_insertionMode = TextMode;
}

void HTMLTreeBuilder::insertGenericRawTextElement(AtomicHTMLToken& token)
{
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

bool HTMLTreeBuilder::indexOfFirstUnopenFormattingElement(unsigned& firstUnopenElementIndex) const
{
    if (m_activeFormattingElements.isEmpty())
        return false;
    unsigned index = m_activeFormattingElements.size();
    do {
        --index;
        const HTMLFormattingElementList::Entry& entry = m_activeFormattingElements[index];
        if (entry.isMarker() || m_openElements.contains(entry.element())) {
            firstUnopenElementIndex = index;
            return true;
        }
    } while (index);
    return false;
}

void HTMLTreeBuilder::reconstructTheActiveFormattingElements()
{
    unsigned firstUnopenElementIndex;
    if (!indexOfFirstUnopenFormattingElement(firstUnopenElementIndex))
        return;

    unsigned unopenEntryIndex = firstUnopenElementIndex;
    ASSERT(unopenEntryIndex < m_activeFormattingElements.size());
    for (; unopenEntryIndex < m_activeFormattingElements.size(); ++unopenEntryIndex) {
        HTMLFormattingElementList::Entry& unopenedEntry = m_activeFormattingElements[unopenEntryIndex];
        // FIXME: We're supposed to save the original token in the entry.
        AtomicHTMLToken fakeToken(HTMLToken::StartTag, unopenedEntry.element()->localName());
        insertElement(fakeToken);
        unopenedEntry.replaceElement(currentElement());
    }
}

namespace {

bool hasImpliedEndTag(Element* element)
{
    return element->hasTagName(ddTag)
        || element->hasTagName(dtTag)
        || element->hasTagName(liTag)
        || element->hasTagName(optionTag)
        || element->hasTagName(optgroupTag)
        || element->hasTagName(pTag)
        || element->hasTagName(rpTag)
        || element->hasTagName(rtTag);
}

}

void HTMLTreeBuilder::generateImpliedEndTagsWithExclusion(const AtomicString& tagName)
{
    while (hasImpliedEndTag(currentElement()) && !currentElement()->hasLocalName(tagName))
        m_openElements.pop();
}

void HTMLTreeBuilder::generateImpliedEndTags()
{
    while (hasImpliedEndTag(currentElement()))
        m_openElements.pop();
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
