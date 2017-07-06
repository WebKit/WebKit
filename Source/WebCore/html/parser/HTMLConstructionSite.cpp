/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
#include "CustomElementRegistry.h"
#include "DOMWindow.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLElementFactory.h"
#include "HTMLFormElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLImageElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLPictureElement.h"
#include "HTMLScriptElement.h"
#include "HTMLTemplateElement.h"
#include "HTMLUnknownElement.h"
#include "JSCustomElementInterface.h"
#include "NotImplemented.h"
#include "SVGElement.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

static inline void setAttributes(Element& element, Vector<Attribute>& attributes, ParserContentPolicy parserContentPolicy)
{
    if (!scriptingContentIsAllowed(parserContentPolicy))
        element.stripScriptingAttributes(attributes);
    element.parserSetAttributes(attributes);
}

static inline void setAttributes(Element& element, AtomicHTMLToken& token, ParserContentPolicy parserContentPolicy)
{
    setAttributes(element, token.attributes(), parserContentPolicy);
}

static bool hasImpliedEndTag(const HTMLStackItem& item)
{
    return item.hasTagName(ddTag)
        || item.hasTagName(dtTag)
        || item.hasTagName(liTag)
        || is<HTMLOptionElement>(item.node())
        || is<HTMLOptGroupElement>(item.node())
        || item.hasTagName(pTag)
        || item.hasTagName(rbTag)
        || item.hasTagName(rpTag)
        || item.hasTagName(rtTag)
        || item.hasTagName(rtcTag);
}

static bool shouldUseLengthLimit(const ContainerNode& node)
{
    return !node.hasTagName(scriptTag) && !node.hasTagName(styleTag) && !node.hasTagName(SVGNames::scriptTag);
}

static inline bool causesFosterParenting(const HTMLStackItem& item)
{
    return item.hasTagName(HTMLNames::tableTag)
        || item.hasTagName(HTMLNames::tbodyTag)
        || item.hasTagName(HTMLNames::tfootTag)
        || item.hasTagName(HTMLNames::theadTag)
        || item.hasTagName(HTMLNames::trTag);
}

static inline bool isAllWhitespace(const String& string)
{
    return string.isAllSpecialCharacters<isHTMLSpace>();
}

static inline void insert(HTMLConstructionSiteTask& task)
{
    if (is<HTMLTemplateElement>(*task.parent)) {
        task.parent = &downcast<HTMLTemplateElement>(*task.parent).content();
        task.nextChild = nullptr;
    }

    ASSERT(!task.child->parentNode());
    if (task.nextChild)
        task.parent->parserInsertBefore(*task.child, *task.nextChild);
    else
        task.parent->parserAppendChild(*task.child);
}

static inline void executeInsertTask(HTMLConstructionSiteTask& task)
{
    ASSERT(task.operation == HTMLConstructionSiteTask::Insert);

    insert(task);

    task.child->beginParsingChildren();

    if (task.selfClosing)
        task.child->finishParsingChildren();
}

static inline void executeReparentTask(HTMLConstructionSiteTask& task)
{
    ASSERT(task.operation == HTMLConstructionSiteTask::Reparent);
    ASSERT(!task.nextChild);

    if (auto* parent = task.child->parentNode())
        parent->parserRemoveChild(*task.child);

    if (task.child->parentNode())
        return;

    task.parent->parserAppendChild(*task.child);
}

static inline void executeInsertAlreadyParsedChildTask(HTMLConstructionSiteTask& task)
{
    ASSERT(task.operation == HTMLConstructionSiteTask::InsertAlreadyParsedChild);

    if (ContainerNode* parent = task.child->parentNode())
        parent->parserRemoveChild(*task.child);

    if (task.child->parentNode())
        return;

    if (task.nextChild && task.nextChild->parentNode() != task.parent)
        return;

    insert(task);
}

static inline void executeTakeAllChildrenAndReparentTask(HTMLConstructionSiteTask& task)
{
    ASSERT(task.operation == HTMLConstructionSiteTask::TakeAllChildrenAndReparent);
    ASSERT(!task.nextChild);

    auto* furthestBlock = task.oldParent();
    task.parent->takeAllChildrenFrom(furthestBlock);

    RELEASE_ASSERT(!task.parent->parentNode());
    furthestBlock->parserAppendChild(*task.parent);
}

