/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2011-2024 Apple Inc. All rights reserved.
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
#include "HTMLConstructionSite.h"

#include "Comment.h"
#include "CustomElementRegistry.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "HTMLElementFactory.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLImageElement.h"
#include "HTMLMaybeFormAssociatedCustomElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLPictureElement.h"
#include "HTMLScriptElement.h"
#include "HTMLTemplateElement.h"
#include "HTMLTreeBuilder.h"
#include "HTMLUnknownElement.h"
#include "JSCustomElementInterface.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "NodeName.h"
#include "NotImplemented.h"
#include "SVGElementInlines.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "Text.h"
#include <unicode/ubrk.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {

using namespace ElementNames;
using namespace HTMLNames;

enum class HasDuplicateAttribute : bool { No, Yes };
static inline void setAttributes(Element& element, Vector<Attribute>& attributes, HasDuplicateAttribute hasDuplicateAttribute, OptionSet<ParserContentPolicy> parserContentPolicy)
{
    if (!scriptingContentIsAllowed(parserContentPolicy))
        element.stripScriptingAttributes(attributes);
    element.parserSetAttributes(attributes.span());
    element.setHasDuplicateAttribute(hasDuplicateAttribute == HasDuplicateAttribute::Yes);
}

static inline void setAttributes(Element& element, AtomHTMLToken& token, OptionSet<ParserContentPolicy> parserContentPolicy)
{
    setAttributes(element, token.attributes(), token.hasDuplicateAttribute() ? HasDuplicateAttribute::Yes : HasDuplicateAttribute::No, parserContentPolicy);
}

static bool hasImpliedEndTag(const HTMLStackItem& item)
{
    switch (item.elementName()) {
    case HTML::dd:
    case HTML::dt:
    case HTML::li:
    case HTML::option:
    case HTML::optgroup:
    case HTML::p:
    case HTML::rb:
    case HTML::rp:
    case HTML::rt:
    case HTML::rtc:
        return true;
    default:
        return false;
    }
}

static bool shouldUseLengthLimit(const ContainerNode& node)
{
    auto* element = dynamicDowncast<Element>(node);
    if (!element)
        return true;

    switch (element->elementName()) {
    case HTML::script:
    case HTML::style:
    case SVG::script:
        return false;
    default:
        return true;
    }
}

static inline bool causesFosterParenting(const HTMLStackItem& item)
{
    switch (item.elementName()) {
    case HTML::table:
    case HTML::tbody:
    case HTML::tfoot:
    case HTML::thead:
    case HTML::tr:
        return true;
    default:
        return false;
    }
}

static inline void insert(HTMLConstructionSiteTask& task)
{
    if (auto templateElement = dynamicDowncast<HTMLTemplateElement>(task.parent)) {
        task.parent = &templateElement->fragmentForInsertion();
        task.nextChild = nullptr;
    }

    ASSERT(!task.child->parentNode());
    if (task.nextChild)
        task.parent->parserInsertBefore(task.protectedNonNullChild(), task.protectedNonNullNextChild());
    else
        task.parent->parserAppendChild(task.protectedNonNullChild());
}

static inline void executeInsertTask(HTMLConstructionSiteTask& task)
{
    ASSERT(task.operation == HTMLConstructionSiteTask::Insert);

    insert(task);

    RefPtr element = dynamicDowncast<Element>(task.child);
    if (!element)
        return;

    element->beginParsingChildren();
    if (task.selfClosing)
        element->finishParsingChildren();
}

static inline void executeReparentTask(HTMLConstructionSiteTask& task)
{
    ASSERT(task.operation == HTMLConstructionSiteTask::Reparent);
    ASSERT(!task.nextChild);

    if (RefPtr parent = task.child->parentNode())
        parent->parserRemoveChild(*task.child);

    if (task.child->parentNode() || task.child->contains(task.parent.get()))
        return;

    task.parent->parserAppendChild(task.protectedNonNullChild());
}

