/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
#include "HTMLFormElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLScriptElement.h"
#include "HTMLToken.h"
#include "HTMLTokenizer.h"
#include "LocalizedStrings.h"
#if ENABLE(MATHML)
#include "MathMLNames.h"
#endif
#include "NotImplemented.h"
#if ENABLE(SVG)
#include "SVGNames.h"
#endif
#include "Settings.h"
#include "Text.h"
#include <wtf/UnusedParam.h>

namespace WebCore {

using namespace HTMLNames;

namespace {

bool hasImpliedEndTag(ContainerNode* node)
{
    return node->hasTagName(ddTag)
        || node->hasTagName(dtTag)
        || node->hasTagName(liTag)
        || node->hasTagName(optionTag)
        || node->hasTagName(optgroupTag)
        || node->hasTagName(pTag)
        || node->hasTagName(rpTag)
        || node->hasTagName(rtTag);
}

bool causesFosterParenting(const QualifiedName& tagName)
{
    return tagName == tableTag
        || tagName == tbodyTag
        || tagName == tfootTag
        || tagName == theadTag
        || tagName == trTag;
}

inline bool isAllWhitespace(const String& string)
{
    return string.isAllSpecialCharacters<isHTMLSpace>();
}

} // namespace

static inline void executeTask(HTMLConstructionSiteTask& task)
{
    if (task.nextChild)
        task.parent->parserInsertBefore(task.child.get(), task.nextChild.get());
    else
        task.parent->parserAddChild(task.child.get());

    // JavaScript run from beforeload (or DOM Mutation or event handlers)
    // might have removed the child, in which case we should not attach it.

    if (task.child->parentNode() && task.parent->attached() && !task.child->attached())
        task.child->attach();

    task.child->beginParsingChildren();

    if (task.selfClosing)
        task.child->finishParsingChildren();
}

// FIXME: This function should return void. Callers should keep a pointer to
// the child if they want one.
template<typename ChildType>
PassRefPtr<ChildType> HTMLConstructionSite::attach(ContainerNode* parent, PassRefPtr<ChildType> prpChild)
{
    RefPtr<ChildType> child = prpChild;

    HTMLConstructionSiteTask task;
    task.parent = parent;
    task.child = child.get();

    if (shouldFosterParent()) {
        fosterParent(task.child);
        return child.release();
    }

    // Add as a sibling of the parent if we have reached the maximum depth allowed.
    if (m_openElements.stackDepth() > m_maximumDOMTreeDepth)
        task.parent = task.parent->parentNode();

    m_attachmentQueue.append(task);
    return child.release();
}

void HTMLConstructionSite::executeQueuedTasks()
{
    const size_t size = m_attachmentQueue.size();
    if (!size)
        return;

    // Copy the task queue into a local variable in case executeTask
    // re-enters the parser.
    AttachmentQueue queue;
    queue.swap(m_attachmentQueue);

    for (size_t i = 0; i < size; ++i)
        executeTask(queue[i]);

    // We might be detached now.
}

HTMLConstructionSite::HTMLConstructionSite(Document* document, unsigned maximumDOMTreeDepth)
    : m_document(document)
    , m_attachmentRoot(document)
    , m_fragmentScriptingPermission(FragmentScriptingAllowed)
    , m_isParsingFragment(false)
    , m_redirectAttachToFosterParent(false)
    , m_maximumDOMTreeDepth(maximumDOMTreeDepth)
{
}

HTMLConstructionSite::HTMLConstructionSite(DocumentFragment* fragment, FragmentScriptingPermission scriptingPermission, unsigned maximumDOMTreeDepth)
    : m_document(fragment->document())
    , m_attachmentRoot(fragment)
    , m_fragmentScriptingPermission(scriptingPermission)
    , m_isParsingFragment(true)
    , m_redirectAttachToFosterParent(false)
    , m_maximumDOMTreeDepth(maximumDOMTreeDepth)
{
}

HTMLConstructionSite::~HTMLConstructionSite()
{
}

void HTMLConstructionSite::detach()
{
    m_document = 0;
    m_attachmentRoot = 0;
}

void HTMLConstructionSite::setForm(HTMLFormElement* form)
{
    // This method should only be needed for HTMLTreeBuilder in the fragment case.
    ASSERT(!m_form);
    m_form = form;
}

PassRefPtr<HTMLFormElement> HTMLConstructionSite::takeForm()
{
    return m_form.release();
}

void HTMLConstructionSite::dispatchDocumentElementAvailableIfNeeded()
{
    ASSERT(m_document);
    if (m_document->frame() && !m_isParsingFragment)
        m_document->frame()->loader()->dispatchDocumentElementAvailable();
}

void HTMLConstructionSite::insertHTMLHtmlStartTagBeforeHTML(AtomicHTMLToken& token)
{
    RefPtr<HTMLHtmlElement> element = HTMLHtmlElement::create(m_document);
    element->parserSetAttributeMap(token.takeAttributes(), m_fragmentScriptingPermission);
    m_openElements.pushHTMLHtmlElement(attach<Element>(m_attachmentRoot, element.get()));
    element->insertedByParser();
    dispatchDocumentElementAvailableIfNeeded();
}

void HTMLConstructionSite::mergeAttributesFromTokenIntoElement(AtomicHTMLToken& token, Element* element)
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

void HTMLConstructionSite::insertHTMLHtmlStartTagInBody(AtomicHTMLToken& token)
{
    // FIXME: parse error
    
    // Fragments do not have a root HTML element, so any additional HTML elements
    // encountered during fragment parsing should be ignored.
    if (m_isParsingFragment)
        return;

    mergeAttributesFromTokenIntoElement(token, m_openElements.htmlElement());
}

void HTMLConstructionSite::insertHTMLBodyStartTagInBody(AtomicHTMLToken& token)
{
    // FIXME: parse error
    mergeAttributesFromTokenIntoElement(token, m_openElements.bodyElement());
}

void HTMLConstructionSite::insertDoctype(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLTokenTypes::DOCTYPE);
    attach(m_attachmentRoot, DocumentType::create(m_document, token.name(), String::adopt(token.publicIdentifier()), String::adopt(token.systemIdentifier())));