static inline void executeTask(HTMLConstructionSiteTask& task)
{
    switch (task.operation) {
    case HTMLConstructionSiteTask::Insert:
        executeInsertTask(task);
        return;
    // All the cases below this point are only used by the adoption agency.
    case HTMLConstructionSiteTask::InsertAlreadyParsedChild:
        executeInsertAlreadyParsedChildTask(task);
        return;
    case HTMLConstructionSiteTask::Reparent:
        executeReparentTask(task);
        return;
    case HTMLConstructionSiteTask::TakeAllChildrenAndReparent:
        executeTakeAllChildrenAndReparentTask(task);
        return;
    }
    ASSERT_NOT_REACHED();
}

void HTMLConstructionSite::attachLater(ContainerNode& parent, Ref<Node>&& child, bool selfClosing)
{
    ASSERT(scriptingContentIsAllowed(m_parserContentPolicy) || !is<Element>(child.get()) || !isScriptElement(downcast<Element>(child.get())));
    ASSERT(pluginContentIsAllowed(m_parserContentPolicy) || !child->isPluginElement());

    if (shouldFosterParent()) {
        fosterParent(WTFMove(child));
        return;
    }

    HTMLConstructionSiteTask task(HTMLConstructionSiteTask::Insert);
    task.parent = &parent;
    task.child = WTFMove(child);
    task.selfClosing = selfClosing;

    // Add as a sibling of the parent if we have reached the maximum depth allowed.
    if (m_openElements.stackDepth() > m_maximumDOMTreeDepth && task.parent->parentNode())
        task.parent = task.parent->parentNode();

    ASSERT(task.parent);
    m_taskQueue.append(WTFMove(task));
}

void HTMLConstructionSite::executeQueuedTasks()
{
    if (m_taskQueue.isEmpty())
        return;

    // Copy the task queue into a local variable in case executeTask
    // re-enters the parser.
    TaskQueue queue = WTFMove(m_taskQueue);

    for (auto& task : queue)
        executeTask(task);

    // We might be detached now.
}

HTMLConstructionSite::HTMLConstructionSite(Document& document, ParserContentPolicy parserContentPolicy, unsigned maximumDOMTreeDepth)
    : m_document(document)
    , m_attachmentRoot(document)
    , m_parserContentPolicy(parserContentPolicy)
    , m_isParsingFragment(false)
    , m_redirectAttachToFosterParent(false)
    , m_maximumDOMTreeDepth(maximumDOMTreeDepth)
    , m_inQuirksMode(document.inQuirksMode())
{
    ASSERT(m_document.isHTMLDocument() || m_document.isXHTMLDocument());
}

HTMLConstructionSite::HTMLConstructionSite(DocumentFragment& fragment, ParserContentPolicy parserContentPolicy, unsigned maximumDOMTreeDepth)
    : m_document(fragment.document())
    , m_attachmentRoot(fragment)
    , m_parserContentPolicy(parserContentPolicy)
    , m_isParsingFragment(true)
    , m_redirectAttachToFosterParent(false)
    , m_maximumDOMTreeDepth(maximumDOMTreeDepth)
    , m_inQuirksMode(fragment.document().inQuirksMode())
{
    ASSERT(m_document.isHTMLDocument() || m_document.isXHTMLDocument());
}

HTMLConstructionSite::~HTMLConstructionSite()
{
}

void HTMLConstructionSite::setForm(HTMLFormElement* form)
{
    // This method should only be needed for HTMLTreeBuilder in the fragment case.
    ASSERT(!m_form);
    m_form = form;
}

RefPtr<HTMLFormElement> HTMLConstructionSite::takeForm()
{
    return WTFMove(m_form);
}

void HTMLConstructionSite::dispatchDocumentElementAvailableIfNeeded()
{
    if (m_isParsingFragment)
        return;

    if (auto* frame = m_document.frame())
        frame->injectUserScripts(InjectAtDocumentStart);
}

void HTMLConstructionSite::insertHTMLHtmlStartTagBeforeHTML(AtomicHTMLToken&& token)
{
    auto element = HTMLHtmlElement::create(m_document);
    setAttributes(element, token, m_parserContentPolicy);
    attachLater(m_attachmentRoot, element.copyRef());
    m_openElements.pushHTMLHtmlElement(HTMLStackItem::create(element.copyRef(), WTFMove(token)));

    executeQueuedTasks();
    element->insertedByParser();
    dispatchDocumentElementAvailableIfNeeded();
}

