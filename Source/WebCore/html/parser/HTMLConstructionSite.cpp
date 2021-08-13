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

static inline void setAttributes(Element& element, AtomHTMLToken& token, ParserContentPolicy parserContentPolicy)
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

    if (auto parent = makeRefPtr(task.child->parentNode()))
        parent->parserRemoveChild(*task.child);

    if (task.child->parentNode())
        return;

    task.parent->parserAppendChild(*task.child);
}

static inline void executeInsertAlreadyParsedChildTask(HTMLConstructionSiteTask& task)
{
    ASSERT(task.operation == HTMLConstructionSiteTask::InsertAlreadyParsedChild);

    if (RefPtr<ContainerNode> parent = task.child->parentNode())
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

    auto furthestBlock = makeRefPtr(task.oldParent());
    task.parent->takeAllChildrenFrom(furthestBlock.get());

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
    ASSERT(scriptingContentIsAllowed(m_parserContentPolicy) || !is<Element>(child) || !isScriptElement(downcast<Element>(child.get())));
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
    , m_whitespaceCache(document.whitespaceCache())
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
    , m_whitespaceCache(fragment.document().whitespaceCache())
{
    ASSERT(m_document.isHTMLDocument() || m_document.isXHTMLDocument());
}

HTMLConstructionSite::~HTMLConstructionSite() = default;

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

    if (auto frame = makeRefPtr(m_document.frame()))
        frame->injectUserScripts(UserScriptInjectionTime::DocumentStart);
}

void HTMLConstructionSite::insertHTMLHtmlStartTagBeforeHTML(AtomHTMLToken&& token)
{
    auto element = HTMLHtmlElement::create(m_document);
    setAttributes(element, token, m_parserContentPolicy);
    attachLater(m_attachmentRoot, element.copyRef());
    m_openElements.pushHTMLHtmlElement(HTMLStackItem::create(element.copyRef(), WTFMove(token)));

    executeQueuedTasks();
    element->insertedByParser();
    dispatchDocumentElementAvailableIfNeeded();
}

void HTMLConstructionSite::mergeAttributesFromTokenIntoElement(AtomHTMLToken&& token, Element& element)
{
    if (token.attributes().isEmpty())
        return;

    for (auto& tokenAttribute : token.attributes()) {
        if (!element.elementData() || !element.findAttributeByName(tokenAttribute.name()))
            element.setAttribute(tokenAttribute.name(), tokenAttribute.value());
    }
}

void HTMLConstructionSite::insertHTMLHtmlStartTagInBody(AtomHTMLToken&& token)
{
    // Fragments do not have a root HTML element, so any additional HTML elements
    // encountered during fragment parsing should be ignored.
    if (m_isParsingFragment)
        return;

    mergeAttributesFromTokenIntoElement(WTFMove(token), m_openElements.htmlElement());
}