    // DOCTYPE nodes are only processed when parsing fragments w/o contextElements, which
    // never occurs.  However, if we ever chose to support such, this code is subtly wrong,
    // because context-less fragments can determine their own quirks mode, and thus change
    // parsing rules (like <p> inside <table>).  For now we ASSERT that we never hit this code
    // in a fragment, as changing the owning document's compatibility mode would be wrong.
    ASSERT(!m_isParsingFragment);
    if (m_isParsingFragment)
        return;

    if (token.forceQuirks())
        m_document->setCompatibilityMode(Document::QuirksMode);
    else {
        // We need to actually add the Doctype node to the DOM.
        executeQueuedTasks();
        m_document->setCompatibilityModeFromDoctype();
    }
}

void HTMLConstructionSite::insertComment(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLTokenTypes::Comment);
    attach(currentNode(), Comment::create(currentNode()->document(), token.comment()));
}

void HTMLConstructionSite::insertCommentOnDocument(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLTokenTypes::Comment);
    attach(m_attachmentRoot, Comment::create(m_document, token.comment()));
}

void HTMLConstructionSite::insertCommentOnHTMLHtmlElement(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLTokenTypes::Comment);
    ContainerNode* parent = m_openElements.rootNode();
    attach(parent, Comment::create(parent->document(), token.comment()));
}

PassRefPtr<Element> HTMLConstructionSite::attachToCurrent(PassRefPtr<Element> child)
{
    return attach(currentNode(), child);
}

void HTMLConstructionSite::insertHTMLHeadElement(AtomicHTMLToken& token)
{
    ASSERT(!shouldFosterParent());
    m_head = attachToCurrent(createHTMLElement(token));
    m_openElements.pushHTMLHeadElement(m_head);
}

void HTMLConstructionSite::insertHTMLBodyElement(AtomicHTMLToken& token)
{
    ASSERT(!shouldFosterParent());
    m_openElements.pushHTMLBodyElement(attachToCurrent(createHTMLElement(token)));
}

void HTMLConstructionSite::insertHTMLFormElement(AtomicHTMLToken& token, bool isDemoted)
{
    RefPtr<Element> element = createHTMLElement(token);
    ASSERT(element->hasTagName(formTag));
    RefPtr<HTMLFormElement> form = static_pointer_cast<HTMLFormElement>(element.release());
    form->setDemoted(isDemoted);
    m_openElements.push(attachToCurrent(form.release()));
    ASSERT(currentElement()->isHTMLElement());
    ASSERT(currentElement()->hasTagName(formTag));
    m_form = static_cast<HTMLFormElement*>(currentElement());
}

void HTMLConstructionSite::insertHTMLElement(AtomicHTMLToken& token)
{
    m_openElements.push(attachToCurrent(createHTMLElement(token)));
}