void HTMLConstructionSite::mergeAttributesFromTokenIntoElement(AtomicHTMLToken&& token, Element& element)
{
    if (token.attributes().isEmpty())
        return;

    for (auto& tokenAttribute : token.attributes()) {
        if (!element.elementData() || !element.findAttributeByName(tokenAttribute.name()))
            element.setAttribute(tokenAttribute.name(), tokenAttribute.value());
    }
}

void HTMLConstructionSite::insertHTMLHtmlStartTagInBody(AtomicHTMLToken&& token)
{
    // Fragments do not have a root HTML element, so any additional HTML elements
    // encountered during fragment parsing should be ignored.
    if (m_isParsingFragment)
        return;

    mergeAttributesFromTokenIntoElement(WTFMove(token), m_openElements.htmlElement());
}

void HTMLConstructionSite::insertHTMLBodyStartTagInBody(AtomicHTMLToken&& token)
{
    mergeAttributesFromTokenIntoElement(WTFMove(token), m_openElements.bodyElement());
}

void HTMLConstructionSite::setDefaultCompatibilityMode()
{
    if (m_isParsingFragment)
        return;
    if (m_document.isSrcdocDocument())
        return;
    setCompatibilityMode(DocumentCompatibilityMode::QuirksMode);
}

void HTMLConstructionSite::setCompatibilityMode(DocumentCompatibilityMode mode)
{
    m_inQuirksMode = (mode == DocumentCompatibilityMode::QuirksMode);
    m_document.setCompatibilityMode(mode);
}