void HTMLConstructionSite::insertHTMLBodyStartTagInBody(AtomHTMLToken&& token)
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
        || startsWithLettersIgnoringASCIICase(publicId, "+//silmaril//dtd html pro v0r11 19970101//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//advasoft ltd//dtd html 3.0 aswedit + extensions//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//as//dtd html 3.0 aswedit + extensions//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 2.0 level 1//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 2.0 level 2//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 2.0 strict level 1//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 2.0 strict level 2//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 2.0 strict//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 2.0//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 2.1e//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 3.0//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 3.2 final//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 3.2//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 3//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html level 0//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html level 1//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html level 2//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html level 3//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html strict level 0//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html strict level 1//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html strict level 2//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html strict level 3//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html strict//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//metrius//dtd metrius presentational//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//microsoft//dtd internet explorer 2.0 html strict//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//microsoft//dtd internet explorer 2.0 html//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//microsoft//dtd internet explorer 2.0 tables//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//microsoft//dtd internet explorer 3.0 html strict//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//microsoft//dtd internet explorer 3.0 html//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//microsoft//dtd internet explorer 3.0 tables//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//netscape comm. corp.//dtd html//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//netscape comm. corp.//dtd strict html//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//o'reilly and associates//dtd html 2.0//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//o'reilly and associates//dtd html extended 1.0//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//o'reilly and associates//dtd html extended relaxed 1.0//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//softquad software//dtd hotmetal pro 6.0::19990601::extensions to html 4.0//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//softquad//dtd hotmetal pro 4.0::19971010::extensions to html 4.0//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//spyglass//dtd html 2.0 extended//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//sq//dtd html 2.0 hotmetal + extensions//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//sun microsystems corp.//dtd hotjava html//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//sun microsystems corp.//dtd hotjava strict html//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 3 1995-03-24//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 3.2 draft//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 3.2 final//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 3.2//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 3.2s draft//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 4.0 frameset//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 4.0 transitional//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html experimental 19960712//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html experimental 970421//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd w3 html//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3o//dtd w3 html 3.0//")
        || equalLettersIgnoringASCIICase(publicId, "-//w3o//dtd w3 html strict 3.0//en//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//webtechs//dtd mozilla html 2.0//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//webtechs//dtd mozilla html//")
        || equalLettersIgnoringASCIICase(publicId, "-/w3c/dtd html 4.0 transitional/en")
        || equalLettersIgnoringASCIICase(publicId, "html")
        || equalLettersIgnoringASCIICase(systemId, "http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd")
        || (systemId.isEmpty() && startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 4.01 frameset//"))
        || (systemId.isEmpty() && startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 4.01 transitional//"))) {
        setCompatibilityMode(DocumentCompatibilityMode::QuirksMode);
        return;
    }

    // Check for Limited Quirks Mode.
    if (startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd xhtml 1.0 frameset//")
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd xhtml 1.0 transitional//")
        || (!systemId.isEmpty() && startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 4.01 frameset//"))
        || (!systemId.isEmpty() && startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 4.01 transitional//"))) {
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

void HTMLConstructionSite::insertDoctype(AtomHTMLToken&& token)
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

void HTMLConstructionSite::insertComment(AtomHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    attachLater(currentNode(), Comment::create(ownerDocumentForCurrentNode(), token.comment()));
}

void HTMLConstructionSite::insertCommentOnDocument(AtomHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    attachLater(m_attachmentRoot, Comment::create(m_document, token.comment()));
}

void HTMLConstructionSite::insertCommentOnHTMLHtmlElement(AtomHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::Comment);
    ContainerNode& parent = m_openElements.rootNode();
    attachLater(parent, Comment::create(parent.document(), token.comment()));
}

void HTMLConstructionSite::insertHTMLHeadElement(AtomHTMLToken&& token)
{
    ASSERT(!shouldFosterParent());
    m_head = HTMLStackItem::create(createHTMLElement(token), WTFMove(token));
    attachLater(currentNode(), m_head->element());
    m_openElements.pushHTMLHeadElement(*m_head);
}

void HTMLConstructionSite::insertHTMLBodyElement(AtomHTMLToken&& token)
{
    ASSERT(!shouldFosterParent());
    auto body = createHTMLElement(token);
    attachLater(currentNode(), body.copyRef());
    m_openElements.pushHTMLBodyElement(HTMLStackItem::create(WTFMove(body), WTFMove(token)));
}

void HTMLConstructionSite::insertHTMLFormElement(AtomHTMLToken&& token, bool isDemoted)
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

void HTMLConstructionSite::insertHTMLElement(AtomHTMLToken&& token)
{
    auto element = createHTMLElement(token);
    attachLater(currentNode(), element.copyRef());
    m_openElements.push(HTMLStackItem::create(WTFMove(element), WTFMove(token)));
}

std::unique_ptr<CustomElementConstructionData> HTMLConstructionSite::insertHTMLElementOrFindCustomElementInterface(AtomHTMLToken&& token)
{
    JSCustomElementInterface* elementInterface = nullptr;
    RefPtr<Element> element = createHTMLElementOrFindCustomElementInterface(token, &elementInterface);
    if (UNLIKELY(elementInterface))
        return makeUnique<CustomElementConstructionData>(*elementInterface, token.name(), WTFMove(token.attributes()));
    attachLater(currentNode(), *element);
    m_openElements.push(HTMLStackItem::create(element.releaseNonNull(), WTFMove(token)));
    return nullptr;
}

void HTMLConstructionSite::insertCustomElement(Ref<Element>&& element, const AtomString& localName, Vector<Attribute>&& attributes)
{
    setAttributes(element, attributes, m_parserContentPolicy);
    attachLater(currentNode(), element.copyRef());
    m_openElements.push(HTMLStackItem::create(WTFMove(element), localName, WTFMove(attributes)));
    executeQueuedTasks();
}

void HTMLConstructionSite::insertSelfClosingHTMLElement(AtomHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::StartTag);
    // Normally HTMLElementStack is responsible for calling finishParsingChildren,
    // but self-closing elements are never in the element stack so the stack
    // doesn't get a chance to tell them that we're done parsing their children.
    attachLater(currentNode(), createHTMLElement(token), true);
    // FIXME: Do we want to acknowledge the token's self-closing flag?
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#acknowledge-self-closing-flag
}

void HTMLConstructionSite::insertFormattingElement(AtomHTMLToken&& token)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#the-stack-of-open-elements
    // Possible active formatting elements include:
    // a, b, big, code, em, font, i, nobr, s, small, strike, strong, tt, and u.
    ASSERT(isFormattingTag(token.name()));
    insertHTMLElement(WTFMove(token));
    m_activeFormattingElements.append(currentStackItem());
}

void HTMLConstructionSite::insertScriptElement(AtomHTMLToken&& token)
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

void HTMLConstructionSite::insertForeignElement(AtomHTMLToken&& token, const AtomString& namespaceURI)
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

    unsigned currentPosition = 0;
    unsigned lengthLimit = shouldUseLengthLimit(*task.parent) ? Text::defaultLengthLimit : std::numeric_limits<unsigned>::max();

    // FIXME: Splitting text nodes into smaller chunks contradicts HTML5 spec, but is currently necessary
    // for performance, see <https://bugs.webkit.org/show_bug.cgi?id=55898>.

    RefPtr<Node> previousChild = task.nextChild ? task.nextChild->previousSibling() : task.parent->lastChild();
    if (is<Text>(previousChild)) {
        // FIXME: We're only supposed to append to this text node if it
        // was the last text node inserted by the parser.
        currentPosition = downcast<Text>(*previousChild).parserAppendData(characters, 0, lengthLimit);
    }

    while (currentPosition < characters.length()) {
        AtomString charactersAtom = m_whitespaceCache.lookup(characters, whitespaceMode);
        auto textNode = Text::createWithLengthLimit(task.parent->document(), charactersAtom.isNull() ? characters : charactersAtom.string(), currentPosition, lengthLimit);
        // If we have a whole string of unbreakable characters the above could lead to an infinite loop. Exceeding the length limit is the lesser evil.
        if (!textNode->length()) {
            String substring = characters.substring(currentPosition);
            AtomString substringAtom = m_whitespaceCache.lookup(substring, whitespaceMode);
            textNode = Text::create(task.parent->document(), substringAtom.isNull() ? substring : substringAtom.string());
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

Ref<Element> HTMLConstructionSite::createElement(AtomHTMLToken& token, const AtomString& namespaceURI)
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

static inline JSCustomElementInterface* findCustomElementInterface(Document& ownerDocument, const AtomString& localName)
{
    auto* window = ownerDocument.domWindow();
    if (!window)
        return nullptr;

    auto* registry = window->customElementRegistry();
    if (LIKELY(!registry))
        return nullptr;

    return registry->findInterface(localName);
}

RefPtr<Element> HTMLConstructionSite::createHTMLElementOrFindCustomElementInterface(AtomHTMLToken& token, JSCustomElementInterface** customElementInterface)
{
    auto& localName = token.name();
    // FIXME: This can't use HTMLConstructionSite::createElement because we
    // have to pass the current form element.  We should rework form association
    // to occur after construction to allow better code sharing here.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tree-construction.html#create-an-element-for-the-token
    Document& ownerDocument = ownerDocumentForCurrentNode();
    bool insideTemplateElement = !ownerDocument.frame();
    auto element = HTMLElementFactory::createKnownElement(localName, ownerDocument, insideTemplateElement ? nullptr : form(), true);
    if (UNLIKELY(!element)) {
        if (auto* elementInterface = findCustomElementInterface(ownerDocument, localName)) {
            if (!m_isParsingFragment) {
                *customElementInterface = elementInterface;
                return nullptr;
            }
            element = HTMLElement::create(QualifiedName { nullAtom(), localName, xhtmlNamespaceURI }, ownerDocument);
            element->setIsCustomElementUpgradeCandidate();
            element->enqueueToUpgrade(*elementInterface);
        } else {
            QualifiedName qualifiedName { nullAtom(), localName, xhtmlNamespaceURI };
            if (Document::validateCustomElementName(localName) == CustomElementNameValidationStatus::Valid) {
                element = HTMLElement::create(qualifiedName, ownerDocument);
                element->setIsCustomElementUpgradeCandidate();
            } else
                element = HTMLUnknownElement::create(qualifiedName, ownerDocument);
        }
    }
    ASSERT(element);

    // FIXME: This is a hack to connect images to pictures before the image has
    // been inserted into the document. It can be removed once asynchronous image
    // loading is working. When this hack is removed, the assertion just before
    // the setPictureElement() call in HTMLImageElement::insertedIntoAncestor
    // can be simplified.
    if (is<HTMLPictureElement>(currentNode()) && is<HTMLImageElement>(*element))
        downcast<HTMLImageElement>(*element).setPictureElement(&downcast<HTMLPictureElement>(currentNode()));

    setAttributes(*element, token, m_parserContentPolicy);
    ASSERT(element->isHTMLElement());
    return element;
}

Ref<Element> HTMLConstructionSite::createHTMLElement(AtomHTMLToken& token)
{
    RefPtr<Element> element = createHTMLElementOrFindCustomElementInterface(token, nullptr);
    ASSERT(element);
    return element.releaseNonNull();
}

Ref<HTMLStackItem> HTMLConstructionSite::createElementFromSavedToken(HTMLStackItem& item)
{
    // NOTE: Moving from item -> token -> item copies the Attribute vector twice!
    AtomHTMLToken fakeToken(HTMLToken::StartTag, item.localName(), Vector<Attribute>(item.attributes()));
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

void HTMLConstructionSite::generateImpliedEndTagsWithExclusion(const AtomString& tagName)
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
    auto* lastTemplateElement = m_openElements.topmost(templateTag->localName());
    if (lastTemplateElement && !m_openElements.inTableScope(tableTag)) {
        task.parent = &lastTemplateElement->element();
        return;
    }

    if (auto* lastTableElementRecord = m_openElements.topmost(tableTag->localName())) {
        auto& lastTableElement = lastTableElementRecord->element();
        auto parent = makeRefPtr(lastTableElement.parentNode());
        // When parsing HTML fragments, we skip step 4.2 ("Let root be a new html element with no attributes") for efficiency,
        // and instead use the DocumentFragment as a root node. So we must treat the root node (DocumentFragment) as if it is a html element here.
        bool parentCanBeFosterParent = parent && (parent->isElementNode() || (m_isParsingFragment && parent == &m_openElements.rootNode()));
        parentCanBeFosterParent = parentCanBeFosterParent || (is<DocumentFragment>(parent) && downcast<DocumentFragment>(parent.get())->isTemplateContent());
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

// Compute a 64 bit code that represents a whitespace-only string's contents.
//
// The code format is a sequence of four pairs of an 8 bit whitespace character
// and an 8 bit count of that character. For example, 0x0A_02_20_08 represents
// two newlines followed by eight space characters.
//
// Returns 0 if any non-whitespace characters are found.
//
// Returns -1 if the code would overflow due to finding more than four
// whitespace character runs.
template<WhitespaceMode whitespaceMode>
uint64_t WhitespaceCache::codeForString(const String& string)
{
    ASSERT(whitespaceMode != NotAllWhitespace);
    ASSERT(string.is8Bit());
    ASSERT(!string.isEmpty());
    ASSERT(string.length() <= maximumCachedStringLength);
    static_assert(maximumCachedStringLength <= 0xFF, "Code format requires whitespace run length fit in one byte");

    auto startOfRun = string.characters8();

    if constexpr (whitespaceMode == WhitespaceUnknown) {
        if (!isHTMLSpace(*startOfRun))
            return 0;
    }

    LChar currentWhitespaceCharacter = *startOfRun;
    auto character = startOfRun + 1;
    auto end = startOfRun + string.length();

    uint64_t code = 0;
    int runsRemaining = 4;

    for (;;) {
        while (character != end && *character == currentWhitespaceCharacter)
            ++character;

        if constexpr (whitespaceMode == WhitespaceUnknown) {
            if (character != end && !isHTMLSpace(*character))
                return 0;
        }

        code <<= 16;
        code |= (currentWhitespaceCharacter << 8);
        code |= (character - startOfRun);

        if (character == end)
            return code;

        if (!--runsRemaining)
            return overflowWhitespaceCode;

        startOfRun = character;
        currentWhitespaceCharacter = *character;
        ++character;

        ASSERT(isHTMLSpace(currentWhitespaceCharacter));
    }

    return code;
}

AtomString WhitespaceCache::lookup(const String& string, WhitespaceMode whitespaceMode)
{
    if (whitespaceMode == NotAllWhitespace || !string.is8Bit() || string.isEmpty())
        return AtomString();

    size_t length = string.length();
    if (length > maximumCachedStringLength)
        return whitespaceMode == AllWhitespace || isAllWhitespace(string) ? AtomString(string) : AtomString();

    uint64_t code;
    if (whitespaceMode == AllWhitespace)
        code = codeForString<AllWhitespace>(string);
    else
        code = codeForString<WhitespaceUnknown>(string);

    if (!code)
        return AtomString();

    size_t lengthIndex = length - 1;
    if (m_codes[lengthIndex] == code) {
        ASSERT(m_atoms[m_indexes[lengthIndex]] == string);
        return m_atoms[m_indexes[lengthIndex]];
    }

    if (code == overflowWhitespaceCode)
        return AtomString(string);

    if (m_codes[lengthIndex]) {
        AtomString whitespaceAtom(string);
        m_codes[lengthIndex] = code;
        m_atoms[m_indexes[lengthIndex]] = whitespaceAtom;
        return whitespaceAtom;
    }

    AtomString whitespaceAtom(string);
    m_codes[lengthIndex] = code;
    m_indexes[lengthIndex] = m_atoms.size();
    m_atoms.append(whitespaceAtom);
    return whitespaceAtom;
}

}