void HTMLConstructionSite::insertSelfClosingHTMLElement(AtomicHTMLToken& token)
{
    ASSERT(token.type() == HTMLTokenTypes::StartTag);
    RefPtr<Element> element = attachToCurrent(createHTMLElement(token));
    // Normally HTMLElementStack is responsible for calling finishParsingChildren,
    // but self-closing elements are never in the element stack so the stack
    // doesn't get a chance to tell them that we're done parsing their children.
    m_attachmentQueue.last().selfClosing = true;
    // FIXME: Do we want to acknowledge the token's self-closing flag?
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#acknowledge-self-closing-flag
}

void HTMLConstructionSite::insertFormattingElement(AtomicHTMLToken& token)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#the-stack-of-open-elements
    // Possible active formatting elements include:
    // a, b, big, code, em, font, i, nobr, s, small, strike, strong, tt, and u.
    insertHTMLElement(token);
    m_activeFormattingElements.append(currentElement());
}

void HTMLConstructionSite::insertScriptElement(AtomicHTMLToken& token)
{
    RefPtr<HTMLScriptElement> element = HTMLScriptElement::create(scriptTag, currentNode()->document(), true);
    if (m_fragmentScriptingPermission == FragmentScriptingAllowed)
        element->parserSetAttributeMap(token.takeAttributes(), m_fragmentScriptingPermission);
    m_openElements.push(attachToCurrent(element.release()));
}

void HTMLConstructionSite::insertForeignElement(AtomicHTMLToken& token, const AtomicString& namespaceURI)
{
    ASSERT(token.type() == HTMLTokenTypes::StartTag);
    notImplemented(); // parseError when xmlns or xmlns:xlink are wrong.

    RefPtr<Element> element = attachToCurrent(createElement(token, namespaceURI));
    if (!token.selfClosing())
        m_openElements.push(element);
}

void HTMLConstructionSite::insertTextNode(const String& characters, WhitespaceMode whitespaceMode)
{
    HTMLConstructionSiteTask task;
    task.parent = currentNode();
    task.nextChild = 0;
    if (shouldFosterParent())
        findFosterSite(task);

    // Strings composed entirely of whitespace are likely to be repeated.
    // Turn them into AtomicString so we share a single string for each.
    bool shouldUseAtomicString = whitespaceMode == AllWhitespace
        || (whitespaceMode == WhitespaceUnknown && isAllWhitespace(characters));

    unsigned currentPosition = 0;

    // FIXME: Splitting text nodes into smaller chunks contradicts HTML5 spec, but is currently necessary
    // for performance, see <https://bugs.webkit.org/show_bug.cgi?id=55898>.

    Node* previousChild = task.nextChild ? task.nextChild->previousSibling() : task.parent->lastChild();
    if (previousChild && previousChild->isTextNode()) {
        // FIXME: We're only supposed to append to this text node if it
        // was the last text node inserted by the parser.
        CharacterData* textNode = static_cast<CharacterData*>(previousChild);
        currentPosition = textNode->parserAppendData(characters.characters(), characters.length(), Text::defaultLengthLimit);
    }

    while (currentPosition < characters.length()) {
        RefPtr<Text> textNode = Text::createWithLengthLimit(task.parent->document(), shouldUseAtomicString ? AtomicString(characters).string() : characters, currentPosition);
        // If we have a whole string of unbreakable characters the above could lead to an infinite loop. Exceeding the length limit is the lesser evil.
        if (!textNode->length()) {
            String substring = characters.substring(currentPosition);
            textNode = Text::create(task.parent->document(), shouldUseAtomicString ? AtomicString(substring).string() : substring);
        }

        currentPosition += textNode->length();
        ASSERT(currentPosition <= characters.length());
        task.child = textNode.release();
        executeTask(task);
    }
}

PassRefPtr<Element> HTMLConstructionSite::createElement(AtomicHTMLToken& token, const AtomicString& namespaceURI)
{
    QualifiedName tagName(nullAtom, token.name(), namespaceURI);
    RefPtr<Element> element = currentNode()->document()->createElement(tagName, true);
    element->parserSetAttributeMap(token.takeAttributes(), m_fragmentScriptingPermission);
    return element.release();
}

PassRefPtr<Element> HTMLConstructionSite::createHTMLElement(AtomicHTMLToken& token)
{
    QualifiedName tagName(nullAtom, token.name(), xhtmlNamespaceURI);
    // FIXME: This can't use HTMLConstructionSite::createElement because we
    // have to pass the current form element.  We should rework form association
    // to occur after construction to allow better code sharing here.
    RefPtr<Element> element = HTMLElementFactory::createHTMLElement(tagName, currentNode()->document(), form(), true);
    element->parserSetAttributeMap(token.takeAttributes(), m_fragmentScriptingPermission);
    ASSERT(element->isHTMLElement());
    return element.release();
}