void HTMLConstructionSite::setCompatibilityModeFromDoctype(const String& name, const String& publicId, const String& systemId)
{
    // There are three possible compatibility modes:
    // Quirks - quirks mode emulates WinIE and NS4. CSS parsing is also relaxed in this mode, e.g., unit types can
    // be omitted from numbers.
    // Limited Quirks - This mode is identical to no-quirks mode except for its treatment of line-height in the inline box model.  
    // No Quirks - no quirks apply. Web pages will obey the specifications to the letter.

    // Check for Quirks Mode.
    if (name != "html"
        || publicId.startsWith("+//Silmaril//dtd html Pro v0r11 19970101//", false)
        || publicId.startsWith("-//AdvaSoft Ltd//DTD HTML 3.0 asWedit + extensions//", false)
        || publicId.startsWith("-//AS//DTD HTML 3.0 asWedit + extensions//", false)
        || publicId.startsWith("-//IETF//DTD HTML 2.0 Level 1//", false)
        || publicId.startsWith("-//IETF//DTD HTML 2.0 Level 2//", false)
        || publicId.startsWith("-//IETF//DTD HTML 2.0 Strict Level 1//", false)
        || publicId.startsWith("-//IETF//DTD HTML 2.0 Strict Level 2//", false)
        || publicId.startsWith("-//IETF//DTD HTML 2.0 Strict//", false)
        || publicId.startsWith("-//IETF//DTD HTML 2.0//", false)
        || publicId.startsWith("-//IETF//DTD HTML 2.1E//", false)
        || publicId.startsWith("-//IETF//DTD HTML 3.0//", false)
        || publicId.startsWith("-//IETF//DTD HTML 3.2 Final//", false)
        || publicId.startsWith("-//IETF//DTD HTML 3.2//", false)
        || publicId.startsWith("-//IETF//DTD HTML 3//", false)
        || publicId.startsWith("-//IETF//DTD HTML Level 0//", false)
        || publicId.startsWith("-//IETF//DTD HTML Level 1//", false)
        || publicId.startsWith("-//IETF//DTD HTML Level 2//", false)
        || publicId.startsWith("-//IETF//DTD HTML Level 3//", false)
        || publicId.startsWith("-//IETF//DTD HTML Strict Level 0//", false)
        || publicId.startsWith("-//IETF//DTD HTML Strict Level 1//", false)
        || publicId.startsWith("-//IETF//DTD HTML Strict Level 2//", false)
        || publicId.startsWith("-//IETF//DTD HTML Strict Level 3//", false)
        || publicId.startsWith("-//IETF//DTD HTML Strict//", false)
        || publicId.startsWith("-//IETF//DTD HTML//", false)
        || publicId.startsWith("-//Metrius//DTD Metrius Presentational//", false)
        || publicId.startsWith("-//Microsoft//DTD Internet Explorer 2.0 HTML Strict//", false)
        || publicId.startsWith("-//Microsoft//DTD Internet Explorer 2.0 HTML//", false)
        || publicId.startsWith("-//Microsoft//DTD Internet Explorer 2.0 Tables//", false)
        || publicId.startsWith("-//Microsoft//DTD Internet Explorer 3.0 HTML Strict//", false)
        || publicId.startsWith("-//Microsoft//DTD Internet Explorer 3.0 HTML//", false)
        || publicId.startsWith("-//Microsoft//DTD Internet Explorer 3.0 Tables//", false)
        || publicId.startsWith("-//Netscape Comm. Corp.//DTD HTML//", false)
        || publicId.startsWith("-//Netscape Comm. Corp.//DTD Strict HTML//", false)
        || publicId.startsWith("-//O'Reilly and Associates//DTD HTML 2.0//", false)
        || publicId.startsWith("-//O'Reilly and Associates//DTD HTML Extended 1.0//", false)
        || publicId.startsWith("-//O'Reilly and Associates//DTD HTML Extended Relaxed 1.0//", false)
        || publicId.startsWith("-//SoftQuad Software//DTD HoTMetaL PRO 6.0::19990601::extensions to HTML 4.0//", false)
        || publicId.startsWith("-//SoftQuad//DTD HoTMetaL PRO 4.0::19971010::extensions to HTML 4.0//", false)
        || publicId.startsWith("-//Spyglass//DTD HTML 2.0 Extended//", false)
        || publicId.startsWith("-//SQ//DTD HTML 2.0 HoTMetaL + extensions//", false)
        || publicId.startsWith("-//Sun Microsystems Corp.//DTD HotJava HTML//", false)
        || publicId.startsWith("-//Sun Microsystems Corp.//DTD HotJava Strict HTML//", false)
        || publicId.startsWith("-//W3C//DTD HTML 3 1995-03-24//", false)
        || publicId.startsWith("-//W3C//DTD HTML 3.2 Draft//", false)
        || publicId.startsWith("-//W3C//DTD HTML 3.2 Final//", false)
        || publicId.startsWith("-//W3C//DTD HTML 3.2//", false)
        || publicId.startsWith("-//W3C//DTD HTML 3.2S Draft//", false)
        || publicId.startsWith("-//W3C//DTD HTML 4.0 Frameset//", false)
        || publicId.startsWith("-//W3C//DTD HTML 4.0 Transitional//", false)
        || publicId.startsWith("-//W3C//DTD HTML Experimental 19960712//", false)
        || publicId.startsWith("-//W3C//DTD HTML Experimental 970421//", false)
        || publicId.startsWith("-//W3C//DTD W3 HTML//", false)
        || publicId.startsWith("-//W3O//DTD W3 HTML 3.0//", false)
        || equalLettersIgnoringASCIICase(publicId, "-//w3o//dtd w3 html strict 3.0//en//")
        || publicId.startsWith("-//WebTechs//DTD Mozilla HTML 2.0//", false)
        || publicId.startsWith("-//WebTechs//DTD Mozilla HTML//", false)
        || equalLettersIgnoringASCIICase(publicId, "-/w3c/dtd html 4.0 transitional/en")
        || equalLettersIgnoringASCIICase(publicId, "html")
        || equalLettersIgnoringASCIICase(systemId, "http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd")
        || (systemId.isEmpty() && publicId.startsWith("-//W3C//DTD HTML 4.01 Frameset//", false))
        || (systemId.isEmpty() && publicId.startsWith("-//W3C//DTD HTML 4.01 Transitional//", false))) {
        setCompatibilityMode(DocumentCompatibilityMode::QuirksMode);
        return;
    }

    // Check for Limited Quirks Mode.
    if (publicId.startsWith("-//W3C//DTD XHTML 1.0 Frameset//", false)
        || publicId.startsWith("-//W3C//DTD XHTML 1.0 Transitional//", false)
        || (!systemId.isEmpty() && publicId.startsWith("-//W3C//DTD HTML 4.01 Frameset//", false))
        || (!systemId.isEmpty() && publicId.startsWith("-//W3C//DTD HTML 4.01 Transitional//", false))) {
        setCompatibilityMode(DocumentCompatibilityMode::LimitedQuirksMode);
        return;
    }

    // Otherwise we are No Quirks Mode.
    setCompatibilityMode(DocumentCompatibilityMode::NoQuirksMode);
}