static inline void executeInsertAlreadyParsedChildTask(HTMLConstructionSiteTask& task)
{
    ASSERT(task.operation == HTMLConstructionSiteTask::InsertAlreadyParsedChild);

    if (RefPtr<ContainerNode> parent = task.child->parentNode())
        parent->parserRemoveChild(*task.child);

    if (task.child->parentNode() || task.child->contains(task.parent.get()))
        return;

    if (task.nextChild && task.nextChild->parentNode() != task.parent)
        return;

    insert(task);
}

static inline void executeTakeAllChildrenAndReparentTask(HTMLConstructionSiteTask& task)
{
    ASSERT(task.operation == HTMLConstructionSiteTask::TakeAllChildrenAndReparent);
    ASSERT(!task.nextChild);

    RefPtr furthestBlock = task.oldParent();
    task.parent->takeAllChildrenFrom(furthestBlock.get());

    RELEASE_ASSERT(!task.parent->parentNode());
    furthestBlock->parserAppendChild(task.protectedNonNullParent());
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

void HTMLConstructionSite::attachLater(Ref<ContainerNode>&& parent, Ref<Node>&& child, bool selfClosing)
{
    ASSERT(scriptingContentIsAllowed(m_parserContentPolicy) || !is<Element>(child) || !isScriptElement(downcast<Element>(child.get())));

    if (shouldFosterParent()) {
        fosterParent(WTFMove(child));
        return;
    }

    HTMLConstructionSiteTask task(HTMLConstructionSiteTask::Insert);
    task.parent = WTFMove(parent);
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

HTMLConstructionSite::HTMLConstructionSite(Document& document, OptionSet<ParserContentPolicy> parserContentPolicy, unsigned maximumDOMTreeDepth)
    : m_document(document)
    , m_attachmentRoot(document)
    , m_parserContentPolicy(parserContentPolicy)
    , m_isParsingFragment(false)
    , m_redirectAttachToFosterParent(false)
    , m_maximumDOMTreeDepth(maximumDOMTreeDepth)
    , m_inQuirksMode(document.inQuirksMode())
{
}

HTMLConstructionSite::HTMLConstructionSite(DocumentFragment& fragment, OptionSet<ParserContentPolicy> parserContentPolicy, unsigned maximumDOMTreeDepth)
    : m_document(fragment.document())
    , m_attachmentRoot(fragment)
    , m_parserContentPolicy(parserContentPolicy)
    , m_isParsingFragment(true)
    , m_redirectAttachToFosterParent(false)
    , m_maximumDOMTreeDepth(maximumDOMTreeDepth)
    , m_inQuirksMode(fragment.document().inQuirksMode())
{
}

HTMLConstructionSite::~HTMLConstructionSite() = default;

Ref<Document> HTMLConstructionSite::protectedDocument() const
{
    return m_document.get();
}

Ref<ContainerNode> HTMLConstructionSite::protectedAttachmentRoot() const
{
    return m_attachmentRoot.get();
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

    if (RefPtr frame = document().frame())
        frame->injectUserScripts(UserScriptInjectionTime::DocumentStart);
}

void HTMLConstructionSite::insertHTMLHtmlStartTagBeforeHTML(AtomHTMLToken&& token)
{
    auto element = HTMLHtmlElement::create(protectedDocument());
    setAttributes(element, token, m_parserContentPolicy);
    attachLater(protectedAttachmentRoot(), element.copyRef());
    m_openElements.pushHTMLHtmlElement(HTMLStackItem(element.copyRef(), WTFMove(token)));

    executeQueuedTasks();
    dispatchDocumentElementAvailableIfNeeded();
}

void HTMLConstructionSite::mergeAttributesFromTokenIntoElement(AtomHTMLToken&& token, Element& element)
{
    if (!scriptingContentIsAllowed(m_parserContentPolicy))
        element.stripScriptingAttributes(token.attributes());

    for (auto& tokenAttribute : token.attributes()) {
        auto& attributeName = tokenAttribute.name();
        if (UNLIKELY(attributeName == nonceAttr)) {
            if (element.hasAttributeWithoutSynchronization(nonceAttr) || !element.nonce().isEmpty())
                continue;
            // Make sure the nonce attribute remains hidden.
            element.setAttribute(attributeName, tokenAttribute.value());
            element.hideNonce();
        } else
            element.setAttributeWithoutOverwriting(attributeName, tokenAttribute.value());
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
    if (document().isSrcdocDocument())
        return;
    setCompatibilityMode(DocumentCompatibilityMode::QuirksMode);
}

void HTMLConstructionSite::setCompatibilityMode(DocumentCompatibilityMode mode)
{
    m_inQuirksMode = (mode == DocumentCompatibilityMode::QuirksMode);
    protectedDocument()->setCompatibilityMode(mode);
}

void HTMLConstructionSite::setCompatibilityModeFromDoctype(const AtomString& name, const String& publicId, const String& systemId)
{
    // There are three possible compatibility modes:
    // Quirks - quirks mode emulates WinIE and NS4. CSS parsing is also relaxed in this mode, e.g., unit types can
    // be omitted from numbers.
    // Limited Quirks - This mode is identical to no-quirks mode except for its treatment of line-height in the inline box model.  
    // No Quirks - no quirks apply. Web pages will obey the specifications to the letter.

    bool isNameHTML = name == HTMLNames::htmlTag->localName();
    if (LIKELY((isNameHTML && publicId.isEmpty() && systemId.isEmpty()) || document().isSrcdocDocument())) {
        setCompatibilityMode(DocumentCompatibilityMode::NoQuirksMode);
        return;
    }

    // Check for Quirks Mode.
    if (!isNameHTML
        || startsWithLettersIgnoringASCIICase(publicId, "+//silmaril//dtd html pro v0r11 19970101//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//advasoft ltd//dtd html 3.0 aswedit + extensions//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//as//dtd html 3.0 aswedit + extensions//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 2.0 level 1//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 2.0 level 2//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 2.0 strict level 1//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 2.0 strict level 2//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 2.0 strict//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 2.0//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 2.1e//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 3.0//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 3.2 final//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 3.2//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html 3//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html level 0//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html level 1//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html level 2//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html level 3//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html strict level 0//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html strict level 1//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html strict level 2//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html strict level 3//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html strict//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//ietf//dtd html//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//metrius//dtd metrius presentational//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//microsoft//dtd internet explorer 2.0 html strict//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//microsoft//dtd internet explorer 2.0 html//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//microsoft//dtd internet explorer 2.0 tables//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//microsoft//dtd internet explorer 3.0 html strict//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//microsoft//dtd internet explorer 3.0 html//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//microsoft//dtd internet explorer 3.0 tables//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//netscape comm. corp.//dtd html//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//netscape comm. corp.//dtd strict html//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//o'reilly and associates//dtd html 2.0//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//o'reilly and associates//dtd html extended 1.0//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//o'reilly and associates//dtd html extended relaxed 1.0//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//softquad software//dtd hotmetal pro 6.0::19990601::extensions to html 4.0//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//softquad//dtd hotmetal pro 4.0::19971010::extensions to html 4.0//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//spyglass//dtd html 2.0 extended//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//sq//dtd html 2.0 hotmetal + extensions//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//sun microsystems corp.//dtd hotjava html//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//sun microsystems corp.//dtd hotjava strict html//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 3 1995-03-24//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 3.2 draft//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 3.2 final//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 3.2//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 3.2s draft//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 4.0 frameset//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 4.0 transitional//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html experimental 19960712//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html experimental 970421//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd w3 html//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3o//dtd w3 html 3.0//"_s)
        || equalLettersIgnoringASCIICase(publicId, "-//w3o//dtd w3 html strict 3.0//en//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//webtechs//dtd mozilla html 2.0//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//webtechs//dtd mozilla html//"_s)
        || equalLettersIgnoringASCIICase(publicId, "-/w3c/dtd html 4.0 transitional/en"_s)
        || equalLettersIgnoringASCIICase(publicId, "html"_s)
        || equalLettersIgnoringASCIICase(systemId, "http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd"_s)
        || (systemId.isEmpty() && startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 4.01 frameset//"_s))
        || (systemId.isEmpty() && startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 4.01 transitional//"_s))) {
        setCompatibilityMode(DocumentCompatibilityMode::QuirksMode);
        return;
    }

    // Check for Limited Quirks Mode.
    if (startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd xhtml 1.0 frameset//"_s)
        || startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd xhtml 1.0 transitional//"_s)
        || (!systemId.isEmpty() && startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 4.01 frameset//"_s))
        || (!systemId.isEmpty() && startsWithLettersIgnoringASCIICase(publicId, "-//w3c//dtd html 4.01 transitional//"_s))) {
        setCompatibilityMode(DocumentCompatibilityMode::LimitedQuirksMode);
        return;
    }

    // Otherwise we are No Quirks Mode.
    setCompatibilityMode(DocumentCompatibilityMode::NoQuirksMode);
}

void HTMLConstructionSite::finishedParsing()
{
    protectedDocument()->finishedParsing();
}

void HTMLConstructionSite::insertDoctype(AtomHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::Type::DOCTYPE);

    String publicId = token.publicIdentifier();
    String systemId = token.systemIdentifier();

    auto document = protectedDocument();
    attachLater(protectedAttachmentRoot(), DocumentType::create(document, token.name(), publicId, systemId));

    // DOCTYPE nodes are only processed when parsing fragments w/o contextElements, which
    // never occurs.  However, if we ever chose to support such, this code is subtly wrong,
    // because context-less fragments can determine their own quirks mode, and thus change
    // parsing rules (like <p> inside <table>).  For now we ASSERT that we never hit this code
    // in a fragment, as changing the owning document's compatibility mode would be wrong.
    ASSERT(!m_isParsingFragment);
    if (m_isParsingFragment)
        return;

    if (token.forceQuirks() && !document->isSrcdocDocument())
        setCompatibilityMode(DocumentCompatibilityMode::QuirksMode);
    else
        setCompatibilityModeFromDoctype(token.name(), publicId, systemId);
}

void HTMLConstructionSite::insertComment(AtomHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::Type::Comment);
    attachLater(protectedCurrentNode(), Comment::create(protectedOwnerDocumentForCurrentNode(), WTFMove(token.comment())));
}

void HTMLConstructionSite::insertCommentOnDocument(AtomHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::Type::Comment);
    attachLater(protectedAttachmentRoot(), Comment::create(protectedDocument(), WTFMove(token.comment())));
}

void HTMLConstructionSite::insertCommentOnHTMLHtmlElement(AtomHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::Type::Comment);
    Ref parent = m_openElements.rootNode();
    Ref parentDocument = parent->document();
    attachLater(WTFMove(parent), Comment::create(parentDocument, WTFMove(token.comment())));
}

void HTMLConstructionSite::insertHTMLHeadElement(AtomHTMLToken&& token)
{
    ASSERT(!shouldFosterParent());
    m_head = HTMLStackItem(createHTMLElement(token), WTFMove(token));
    attachLater(protectedCurrentNode(), m_head.element());
    m_openElements.pushHTMLHeadElement(HTMLStackItem(m_head));
}

void HTMLConstructionSite::insertHTMLBodyElement(AtomHTMLToken&& token)
{
    ASSERT(!shouldFosterParent());
    auto body = createHTMLElement(token);
    attachLater(protectedCurrentNode(), body.copyRef());
    m_openElements.pushHTMLBodyElement(HTMLStackItem(WTFMove(body), WTFMove(token)));
}

void HTMLConstructionSite::insertHTMLFormElement(AtomHTMLToken&& token)
{
    auto formElement = downcast<HTMLFormElement>(createHTMLElement(token));
    // If there is no template element on the stack of open elements, set the
    // form element pointer to point to the element created.
    if (!openElements().hasTemplateInHTMLScope())
        m_form = formElement.ptr();
    attachLater(protectedCurrentNode(), formElement.copyRef());
    m_openElements.push(HTMLStackItem(WTFMove(formElement), WTFMove(token)));
}

void HTMLConstructionSite::insertHTMLElement(AtomHTMLToken&& token)
{
    auto element = createHTMLElement(token);
    attachLater(protectedCurrentNode(), element.copyRef());
    m_openElements.push(HTMLStackItem(WTFMove(element), WTFMove(token)));
}

void HTMLConstructionSite::insertHTMLTemplateElement(AtomHTMLToken&& token)
{
    if (m_parserContentPolicy.contains(ParserContentPolicy::AllowDeclarativeShadowRoots)) {
        std::optional<ShadowRootMode> mode;
        auto delegatesFocus = ShadowRootDelegatesFocus::No;
        auto clonable = ShadowRootClonable::No;
        auto serializable = ShadowRootSerializable::No;
        for (auto& attribute : token.attributes()) {
            if (attribute.name() == HTMLNames::shadowrootmodeAttr) {
                if (equalLettersIgnoringASCIICase(attribute.value(), "closed"_s))
                    mode = ShadowRootMode::Closed;
                else if (equalLettersIgnoringASCIICase(attribute.value(), "open"_s))
                    mode = ShadowRootMode::Open;
            } else if (attribute.name() == HTMLNames::shadowrootdelegatesfocusAttr)
                delegatesFocus = ShadowRootDelegatesFocus::Yes;
            else if (attribute.name() == HTMLNames::shadowrootclonableAttr)
                clonable = ShadowRootClonable::Yes;
            else if (attribute.name() == HTMLNames::shadowrootserializableAttr)
                serializable = ShadowRootSerializable::Yes;
        }
        if (mode && is<Element>(currentNode())) {
            auto exceptionOrShadowRoot = currentElement().attachDeclarativeShadow(*mode, delegatesFocus, clonable, serializable);
            if (!exceptionOrShadowRoot.hasException()) {
                Ref shadowRoot = exceptionOrShadowRoot.releaseReturnValue();
                auto element = createHTMLElement(token);
                downcast<HTMLTemplateElement>(element.get()).setDeclarativeShadowRoot(shadowRoot);
                m_openElements.push(HTMLStackItem(WTFMove(element), WTFMove(token)));
                return;
            }
        }
    }
    insertHTMLElement(WTFMove(token));
}

std::unique_ptr<CustomElementConstructionData> HTMLConstructionSite::insertHTMLElementOrFindCustomElementInterface(AtomHTMLToken&& token)
{
    auto [element, elementInterface, registry] = createHTMLElementOrFindCustomElementInterface(token);
    if (UNLIKELY(elementInterface)) {
        RELEASE_ASSERT(registry);
        return makeUnique<CustomElementConstructionData>(elementInterface.releaseNonNull(), registry.releaseNonNull(), token.name(), WTFMove(token.attributes()));
    }
    attachLater(protectedCurrentNode(), *element);
    m_openElements.push(HTMLStackItem(element.releaseNonNull(), WTFMove(token)));
    return nullptr;
}

void HTMLConstructionSite::insertCustomElement(Ref<Element>&& element, Vector<Attribute>&& attributes)
{
    setAttributes(element, attributes, HasDuplicateAttribute::No, m_parserContentPolicy);
    attachLater(protectedCurrentNode(), element.copyRef());
    m_openElements.push(HTMLStackItem(WTFMove(element), WTFMove(attributes)));
    executeQueuedTasks();
}

void HTMLConstructionSite::insertSelfClosingHTMLElement(AtomHTMLToken&& token)
{
    ASSERT(token.type() == HTMLToken::Type::StartTag);
    // Normally HTMLElementStack is responsible for calling finishParsingChildren,
    // but self-closing elements are never in the element stack so the stack
    // doesn't get a chance to tell them that we're done parsing their children.
    attachLater(protectedCurrentNode(), createHTMLElement(token), true);
    // FIXME: Do we want to acknowledge the token's self-closing flag?
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#acknowledge-self-closing-flag
}

void HTMLConstructionSite::insertFormattingElement(AtomHTMLToken&& token)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#the-stack-of-open-elements
    // Possible active formatting elements include:
    // a, b, big, code, em, font, i, nobr, s, small, strike, strong, tt, and u.
    ASSERT(isFormattingTag(token.tagName()));
    insertHTMLElement(WTFMove(token));
    m_activeFormattingElements.append(HTMLStackItem(currentStackItem()));
}

void HTMLConstructionSite::insertScriptElement(AtomHTMLToken&& token)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/scripting-1.html#already-started
    // http://html5.org/specs/dom-parsing.html#dom-range-createcontextualfragment
    // For createContextualFragment, the specifications say to mark it parser-inserted and already-started and later unmark them.
    // However, we short circuit that logic to avoid the subtree traversal to find script elements since scripts can never see
    // those flags or effects thereof.
    const bool parserInserted = !m_parserContentPolicy.contains(ParserContentPolicy::DoNotMarkAlreadyStarted);
    const bool alreadyStarted = m_isParsingFragment && parserInserted;
    auto element = HTMLScriptElement::create(scriptTag, ownerDocumentForCurrentNode(), parserInserted, alreadyStarted);
    setAttributes(element, token, m_parserContentPolicy);
    if (scriptingContentIsAllowed(m_parserContentPolicy))
        attachLater(protectedCurrentNode(), element.copyRef());
    m_openElements.push(HTMLStackItem(WTFMove(element), WTFMove(token)));
}