PassRefPtr<Element> HTMLConstructionSite::createHTMLElementFromElementRecord(HTMLElementStack::ElementRecord* record)
{
    return createHTMLElementFromSavedElement(record->element());
}

namespace {

PassRefPtr<NamedNodeMap> cloneAttributes(Element* element)
{
    NamedNodeMap* attributes = element->attributes(true);
    if (!attributes)
        return 0;

    RefPtr<NamedNodeMap> newAttributes = NamedNodeMap::create();
    for (size_t i = 0; i < attributes->length(); ++i) {
        Attribute* attribute = attributes->attributeItem(i);
        RefPtr<Attribute> clone = Attribute::createMapped(attribute->name(), attribute->value());
        newAttributes->addAttribute(clone);
    }
    return newAttributes.release();
}

}

PassRefPtr<Element> HTMLConstructionSite::createHTMLElementFromSavedElement(Element* element)
{
    // FIXME: This method is wrong.  We should be using the original token.
    // Using an Element* causes us to fail examples like this:
    // <b id="1"><p><script>document.getElementById("1").id = "2"</script></p>TEXT</b>
    // When reconstructTheActiveFormattingElements calls this method to open
    // a second <b> tag to wrap TEXT, it will have id "2", even though the HTML5
    // spec implies it should be "1".  Minefield matches the HTML5 spec here.

    ASSERT(element->isHTMLElement()); // otherwise localName() might be wrong.
    AtomicHTMLToken fakeToken(HTMLTokenTypes::StartTag, element->localName(), cloneAttributes(element));
    return createHTMLElement(fakeToken);
}

bool HTMLConstructionSite::indexOfFirstUnopenFormattingElement(unsigned& firstUnopenElementIndex) const
{
    if (m_activeFormattingElements.isEmpty())
        return false;
    unsigned index = m_activeFormattingElements.size();
    do {
        --index;
        const HTMLFormattingElementList::Entry& entry = m_activeFormattingElements.at(index);
        if (entry.isMarker() || m_openElements.contains(entry.element())) {
            firstUnopenElementIndex = index + 1;
            return firstUnopenElementIndex < m_activeFormattingElements.size();
        }
    } while (index);
    firstUnopenElementIndex = index;
    return true;
}

void HTMLConstructionSite::reconstructTheActiveFormattingElements()
{
    unsigned firstUnopenElementIndex;
    if (!indexOfFirstUnopenFormattingElement(firstUnopenElementIndex))
        return;

    unsigned unopenEntryIndex = firstUnopenElementIndex;
    ASSERT(unopenEntryIndex < m_activeFormattingElements.size());
    for (; unopenEntryIndex < m_activeFormattingElements.size(); ++unopenEntryIndex) {
        HTMLFormattingElementList::Entry& unopenedEntry = m_activeFormattingElements.at(unopenEntryIndex);
        RefPtr<Element> reconstructed = createHTMLElementFromSavedElement(unopenedEntry.element());
        m_openElements.push(attachToCurrent(reconstructed.release()));
        unopenedEntry.replaceElement(currentElement());
    }
}

void HTMLConstructionSite::generateImpliedEndTagsWithExclusion(const AtomicString& tagName)
{
    while (hasImpliedEndTag(currentNode()) && !currentNode()->hasLocalName(tagName))
        m_openElements.pop();
}

void HTMLConstructionSite::generateImpliedEndTags()
{
    while (hasImpliedEndTag(currentNode()))
        m_openElements.pop();
}

void HTMLConstructionSite::findFosterSite(HTMLConstructionSiteTask& task)
{
    HTMLElementStack::ElementRecord* lastTableElementRecord = m_openElements.topmost(tableTag.localName());
    if (lastTableElementRecord) {
        Element* lastTableElement = lastTableElementRecord->element();
        if (ContainerNode* parent = lastTableElement->parentNode()) {
            task.parent = parent;
            task.nextChild = lastTableElement;
            return;
        }
        task.parent = lastTableElementRecord->next()->element();
        return;
    }
    // Fragment case
    task.parent = m_openElements.rootNode(); // DocumentFragment
}

bool HTMLConstructionSite::shouldFosterParent() const
{
    return m_redirectAttachToFosterParent
        && currentNode()->isElementNode()
        && causesFosterParenting(currentElement()->tagQName());
}

void HTMLConstructionSite::fosterParent(PassRefPtr<Node> node)
{
    HTMLConstructionSiteTask task;
    task.nextChild = 0;
    findFosterSite(task);
    task.child = node;
    executeTask(task);
}

}