void HTMLConstructionSite::finishedParsing()
{
    m_document.finishedParsing();
}

void HTMLConstructionSite::insertDoctype(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::DOCTYPE);

    String publicId = token.publicIdentifier();
    String systemId = token.systemIdentifier();

    attachLater(m_attachmentRoot, DocumentType::create(m_document, token.name(), publicId, systemId));

    // DOCTYPE nodes are only processed when parsing fragments w/o contextElements, which
    // never occurs.  However, if we ever chose to support such, this code is subtly wrong,
    // because context-less fragments can determine their own quirks mode, and thus change
    // parsing rules (like <p> inside <table>).  For now we ASSERT that we never hit this code
    // in a fragment, as changing the owning document's compatibility mode would be wrong.
    ASSERT(!m_isParsingFragment);
    if (m_isParsingFragment)
        return;

    if (token.forceQuirks())
        setCompatibilityMode(DocumentCompatibilityMode::QuirksMode);
    else
        setCompatibilityModeFromDoctype(token.name(), publicId, systemId);
}

void HTMLConstructionSite::insertComment(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    attachLater(currentNode(), Comment::create(ownerDocumentForCurrentNode(), token.comment()));
}

void HTMLConstructionSite::insertCommentOnDocument(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    attachLater(m_attachmentRoot, Comment::create(m_document, token.comment()));
}

void HTMLConstructionSite::insertCommentOnHTMLHtmlElement(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    ContainerNode& parent = m_openElements.rootNode();
    attachLater(parent, Comment::create(parent.document(), token.comment()));
}

void HTMLConstructionSite::insertHTMLHeadElement(AtomicHTMLToken&& token)
{
    ASSERT(!shouldFosterParent());
    m_head = HTMLStackItem::create(createHTMLElement(token), WTFMove(token));
    attachLater(currentNode(), m_head->element());
    m_openElements.pushHTMLHeadElement(*m_head);
}

void HTMLConstructionSite::insertHTMLBodyElement(AtomicHTMLToken&& token)
{
    ASSERT(!shouldFosterParent());
    auto body = createHTMLElement(token);
    attachLater(currentNode(), body.copyRef());
    m_openElements.pushHTMLBodyElement(HTMLStackItem::create(WTFMove(body), WTFMove(token)));
}

void HTMLConstructionSite::insertHTMLFormElement(AtomicHTMLToken&& token, bool isDemoted)
{
    auto element = createHTMLElement(token);
    auto& formElement = downcast<HTMLFormElement>(element.get());
    // If there is no template element on the stack of open elements, set the
    // form element pointer to point to the element created.
    if (!openElements().hasTemplateInHTMLScope())
        m_form = &formElement;
    formElement.setDemoted(isDemoted);
    attachLater(currentNode(), formElement);
    m_openElements.push(HTMLStackItem::create(formElement, WTFMove(token)));
}

void HTMLConstructionSite::insertHTMLElement(AtomicHTMLToken&& token)
{
    auto element = createHTMLElement(token);
    attachLater(currentNode(), element.copyRef());
    m_openElements.push(HTMLStackItem::create(WTFMove(element), WTFMove(token)));
}

std::unique_ptr<CustomElementConstructionData> HTMLConstructionSite::insertHTMLElementOrFindCustomElementInterface(AtomicHTMLToken&& token)
{
    JSCustomElementInterface* elementInterface = nullptr;
    RefPtr<Element> element = createHTMLElementOrFindCustomElementInterface(token, &elementInterface);
    if (UNLIKELY(elementInterface))
        return std::make_unique<CustomElementConstructionData>(*elementInterface, token.name(), WTFMove(token.attributes()));
    attachLater(currentNode(), *element);
    m_openElements.push(HTMLStackItem::create(element.releaseNonNull(), WTFMove(token)));
    return nullptr;
}

void HTMLConstructionSite::insertCustomElement(Ref<Element>&& element, const AtomicString& localName, Vector<Attribute>&& attributes)
{
    setAttributes(element, attributes, m_parserContentPolicy);
    attachLater(currentNode(), element.copyRef());
    m_openElements.push(HTMLStackItem::create(WTFMove(element), localName, WTFMove(attributes)));
}