void HTMLConstructionSite::insertForeignElement(AtomHTMLToken&& token, const AtomString& namespaceURI)
{
    ASSERT(token.type() == HTMLToken::Type::StartTag);
    notImplemented(); // parseError when xmlns or xmlns:xlink are wrong.

    auto element = createElement(token, namespaceURI);
    if (scriptingContentIsAllowed(m_parserContentPolicy) || !isScriptElement(element.get()))
        attachLater(protectedCurrentNode(), element.copyRef(), token.selfClosing());
    if (!token.selfClosing())
        m_openElements.push(HTMLStackItem(WTFMove(element), WTFMove(token)));
}

static NEVER_INLINE unsigned findBreakIndexSlow(const String& string, unsigned currentPosition, unsigned proposedBreakIndex)
{
    unsigned stringLength = string.length();
    // Check that we are not on an unbreakable boundary.
    // Some text break iterator implementations work best if the passed buffer is as small as possible,
    // see <https://bugs.webkit.org/show_bug.cgi?id=29092>.
    // We need at least two characters look-ahead to account for UTF-16 surrogates.
    unsigned breakSearchLength = std::min(proposedBreakIndex - currentPosition + 2, stringLength - currentPosition);
    NonSharedCharacterBreakIterator it(StringView(string).substring(currentPosition, breakSearchLength));

    unsigned stringLengthLimit = proposedBreakIndex - currentPosition;
    if (ubrk_isBoundary(it, stringLengthLimit))
        return proposedBreakIndex;

    unsigned breakIndexInSubstring = ubrk_preceding(it, stringLengthLimit);
    return currentPosition + breakIndexInSubstring;
}