void HTMLConstructionSite::insertSelfClosingHTMLElement(AtomicHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    // Normally HTMLElementStack is responsible for calling finishParsingChildren,
    // but self-closing elements are never in the element stack so the stack
    // doesn't get a chance to tell them that we're done parsing their children.
    attachLater(currentNode(), createHTMLElement(token), true);
    // FIXME: Do we want to acknowledge the token's self-closing flag?
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#acknowledge-self-closing-flag
}

void HTMLConstructionSite::insertFormattingElement(AtomicHTMLToken&& token)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#the-stack-of-open-elements
    // Possible active formatting elements include:
    // a, b, big, code, em, font, i, nobr, s, small, strike, strong, tt, and u.
    ASSERT(isFormattingTag(token.name()));
    insertHTMLElement(WTFMove(token));
    m_activeFormattingElements.append(currentStackItem());
}

void HTMLConstructionSite::insertScriptElement(AtomicHTMLToken&& token)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/scripting-1.html#already-started
    // http://html5.org/specs/dom-parsing.html#dom-range-createcontextualfragment
    // For createContextualFragment, the specifications say to mark it parser-inserted and already-started and later unmark them.
    // However, we short circuit that logic to avoid the subtree traversal to find script elements since scripts can never see
    // those flags or effects thereof.
    const bool parserInserted = m_parserContentPolicy != AllowScriptingContentAndDoNotMarkAlreadyStarted;
    const bool alreadyStarted = m_isParsingFragment && parserInserted;
    auto element = HTMLScriptElement::create(scriptTag, ownerDocumentForCurrentNode(), parserInserted, alreadyStarted);
    setAttributes(element, token, m_parserContentPolicy);
    if (scriptingContentIsAllowed(m_parserContentPolicy))
        attachLater(currentNode(), element.copyRef());
    m_openElements.push(HTMLStackItem::create(WTFMove(element), WTFMove(token)));
}

void HTMLConstructionSite::insertForeignElement(AtomicHTMLToken&& token, const AtomicString& namespaceURI)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    notImplemented(); // parseError when xmlns or xmlns:xlink are wrong.

    auto element = createElement(token, namespaceURI);
    if (scriptingContentIsAllowed(m_parserContentPolicy) || !isScriptElement(element.get()))
        attachLater(currentNode(), element.copyRef(), token.selfClosing());
    if (!token.selfClosing())
        m_openElements.push(HTMLStackItem::create(WTFMove(element), WTFMove(token), namespaceURI));
}

void HTMLConstructionSite::insertTextNode(const String& characters, WhitespaceMode whitespaceMode)
{
    HTMLConstructionSiteTask task(HTMLConstructionSiteTask::Insert);
    task.parent = &currentNode();

    if (shouldFosterParent())
        findFosterSite(task);

    if (is<HTMLTemplateElement>(*task.parent))
        task.parent = &downcast<HTMLTemplateElement>(*task.parent).content();

    // Strings composed entirely of whitespace are likely to be repeated.
    // Turn them into AtomicString so we share a single string for each.
    bool shouldUseAtomicString = whitespaceMode == AllWhitespace || (whitespaceMode == WhitespaceUnknown && isAllWhitespace(characters));

    unsigned currentPosition = 0;
    unsigned lengthLimit = shouldUseLengthLimit(*task.parent) ? Text::defaultLengthLimit : std::numeric_limits<unsigned>::max();

    // FIXME: Splitting text nodes into smaller chunks contradicts HTML5 spec, but is currently necessary
    // for performance, see <https://bugs.webkit.org/show_bug.cgi?id=55898>.

    Node* previousChild = task.nextChild ? task.nextChild->previousSibling() : task.parent->lastChild();
    if (is<Text>(previousChild)) {
        // FIXME: We're only supposed to append to this text node if it
        // was the last text node inserted by the parser.
        currentPosition = downcast<Text>(*previousChild).parserAppendData(characters, 0, lengthLimit);
    }

    while (currentPosition < characters.length()) {
        auto textNode = Text::createWithLengthLimit(task.parent->document(), shouldUseAtomicString ? AtomicString(characters).string() : characters, currentPosition, lengthLimit);
        // If we have a whole string of unbreakable characters the above could lead to an infinite loop. Exceeding the length limit is the lesser evil.
        if (!textNode->length()) {
            String substring = characters.substring(currentPosition);
            textNode = Text::create(task.parent->document(), shouldUseAtomicString ? AtomicString(substring).string() : substring);
        }

        currentPosition += textNode->length();
        ASSERT(currentPosition <= characters.length());
        task.child = WTFMove(textNode);

        executeTask(task);
    }
}

void HTMLConstructionSite::reparent(HTMLElementStack::ElementRecord& newParent, HTMLElementStack::ElementRecord& child)
{
    HTMLConstructionSiteTask task(HTMLConstructionSiteTask::Reparent);
    task.parent = &newParent.node();
    task.child = &child.element();
    m_taskQueue.append(WTFMove(task));
}

void HTMLConstructionSite::insertAlreadyParsedChild(HTMLStackItem& newParent, HTMLElementStack::ElementRecord& child)
{
    HTMLConstructionSiteTask task(HTMLConstructionSiteTask::InsertAlreadyParsedChild);
    if (causesFosterParenting(newParent)) {
        findFosterSite(task);
        ASSERT(task.parent);
    } else
        task.parent = &newParent.node();
    task.child = &child.element();
    m_taskQueue.append(WTFMove(task));
}

void HTMLConstructionSite::takeAllChildrenAndReparent(HTMLStackItem& newParent, HTMLElementStack::ElementRecord& oldParent)
{
    HTMLConstructionSiteTask task(HTMLConstructionSiteTask::TakeAllChildrenAndReparent);
    task.parent = &newParent.node();
    task.child = &oldParent.node();
    m_taskQueue.append(WTFMove(task));
}

Ref<Element> HTMLConstructionSite::createElement(AtomicHTMLToken& token, const AtomicString& namespaceURI)
{
    QualifiedName tagName(nullAtom(), token.name(), namespaceURI);
    auto element = ownerDocumentForCurrentNode().createElement(tagName, true);
    setAttributes(element, token, m_parserContentPolicy);
    return element;
}

inline Document& HTMLConstructionSite::ownerDocumentForCurrentNode()
{
    if (is<HTMLTemplateElement>(currentNode()))
        return downcast<HTMLTemplateElement>(currentNode()).content().document();
    return currentNode().document();
}

RefPtr<Element> HTMLConstructionSite::createHTMLElementOrFindCustomElementInterface(AtomicHTMLToken& token, JSCustomElementInterface** customElementInterface)
{
    auto& localName = token.name();
    // FIXME: This can't use HTMLConstructionSite::createElement because we
    // have to pass the current form element.  We should rework form association
    // to occur after construction to allow better code sharing here.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tree-construction.html#create-an-element-for-the-token
    Document& ownerDocument = ownerDocumentForCurrentNode();
    bool insideTemplateElement = !ownerDocument.frame();
    RefPtr<Element> element = HTMLElementFactory::createKnownElement(localName, ownerDocument, insideTemplateElement ? nullptr : form(), true);
    if (UNLIKELY(!element)) {
        auto* window = ownerDocument.domWindow();
        if (customElementInterface && window) {
            auto* registry = window->customElementRegistry();
            if (UNLIKELY(registry)) {
                if (auto* elementInterface = registry->findInterface(localName)) {
                    *customElementInterface = elementInterface;
                    return nullptr;
                }
            }
        }

        QualifiedName qualifiedName(nullAtom(), localName, xhtmlNamespaceURI);
        if (Document::validateCustomElementName(localName) == CustomElementNameValidationStatus::Valid) {
            element = HTMLElement::create(qualifiedName, ownerDocument);
            element->setIsCustomElementUpgradeCandidate();
        } else
            element = HTMLUnknownElement::create(qualifiedName, ownerDocument);
    }
    ASSERT(element);

    // FIXME: This is a hack to connect images to pictures before the image has
    // been inserted into the document. It can be removed once asynchronous image
    // loading is working.
    if (is<HTMLPictureElement>(currentNode()) && is<HTMLImageElement>(*element))
        downcast<HTMLImageElement>(*element).setPictureElement(&downcast<HTMLPictureElement>(currentNode()));

    setAttributes(*element, token, m_parserContentPolicy);
    ASSERT(element->isHTMLElement());
    return element;
}

Ref<Element> HTMLConstructionSite::createHTMLElement(AtomicHTMLToken& token)
{
    RefPtr<Element> element = createHTMLElementOrFindCustomElementInterface(token, nullptr);
    ASSERT(element);
    return element.releaseNonNull();
}