static ALWAYS_INLINE unsigned findBreakIndex(const String& string, unsigned currentPosition, unsigned proposedBreakIndex)
{
    ASSERT(currentPosition < proposedBreakIndex);
    ASSERT(proposedBreakIndex <= string.length());

    if (LIKELY(proposedBreakIndex == string.length() || string.is8Bit()))
        return proposedBreakIndex;

    return findBreakIndexSlow(string, currentPosition, proposedBreakIndex);
}

void HTMLConstructionSite::insertTextNode(const String& characters)
{
    HTMLConstructionSiteTask task(HTMLConstructionSiteTask::Insert);
    task.parent = &currentNode();

    if (shouldFosterParent())
        findFosterSite(task);

    unsigned currentPosition = 0;
    unsigned lengthLimit = shouldUseLengthLimit(*task.parent) ? Text::defaultLengthLimit : std::numeric_limits<unsigned>::max();

    // FIXME: Splitting text nodes into smaller chunks contradicts HTML5 spec, but is currently necessary
    // for performance, see <https://bugs.webkit.org/show_bug.cgi?id=55898>.

    RefPtr<Node> previousChild;
    if (task.nextChild)
        previousChild = task.nextChild->previousSibling();
    else {
        if (auto templateParent = dynamicDowncast<HTMLTemplateElement>(task.parent.get()); UNLIKELY(templateParent)) {
            auto parentNode = templateParent->contentIfAvailable();
            previousChild = parentNode ? parentNode->lastChild() : nullptr;
        } else
            previousChild = task.parent->lastChild();
    }
    if (auto* previousTextChild = dynamicDowncast<Text>(previousChild.get()); previousTextChild && previousTextChild->length() < lengthLimit) {
        // FIXME: We're only supposed to append to this text node if it was the last text node inserted by the parser.
        unsigned proposedBreakIndex = std::min(characters.length(), lengthLimit - previousTextChild->length());
        if (unsigned breakIndex = findBreakIndex(characters, 0, proposedBreakIndex)) {
            previousTextChild->parserAppendData(StringView(characters).left(breakIndex));
            currentPosition = breakIndex;
        }
    }

    while (currentPosition < characters.length()) {
        unsigned proposedBreakIndex = std::min(currentPosition + lengthLimit, characters.length());
        unsigned breakIndex = findBreakIndex(characters, currentPosition, proposedBreakIndex);
        // If we couldn't find a break index (due to unbreakable characters), then we just don't split.
        if (UNLIKELY(breakIndex == currentPosition))
            breakIndex = characters.length();

        unsigned substringLength = breakIndex - currentPosition;
        auto substring = characters.substring(currentPosition, substringLength);
        auto textNode = Text::create(task.parent->document(), WTFMove(substring));

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

static inline QualifiedName qualifiedNameForTag(AtomHTMLToken& token, const AtomString& namespaceURI)
{
    auto nodeNamespace = findNamespace(namespaceURI);
    auto elementName = elementNameForTag(nodeNamespace, token.tagName());
    if (LIKELY(elementName != ElementName::Unknown))
        return qualifiedNameForNodeName(elementName);
    return { nullAtom(), token.name(), namespaceURI, nodeNamespace, elementName };
}

static inline QualifiedName qualifiedNameForHTMLTag(const AtomHTMLToken& token)
{
    auto elementName = elementNameForTag(Namespace::HTML, token.tagName());
    if (LIKELY(elementName != ElementName::Unknown))
        return qualifiedNameForNodeName(elementName);
    return { nullAtom(), token.name(), xhtmlNamespaceURI, Namespace::HTML, elementName };
}

Ref<Element> HTMLConstructionSite::createElement(AtomHTMLToken& token, const AtomString& namespaceURI)
{
    auto element = treeScopeForCurrentNode().createElement(qualifiedNameForTag(token, namespaceURI), true);
    setAttributes(element, token, m_parserContentPolicy);
    return element;
}

inline TreeScope& HTMLConstructionSite::treeScopeForCurrentNode()
{
    if (auto* templateElement = dynamicDowncast<HTMLTemplateElement>(currentNode()))
        return templateElement->fragmentForInsertion().treeScope();
    return currentNode().treeScope();
}

inline Document& HTMLConstructionSite::ownerDocumentForCurrentNode()
{
    if (auto* templateElement = dynamicDowncast<HTMLTemplateElement>(currentNode()))
        return templateElement->fragmentForInsertion().document();
    return currentNode().document();
}

std::tuple<RefPtr<HTMLElement>, RefPtr<JSCustomElementInterface>, RefPtr<CustomElementRegistry>> HTMLConstructionSite::createHTMLElementOrFindCustomElementInterface(AtomHTMLToken& token)
{
    // FIXME: This can't use HTMLConstructionSite::createElement because we
    // have to pass the current form element.  We should rework form association
    // to occur after construction to allow better code sharing here.
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tree-construction.html#create-an-element-for-the-token
    Ref treeScope = treeScopeForCurrentNode();
    Ref ownerDocument = treeScope->documentScope();
    bool insideTemplateElement = !ownerDocument->frame();
    RefPtr element = HTMLElementFactory::createKnownElement(token.tagName(), ownerDocument, insideTemplateElement ? nullptr : form(), true);
    if (UNLIKELY(!element)) {
        RefPtr<CustomElementRegistry> registry = treeScope->customElementRegistry();
        auto* elementInterface = registry ? registry->findInterface(token.name()) : nullptr;
        if (UNLIKELY(elementInterface)) {
            if (!m_isParsingFragment)
                return { nullptr, elementInterface, WTFMove(registry) };
            ASSERT(qualifiedNameForHTMLTag(token) == elementInterface->name());
            element = elementInterface->createElement(ownerDocument);
            element->setIsCustomElementUpgradeCandidate();
            element->enqueueToUpgrade(*elementInterface);
        } else {
            auto qualifiedName = qualifiedNameForHTMLTag(token);
            if (Document::validateCustomElementName(token.name()) == CustomElementNameValidationStatus::Valid) {
                element = HTMLMaybeFormAssociatedCustomElement::create(qualifiedName, ownerDocument);
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
    if (auto* currentPictureElement = dynamicDowncast<HTMLPictureElement>(currentNode())) {
        if (auto* imageElement = dynamicDowncast<HTMLImageElement>(*element))
            imageElement->setPictureElement(currentPictureElement);
    }

    setAttributes(*element, token, m_parserContentPolicy);
    return { element, nullptr, nullptr };
}

Ref<HTMLElement> HTMLConstructionSite::createHTMLElement(AtomHTMLToken& token)
{
    auto [element, jsInterface, registry] = createHTMLElementOrFindCustomElementInterface(token);
    ASSERT(element);
    ASSERT_UNUSED(jsInterface, !jsInterface);
    ASSERT_UNUSED(registry, !registry);
    return element.releaseNonNull();
}

HTMLStackItem HTMLConstructionSite::createElementFromSavedToken(const HTMLStackItem& item)
{
    // NOTE: Moving from item -> token -> item copies the Attribute vector twice!
    auto tagName = tagNameForElementName(item.elementName());
    AtomHTMLToken fakeToken(HTMLToken::Type::StartTag, tagName, item.localName(), Vector<Attribute>(item.attributes()));
    ASSERT(item.namespaceURI() == HTMLNames::xhtmlNamespaceURI);
    ASSERT(isFormattingTag(tagName));
    return HTMLStackItem(createHTMLElement(fakeToken), WTFMove(fakeToken));
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
        ASSERT(!unopenedEntry.stackItem().isNull());
        auto reconstructed = createElementFromSavedToken(unopenedEntry.stackItem());
        attachLater(protectedCurrentNode(), reconstructed.node());
        m_openElements.push(HTMLStackItem(reconstructed));
        unopenedEntry.replaceElement(WTFMove(reconstructed));
    }
}

void HTMLConstructionSite::generateImpliedEndTagsWithExclusion(ElementName elementName)
{
    ASSERT(elementName != ElementName::Unknown);
    while (hasImpliedEndTag(currentStackItem()) && currentStackItem().elementName() != elementName)
        m_openElements.pop();
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

// Adjusts |task| to match the "adjusted insertion location" determined by the foster parenting algorithm,
// laid out as the substeps of step 2 of https://html.spec.whatwg.org/#appropriate-place-for-inserting-a-node
void HTMLConstructionSite::findFosterSite(HTMLConstructionSiteTask& task)
{
    // When a node is to be foster parented, the last template element with no table element is below it in the stack of open elements is the foster parent element (NOT the template's parent!)
    auto* lastTemplate = m_openElements.topmost(HTML::template_);
    auto* lastTable = m_openElements.topmost(HTML::table);
    if (lastTemplate && (!lastTable || lastTemplate->isAbove(*lastTable))) {
        task.parent = &lastTemplate->element();
        return;
    }

    if (!lastTable) {
        // Fragment case
        task.parent = &m_openElements.rootNode();
        return;
    }

    if (auto* parent = lastTable->element().parentNode()) {
        task.parent = parent;
        task.nextChild = &lastTable->element();
        return;
    }

    task.parent = &lastTable->next()->element();
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

} // namespace WebCore