Ref<HTMLStackItem> HTMLConstructionSite::createElementFromSavedToken(HTMLStackItem& item)
{
    // NOTE: Moving from item -> token -> item copies the Attribute vector twice!
    AtomicHTMLToken fakeToken(HTMLToken::StartTag, item.localName(), Vector<Attribute>(item.attributes()));
    ASSERT(item.namespaceURI() == HTMLNames::xhtmlNamespaceURI);
    ASSERT(isFormattingTag(item.localName()));
    return HTMLStackItem::create(createHTMLElement(fakeToken), WTFMove(fakeToken), item.namespaceURI());
}

std::optional<unsigned> HTMLConstructionSite::indexOfFirstUnopenFormattingElement() const
{
    if (m_activeFormattingElements.isEmpty())
        return std::nullopt;
    unsigned index = m_activeFormattingElements.size();
    do {
        --index;
        const auto& entry = m_activeFormattingElements.at(index);
        if (entry.isMarker() || m_openElements.contains(entry.element())) {
            unsigned firstUnopenElementIndex = index + 1;
            return firstUnopenElementIndex < m_activeFormattingElements.size() ? firstUnopenElementIndex : std::optional<unsigned>(std::nullopt);
        }
    } while (index);

    return index;
}

void HTMLConstructionSite::reconstructTheActiveFormattingElements()
{
    std::optional<unsigned> firstUnopenElementIndex = indexOfFirstUnopenFormattingElement();
    if (!firstUnopenElementIndex)
        return;

    ASSERT(firstUnopenElementIndex.value() < m_activeFormattingElements.size());
    for (unsigned unopenEntryIndex = firstUnopenElementIndex.value(); unopenEntryIndex < m_activeFormattingElements.size(); ++unopenEntryIndex) {
        auto& unopenedEntry = m_activeFormattingElements.at(unopenEntryIndex);
        ASSERT(unopenedEntry.stackItem());
        auto reconstructed = createElementFromSavedToken(*unopenedEntry.stackItem());
        attachLater(currentNode(), reconstructed->node());
        m_openElements.push(reconstructed.copyRef());
        unopenedEntry.replaceElement(WTFMove(reconstructed));
    }
}

void HTMLConstructionSite::generateImpliedEndTagsWithExclusion(const AtomicString& tagName)
{
    while (hasImpliedEndTag(currentStackItem()) && !currentStackItem().matchesHTMLTag(tagName))
        m_openElements.pop();
}

void HTMLConstructionSite::generateImpliedEndTags()
{
    while (hasImpliedEndTag(currentStackItem()))
        m_openElements.pop();
}

void HTMLConstructionSite::findFosterSite(HTMLConstructionSiteTask& task)
{
    // When a node is to be foster parented, the last template element with no table element is below it in the stack of open elements is the foster parent element (NOT the template's parent!)
    auto* lastTemplateElement = m_openElements.topmost(templateTag.localName());
    if (lastTemplateElement && !m_openElements.inTableScope(tableTag)) {
        task.parent = &lastTemplateElement->element();
        return;
    }

    if (auto* lastTableElementRecord = m_openElements.topmost(tableTag.localName())) {
        auto& lastTableElement = lastTableElementRecord->element();
        auto* parent = lastTableElement.parentNode();
        // When parsing HTML fragments, we skip step 4.2 ("Let root be a new html element with no attributes") for efficiency,
        // and instead use the DocumentFragment as a root node. So we must treat the root node (DocumentFragment) as if it is a html element here.
        bool parentCanBeFosterParent = parent && (parent->isElementNode() || (m_isParsingFragment && parent == &m_openElements.rootNode()));
        parentCanBeFosterParent = parentCanBeFosterParent || (is<DocumentFragment>(parent) && downcast<DocumentFragment>(parent)->isTemplateContent());
        if (parentCanBeFosterParent) {
            task.parent = parent;
            task.nextChild = &lastTableElement;
            return;
        }
        task.parent = &lastTableElementRecord->next()->element();
        return;
    }
    // Fragment case
    task.parent = &m_openElements.rootNode(); // DocumentFragment
}

bool HTMLConstructionSite::shouldFosterParent() const
{
    return m_redirectAttachToFosterParent && causesFosterParenting(currentStackItem());
}

void HTMLConstructionSite::fosterParent(Ref<Node>&& node)
{
    HTMLConstructionSiteTask task(HTMLConstructionSiteTask::Insert);
    findFosterSite(task);
    task.child = WTFMove(node);
    ASSERT(task.parent);

    m_taskQueue.append(WTFMove(task));
}

}
