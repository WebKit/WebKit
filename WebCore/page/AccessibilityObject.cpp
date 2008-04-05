/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AccessibilityObject.h"

#include "AXObjectCache.h"
#include "CharacterNames.h"
#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLAreaElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLMapElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "NodeList.h"
#include "NotImplemented.h"
#include "Page.h"
#include "RenderImage.h"
#include "RenderListMarker.h"
#include "RenderMenuList.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SelectionController.h"
#include "TextIterator.h"
#include "htmlediting.h"
#include "visible_units.h"

using namespace std;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

AccessibilityObject::AccessibilityObject(RenderObject* renderer)
    : m_renderer(renderer)
    , m_id(0)
{
#ifndef NDEBUG
    m_renderer->setHasAXObject(true);
#endif
}

AccessibilityObject::~AccessibilityObject()
{
    ASSERT(isDetached());
}

PassRefPtr<AccessibilityObject> AccessibilityObject::create(RenderObject* renderer)
{
    return adoptRef(new AccessibilityObject(renderer));
}

void AccessibilityObject::detach()
{
    removeAXObjectID();
#ifndef NDEBUG
    if (m_renderer)
        m_renderer->setHasAXObject(false);
#endif
    m_renderer = 0;
    m_wrapper = 0;
    clearChildren();
}

AccessibilityObject* AccessibilityObject::firstChild() const
{
    if (!m_renderer || !m_renderer->firstChild())
        return 0;

    return m_renderer->document()->axObjectCache()->get(m_renderer->firstChild());
}

AccessibilityObject* AccessibilityObject::lastChild() const
{
    if (!m_renderer || !m_renderer->lastChild())
        return 0;

    return m_renderer->document()->axObjectCache()->get(m_renderer->lastChild());
}

AccessibilityObject* AccessibilityObject::previousSibling() const
{
    if (!m_renderer || !m_renderer->previousSibling())
        return 0;

    return m_renderer->document()->axObjectCache()->get(m_renderer->previousSibling());
}

AccessibilityObject* AccessibilityObject::nextSibling() const
{
    if (!m_renderer || !m_renderer->nextSibling())
        return 0;

    return m_renderer->document()->axObjectCache()->get(m_renderer->nextSibling());
}

AccessibilityObject* AccessibilityObject::parentObject() const
{
    if (m_areaElement)
        return m_renderer->document()->axObjectCache()->get(m_renderer);

    if (!m_renderer || !m_renderer->parent())
        return 0;

    return m_renderer->document()->axObjectCache()->get(m_renderer->parent());
}

AccessibilityObject* AccessibilityObject::parentObjectUnignored() const
{
    AccessibilityObject* parent;
    for (parent = parentObject(); parent && parent->accessibilityIsIgnored(); parent = parent->parentObject())
        ;
    return parent;
}

bool AccessibilityObject::isWebArea() const
{
    return m_renderer->isRenderView();
}

bool AccessibilityObject::isImageButton() const
{
    return isImage() && m_renderer->element() && m_renderer->element()->hasTagName(inputTag);
}

bool AccessibilityObject::isRenderImage() const
{
    return isImage() && !strcmp(m_renderer->renderName(), "RenderImage");
}

bool AccessibilityObject::isAnchor() const
{
    return m_areaElement || (!isImage() && m_renderer->element() && m_renderer->element()->isLink());
}

bool AccessibilityObject::isTextControl() const
{
    return m_renderer->isTextField() || m_renderer->isTextArea();
}

bool AccessibilityObject::isImage() const
{
    return m_renderer->isImage();
}

bool AccessibilityObject::isAttachment() const
{
    // widgets are the replaced elements that we represent to AX as attachments
    bool isWidget = m_renderer && m_renderer->isWidget();

    // assert that a widget is a replaced element that is not an image
    ASSERT(!isWidget || (m_renderer->isReplaced() && !isImage()));
    return isWidget;
}

bool AccessibilityObject::isPasswordField() const
{
    if (!m_renderer)
        return 0;
    if (!m_renderer->element())
        return 0;
    if (!m_renderer->element()->isHTMLElement())
        return 0;
    return static_cast<HTMLElement*>(m_renderer->element())->isPasswordField();
}

int AccessibilityObject::headingLevel(Node* node)
{
    // headings can be in block flow and non-block flow

    if (!node)
        return 0;

    if (node->hasTagName(h1Tag))
        return 1;

    if (node->hasTagName(h2Tag))
        return 2;

    if (node->hasTagName(h3Tag))
        return 3;

    if (node->hasTagName(h4Tag))
        return 4;

    if (node->hasTagName(h5Tag))
        return 5;

    if (node->hasTagName(h6Tag))
        return 6;

    return 0;
}

bool AccessibilityObject::isHeading() const
{
    return headingLevel(m_renderer->element()) != 0;
}

HTMLAnchorElement* AccessibilityObject::anchorElement() const
{
    // return already-known anchor for image areas
    if (m_areaElement)
        return m_areaElement.get();

    // Search up the render tree for a RenderObject with a DOM node.  Defer to an earlier continuation, though.
    RenderObject* currRenderer;
    for (currRenderer = m_renderer; currRenderer && !currRenderer->element(); currRenderer = currRenderer->parent()) {
        if (currRenderer->continuation())
            return currRenderer->document()->axObjectCache()->get(currRenderer->continuation())->anchorElement();
    }

    // bail if none found
    if (!currRenderer)
        return 0;

    // search up the DOM tree for an anchor element
    // NOTE: this assumes that any non-image with an anchor is an HTMLAnchorElement
    Node* node = currRenderer->node();
    for ( ; node; node = node->parentNode()) {
        if (node->hasTagName(aTag))
            return static_cast<HTMLAnchorElement*>(node);
    }

    return 0;
}

static bool isCheckboxOrRadio(HTMLInputElement* input)
{
    return (input->inputType() == HTMLInputElement::CHECKBOX ||
            input->inputType() == HTMLInputElement::RADIO);
}

static bool isCheckboxOrRadio(Node* node)
{
    if (!node->hasTagName(inputTag))
        return false;
    return isCheckboxOrRadio(static_cast<HTMLInputElement*>(node));
}

Element* AccessibilityObject::actionElement() const
{
    if (m_renderer->element() && m_renderer->element()->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(m_renderer->element());
        if (!input->disabled() && (isCheckboxOrRadio(input) ||
                                   input->isTextButton()))
            return input;
    }

    if (isImageButton())
        return static_cast<Element*>(m_renderer->element());

    if (m_renderer->isMenuList())
        return static_cast<RenderMenuList*>(m_renderer)->selectElement();

    Element* elt = anchorElement();
    if (!elt)
        elt = mouseButtonListener();
    return elt;
}

Element* AccessibilityObject::mouseButtonListener() const
{
    Node* node = m_renderer->element();
    if (!node)
        return 0;
    if (!node->isEventTargetNode())
        return 0;

    // FIXME: Do the continuation search like anchorElement does
    for (EventTargetNode* elt = static_cast<EventTargetNode*>(node); elt; elt = static_cast<EventTargetNode*>(elt->parentNode())) {
        if (elt->getHTMLEventListener(clickEvent) || elt->getHTMLEventListener(mousedownEvent) || elt->getHTMLEventListener(mouseupEvent))
            return static_cast<Element*>(elt);
    }

    return 0;
}

String AccessibilityObject::helpText() const
{
    if (!m_renderer)
        return String();

    if (m_areaElement) {
        const AtomicString& summary = m_areaElement->getAttribute(summaryAttr);
        if (!summary.isEmpty())
            return summary;
        const AtomicString& title = m_areaElement->getAttribute(titleAttr);
        if (!title.isEmpty())
            return title;
    }

    for (RenderObject* curr = m_renderer; curr; curr = curr->parent()) {
        if (curr->element() && curr->element()->isHTMLElement()) {
            const AtomicString& summary = static_cast<Element*>(curr->element())->getAttribute(summaryAttr);
            if (!summary.isEmpty())
                return summary;
            const AtomicString& title = static_cast<Element*>(curr->element())->getAttribute(titleAttr);
            if (!title.isEmpty())
                return title;
        }
    }

    return String();
}

String AccessibilityObject::textUnderElement() const
{
    if (!m_renderer)
        return String();

    Node* node = m_renderer->element();
    if (node) {
        if (Frame* frame = node->document()->frame()) {
            // catch stale WebCoreAXObject (see <rdar://problem/3960196>)
            if (frame->document() != node->document())
                return String();
            return plainText(rangeOfContents(node).get());
        }
    }

    // return the null string for anonymous text because it is non-trivial to get
    // the actual text and, so far, that is not needed
    return String();
}

bool AccessibilityObject::hasIntValue() const
{
    if (isHeading())
        return true;

    if (m_renderer->element() && isCheckboxOrRadio(m_renderer->element()))
        return true;

    return false;
}

int AccessibilityObject::intValue() const
{
    if (!m_renderer || m_areaElement || isPasswordField())
        return 0;

    if (isHeading())
        return headingLevel(m_renderer->element());

    Node* node = m_renderer->element();
    if (node && isCheckboxOrRadio(node))
        return static_cast<HTMLInputElement*>(node)->checked();

    return 0;
}

String AccessibilityObject::stringValue() const
{
    if (!m_renderer || m_areaElement || isPasswordField())
        return String();

    if (m_renderer->isText())
        return textUnderElement();

    if (m_renderer->isMenuList())
        return static_cast<RenderMenuList*>(m_renderer)->text();

    if (m_renderer->isListMarker())
        return static_cast<RenderListMarker*>(m_renderer)->text();

    if (isWebArea()) {
        if (m_renderer->document()->frame())
            return String();

        // FIXME: should use startOfDocument and endOfDocument (or rangeForDocument?) here
        VisiblePosition startVisiblePosition = m_renderer->positionForCoordinates(0, 0);
        VisiblePosition endVisiblePosition = m_renderer->positionForCoordinates(INT_MAX, INT_MAX);
        if (startVisiblePosition.isNull() || endVisiblePosition.isNull())
            return String();

        return plainText(makeRange(startVisiblePosition, endVisiblePosition).get());
    }

    if (isTextControl())
        return static_cast<RenderTextControl*>(m_renderer)->text();

    // FIXME: We might need to implement a value here for more types
    // FIXME: It would be better not to advertise a value at all for the types for which we don't implement one;
    // this would require subclassing or making accessibilityAttributeNames do something other than return a
    // single static array.
    return String();
}

static HTMLLabelElement* labelForElement(Element* element)
{
    RefPtr<NodeList> list = element->document()->getElementsByTagName("label");
    unsigned len = list->length();
    for (unsigned i = 0; i < len; i++) {
        if (list->item(i)->hasTagName(labelTag)) {
            HTMLLabelElement* label = static_cast<HTMLLabelElement*>(list->item(i));
            if (label->correspondingControl() == element)
                return label;
        }
    }

    return 0;
}

String AccessibilityObject::title() const
{
    if (!m_renderer || m_areaElement || !m_renderer->element())
        return String();

    if (m_renderer->element()->hasTagName(buttonTag))
        return textUnderElement();

    if (m_renderer->element()->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(m_renderer->element());
        if (input->isTextButton())
            return input->value();

        HTMLLabelElement* label = labelForElement(input);
        if (label)
            return label->innerText();
    }

    if (m_renderer->element()->isLink() || isHeading())
        return textUnderElement();

    return String();
}

String AccessibilityObject::accessibilityDescription() const
{
    if (!m_renderer || m_areaElement)
        return String();

    if (isImage()) {
        if (m_renderer->element() && m_renderer->element()->isHTMLElement()) {
            const AtomicString& alt = static_cast<HTMLElement*>(m_renderer->element())->getAttribute(altAttr);
            if (alt.isEmpty())
                return String();
            return alt;
        }
    }

    if (isWebArea()) {
        Document *document = m_renderer->document();
        Node* owner = document->ownerElement();
        if (owner) {
            if (owner->hasTagName(frameTag) || owner->hasTagName(iframeTag))
                return static_cast<HTMLFrameElementBase*>(owner)->name();
            if (owner->isHTMLElement())
                return static_cast<HTMLElement*>(owner)->getAttribute(nameAttr);
        }
        owner = document->body();
        if (owner && owner->isHTMLElement())
            return static_cast<HTMLElement*>(owner)->getAttribute(nameAttr);
    }

    return String();
}

IntRect AccessibilityObject::boundingBoxRect() const
{
    IntRect rect;
    RenderObject* obj = m_renderer;

    if (!obj)
        return IntRect();

    if (obj->isInlineContinuation())
        obj = obj->element()->renderer();
    Vector<IntRect> rects;
    int x, y;
    obj->absolutePosition(x, y);
    obj->absoluteRects(rects, x, y);
    const size_t n = rects.size();
    for (size_t i = 0; i < n; ++i) {
        IntRect r = rects[i];
        if (!r.isEmpty()) {
            if (obj->style()->hasAppearance())
                theme()->adjustRepaintRect(obj, r);
            rect.unite(r);
        }
    }
    return rect;
}

IntSize AccessibilityObject::size() const
{
    IntRect rect = m_areaElement ? m_areaElement->getRect(m_renderer) : boundingBoxRect();
    return rect.size();
}

// the closest object for an internal anchor
AccessibilityObject* AccessibilityObject::linkedUIElement() const
{
    if (!isAnchor())
        return 0;

    HTMLAnchorElement* anchor = anchorElement();
    if (!anchor)
        return 0;

    KURL linkURL = anchor->href();
    String ref = linkURL.ref();
    if (ref.isEmpty())
        return 0;

    // check if URL is the same as current URL
    linkURL.setRef("");
    if (m_renderer->document()->url() != linkURL)
        return 0;

    Node* linkedNode = m_renderer->document()->getElementById(ref);
    if (!linkedNode) {
        linkedNode = m_renderer->document()->anchors()->namedItem(ref, !m_renderer->document()->inCompatMode());
        if (!linkedNode)
            return 0;
    }

    // the element we find may not be accessible, keep searching until we find a good one
    AccessibilityObject* linkedAXElement = m_renderer->document()->axObjectCache()->get(linkedNode->renderer());
    while (linkedAXElement && linkedAXElement->accessibilityIsIgnored()) {
        linkedNode = linkedNode->traverseNextNode();
        if (!linkedNode)
            return 0;
        linkedAXElement = m_renderer->document()->axObjectCache()->get(linkedNode->renderer());
    }

    return linkedAXElement;
}

bool AccessibilityObject::accessibilityShouldUseUniqueId() const
{
    return isWebArea() || isTextControl();
}

bool AccessibilityObject::accessibilityIsIgnored() const
{
    // ignore invisible element
    if (!m_renderer || m_renderer->style()->visibility() != VISIBLE)
        return true;

    // ignore popup menu items because AppKit does
    for (RenderObject* parent = m_renderer->parent(); parent; parent = parent->parent()) {
        if (parent->isMenuList())
            return true;
    }

    // NOTE: BRs always have text boxes now, so the text box check here can be removed
    if (m_renderer->isText())
        return m_renderer->isBR() || !static_cast<RenderText*>(m_renderer)->firstTextBox();

    if (isHeading())
        return false;

    if (m_areaElement || (m_renderer->element() && m_renderer->element()->isLink()))
        return false;

    // all controls are accessible
    if (m_renderer->element() && m_renderer->element()->isControl())
        return false;

    if (m_renderer->isBlockFlow() && m_renderer->childrenInline())
        return !static_cast<RenderBlock*>(m_renderer)->firstLineBox() && !mouseButtonListener();

    // ignore images seemingly used as spacers
    if (isRenderImage()) {
        // informal standard is to ignore images with zero-length alt strings
        Node* node = m_renderer->element();
        if (node && node->isElementNode()) {
            Element* elt = static_cast<Element*>(node);
            const AtomicString& alt = elt->getAttribute(altAttr);
            if (alt.isEmpty() && !alt.isNull())
                return true;
        }

        // check for one-dimensional image
        if (m_renderer->height() <= 1 || m_renderer->width() <= 1)
            return true;

        // check whether rendered image was stretched from one-dimensional file image
        RenderImage* image = static_cast<RenderImage*>(m_renderer);
        if (image->cachedImage()) {
            IntSize imageSize = image->cachedImage()->imageSize(image->view()->zoomFactor());
            return imageSize.height() <= 1 || imageSize.width() <= 1;
        }

        return false;
    }

    return !m_renderer->isListMarker() && !isWebArea();
}

bool AccessibilityObject::isLoaded() const
{
    return !m_renderer->document()->tokenizer();
}

int AccessibilityObject::layoutCount() const
{
    return static_cast<RenderView*>(m_renderer)->frameView()->layoutCount();
}

int AccessibilityObject::textLength() const
{
    ASSERT(isTextControl());

    if (isPasswordField())
        return -1; // need to return something distinct from 0

    return static_cast<RenderTextControl*>(m_renderer)->text().length();
}

String AccessibilityObject::selectedText() const
{
    ASSERT(isTextControl());

    if (isPasswordField())
        return String(); // need to return something distinct from empty string

    RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
    return textControl->text().substring(textControl->selectionStart(), textControl->selectionEnd() - textControl->selectionStart());
}

Selection AccessibilityObject::selection() const
{
    return m_renderer->document()->frame()->selectionController()->selection();
}

AccessibilityObject::PlainTextRange AccessibilityObject::selectedTextRange() const
{
    ASSERT(isTextControl());

    if (isPasswordField())
        return PlainTextRange();

    RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
    return PlainTextRange(textControl->selectionStart(), textControl->selectionEnd() - textControl->selectionStart());
}

void AccessibilityObject::setSelectedText(const String&)
{
    // TODO: set selected text (ReplaceSelectionCommand). <rdar://problem/4712125>
    notImplemented();
}

void AccessibilityObject::setSelectedTextRange(const PlainTextRange& range)
{
    RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
    textControl->setSelectionRange(range.start, range.start + range.length);
}

void AccessibilityObject::makeRangeVisible(const PlainTextRange&)
{
    // TODO: make range visible (scrollRectToVisible).  <rdar://problem/4712101>
    notImplemented();
}

KURL AccessibilityObject::url() const
{
    if (isAnchor()) {
        if (HTMLAnchorElement* anchor = anchorElement())
            return anchor->href();
    }
    if (isImage() && m_renderer->element() && m_renderer->element()->hasTagName(imgTag))
        return static_cast<HTMLImageElement*>(m_renderer->element())->src();

    return KURL();
}

bool AccessibilityObject::isVisited() const
{
    return m_renderer->style()->pseudoState() == PseudoVisited;
}

bool AccessibilityObject::isFocused() const
{
    return (m_renderer->element() && m_renderer->document()->focusedNode() == m_renderer->element());
}

void AccessibilityObject::setFocused(bool on)
{
    if (!canSetFocusAttribute())
        return;

    if (!on)
        m_renderer->document()->setFocusedNode(0);
    else {
        if (m_renderer->element()->isElementNode())
            static_cast<Element*>(m_renderer->element())->focus();
        else
            m_renderer->document()->setFocusedNode(m_renderer->element());
    }
}

void AccessibilityObject::setValue(const String& string)
{
    if (m_renderer->isTextField()) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(m_renderer->element());
        input->setValue(string);
    } else if (m_renderer->isTextArea()) {
        HTMLTextAreaElement* textArea = static_cast<HTMLTextAreaElement*>(m_renderer->element());
        textArea->setValue(string);
    }
}

bool AccessibilityObject::isEnabled() const
{
    return m_renderer->element() ? m_renderer->element()->isEnabled() : true;
}

void AccessibilityObject::press()
{
    Element* actionElem = actionElement();
    if (!actionElem)
        return;
    if (Frame* f = actionElem->document()->frame())
        f->loader()->resetMultipleFormSubmissionProtection();
    actionElem->accessKeyAction(true);
}

RenderObject* AccessibilityObject::topRenderer() const
{
    return m_renderer->document()->topDocument()->renderer();
}

RenderTextControl* AccessibilityObject::textControl() const
{
    if (!isTextControl())
        return 0;

    return static_cast<RenderTextControl*>(m_renderer);
}

Widget* AccessibilityObject::widget() const
{
    if (!m_renderer->isWidget())
        return 0;

    return static_cast<RenderWidget*>(m_renderer)->widget();
}

AXObjectCache* AccessibilityObject::axObjectCache() const
{
    return m_renderer->document()->axObjectCache();
}

void AccessibilityObject::getDocumentLinks(Vector<RefPtr<AccessibilityObject> >& result) const
{
    Document* document = m_renderer->document();
    RefPtr<HTMLCollection> coll = document->links();
    Node* curr = coll->firstItem();
    while (curr) {
        RenderObject* obj = curr->renderer();
        if (obj) {
            RefPtr<AccessibilityObject> axobj = document->axObjectCache()->get(obj);
            ASSERT(axobj);
            ASSERT(axobj->roleValue() == WebCoreLinkRole);
            if (!axobj->accessibilityIsIgnored())
                result.append(axobj);
        }
        curr = coll->nextItem();
    }
}

FrameView* AccessibilityObject::documentFrameView() const
{
    // this is the RenderObject's Document's Frame's FrameView
    return m_renderer->document()->view();
}

Widget* AccessibilityObject::widgetForAttachmentView() const
{
    if (!isAttachment())
        return 0;
    return static_cast<RenderWidget*>(m_renderer)->widget();
}

FrameView* AccessibilityObject::frameViewIfRenderView() const
{
    if (!m_renderer->isRenderView())
        return 0;
    // this is the RenderObject's Document's renderer's FrameView
    return m_renderer->view()->frameView();
}

// This function is like a cross-platform version of - (WebCoreTextMarkerRange*)textMarkerRange. It returns
// a Range that we can convert to a WebCoreTextMarkerRange in the Obj-C file
VisiblePositionRange AccessibilityObject::visiblePositionRange() const
{
    if (!m_renderer)
        return VisiblePositionRange();

    // construct VisiblePositions for start and end
    Node* node = m_renderer->element();
    VisiblePosition startPos = VisiblePosition(node, 0, VP_DEFAULT_AFFINITY);
    VisiblePosition endPos = VisiblePosition(node, maxDeepOffset(node), VP_DEFAULT_AFFINITY);

    // the VisiblePositions are equal for nodes like buttons, so adjust for that
    if (startPos == endPos) {
        endPos = endPos.next();
        if (endPos.isNull())
            endPos = startPos;
    }

    return VisiblePositionRange(startPos, endPos);
}

VisiblePositionRange AccessibilityObject::doAXTextMarkerRangeForLine(unsigned lineCount) const
{
    if (lineCount == 0 || !m_renderer)
        return VisiblePositionRange();

    // iterate over the lines
    // FIXME: this is wrong when lineNumber is lineCount+1,  because nextLinePosition takes you to the
    // last offset of the last line
    VisiblePosition visiblePos = m_renderer->document()->renderer()->positionForCoordinates(0, 0);
    VisiblePosition savedVisiblePos;
    while (--lineCount != 0) {
        savedVisiblePos = visiblePos;
        visiblePos = nextLinePosition(visiblePos, 0);
        if (visiblePos.isNull() || visiblePos == savedVisiblePos)
            return VisiblePositionRange();
    }

    // make a caret selection for the marker position, then extend it to the line
    // NOTE: ignores results of sel.modify because it returns false when
    // starting at an empty line.  The resulting selection in that case
    // will be a caret at visiblePos.
    SelectionController selectionController;
    selectionController.setSelection(Selection(visiblePos));
    selectionController.modify(SelectionController::EXTEND, SelectionController::RIGHT, LineBoundary);

    return VisiblePositionRange(selectionController.selection().visibleStart(), selectionController.selection().visibleEnd());
}

VisiblePositionRange AccessibilityObject::doAXTextMarkerRangeForUnorderedTextMarkers(const VisiblePosition& visiblePos1, const VisiblePosition& visiblePos2) const
{
    if (visiblePos1.isNull() || visiblePos2.isNull())
        return VisiblePositionRange();

    VisiblePosition startPos;
    VisiblePosition endPos;
    bool alreadyInOrder;

    // upstream is ordered before downstream for the same position
    if (visiblePos1 == visiblePos2 && visiblePos2.affinity() == UPSTREAM)
        alreadyInOrder = false;

    // use selection order to see if the positions are in order
    else
        alreadyInOrder = Selection(visiblePos1, visiblePos2).isBaseFirst();

    if (alreadyInOrder) {
        startPos = visiblePos1;
        endPos = visiblePos2;
    } else {
        startPos = visiblePos2;
        endPos = visiblePos1;
    }

    return VisiblePositionRange(startPos, endPos);
}

VisiblePositionRange AccessibilityObject::doAXLeftWordTextMarkerRangeForTextMarker(const VisiblePosition& visiblePos) const
{
    VisiblePosition startPosition = startOfWord(visiblePos, LeftWordIfOnBoundary);
    VisiblePosition endPosition = endOfWord(startPosition);
    return VisiblePositionRange(startPosition, endPosition);
}

VisiblePositionRange AccessibilityObject::doAXRightWordTextMarkerRangeForTextMarker(const VisiblePosition& visiblePos) const
{
    VisiblePosition startPosition = startOfWord(visiblePos, RightWordIfOnBoundary);
    VisiblePosition endPosition = endOfWord(startPosition);
    return VisiblePositionRange(startPosition, endPosition);
}

static VisiblePosition updateAXLineStartForVisiblePosition(const VisiblePosition& visiblePosition)
{
    // A line in the accessibility sense should include floating objects, such as aligned image, as part of a line.
    // So let's update the position to include that.
    VisiblePosition tempPosition;
    VisiblePosition startPosition = visiblePosition;
    Position p;
    RenderObject* renderer;
    while (true) {
        tempPosition = startPosition.previous();
        if (tempPosition.isNull())
            break;
        p = tempPosition.deepEquivalent();
        if (!p.node())
            break;
        renderer = p.node()->renderer();
        if (!renderer || renderer->inlineBox(p.offset(), tempPosition.affinity()) || (renderer->isRenderBlock() && p.offset() == 0))
            break;
        startPosition = tempPosition;
    }

    return startPosition;
}

VisiblePositionRange AccessibilityObject::doAXLeftLineTextMarkerRangeForTextMarker(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePositionRange();

    // make a caret selection for the position before marker position (to make sure
    // we move off of a line start)
    VisiblePosition prevVisiblePos = visiblePos.previous();
    if (prevVisiblePos.isNull())
        return VisiblePositionRange();

    VisiblePosition startPosition = startOfLine(prevVisiblePos);

    // keep searching for a valid line start position.  Unless the textmarker is at the very beginning, there should
    // always be a valid line range.  However, startOfLine will return null for position next to a floating object,
    // since floating object doesn't really belong to any line.
    // This check will reposition the marker before the floating object, to ensure we get a line start.
    if (startPosition.isNull()) {
        while (startPosition.isNull() && prevVisiblePos.isNotNull()) {
            prevVisiblePos = prevVisiblePos.previous();
            startPosition = startOfLine(prevVisiblePos);
        }
    } else
        startPosition = updateAXLineStartForVisiblePosition(startPosition);

    VisiblePosition endPosition = endOfLine(prevVisiblePos);
    return VisiblePositionRange(startPosition, endPosition);
}

VisiblePositionRange AccessibilityObject::doAXRightLineTextMarkerRangeForTextMarker(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePositionRange();

    // make sure we move off of a line end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return VisiblePositionRange();

    VisiblePosition startPosition = startOfLine(nextVisiblePos);

    // fetch for a valid line start position
    if (startPosition.isNull() ) {
        startPosition = visiblePos;
        nextVisiblePos = nextVisiblePos.next();
    } else
        startPosition = updateAXLineStartForVisiblePosition(startPosition);

    VisiblePosition endPosition = endOfLine(nextVisiblePos);

    // as long as the position hasn't reached the end of the doc,  keep searching for a valid line end position
    // Unless the textmarker is at the very end, there should always be a valid line range.  However, endOfLine will
    // return null for position by a floating object, since floating object doesn't really belong to any line.
    // This check will reposition the marker after the floating object, to ensure we get a line end.
    while (endPosition.isNull() && nextVisiblePos.isNotNull()) {
        nextVisiblePos = nextVisiblePos.next();
        endPosition = endOfLine(nextVisiblePos);
    }

    return VisiblePositionRange(startPosition, endPosition);
}

VisiblePositionRange AccessibilityObject::doAXSentenceTextMarkerRangeForTextMarker(const VisiblePosition& visiblePos) const
{
    // FIXME: FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    VisiblePosition startPosition = startOfSentence(visiblePos);
    VisiblePosition endPosition = endOfSentence(startPosition);
    return VisiblePositionRange(startPosition, endPosition);
}

VisiblePositionRange AccessibilityObject::doAXParagraphTextMarkerRangeForTextMarker(const VisiblePosition& visiblePos) const
{
    VisiblePosition startPosition = startOfParagraph(visiblePos);
    VisiblePosition endPosition = endOfParagraph(startPosition);
    return VisiblePositionRange(startPosition, endPosition);
}

static VisiblePosition startOfStyleRange(const VisiblePosition visiblePos)
{
    RenderObject* renderer = visiblePos.deepEquivalent().node()->renderer();
    RenderObject* startRenderer = renderer;
    RenderStyle* style = renderer->style();

    // traverse backward by renderer to look for style change
    for (RenderObject* r = renderer->previousInPreOrder(); r; r = r->previousInPreOrder()) {
        // skip non-leaf nodes
        if (r->firstChild())
            continue;

        // stop at style change
        if (r->style() != style)
            break;

        // remember match
        startRenderer = r;
    }

    return VisiblePosition(startRenderer->node(), 0, VP_DEFAULT_AFFINITY);
}

static VisiblePosition endOfStyleRange(const VisiblePosition visiblePos)
{
    RenderObject* renderer = visiblePos.deepEquivalent().node()->renderer();
    RenderObject* endRenderer = renderer;
    RenderStyle* style = renderer->style();

    // traverse forward by renderer to look for style change
    for (RenderObject* r = renderer->nextInPreOrder(); r; r = r->nextInPreOrder()) {
        // skip non-leaf nodes
        if (r->firstChild())
            continue;

        // stop at style change
        if (r->style() != style)
            break;

        // remember match
        endRenderer = r;
    }

    return VisiblePosition(endRenderer->node(), maxDeepOffset(endRenderer->node()), VP_DEFAULT_AFFINITY);
}

VisiblePositionRange AccessibilityObject::doAXStyleTextMarkerRangeForTextMarker(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePositionRange();

    return VisiblePositionRange(startOfStyleRange(visiblePos), endOfStyleRange(visiblePos));
}

// NOTE: Consider providing this utility method as AX API
VisiblePositionRange AccessibilityObject::textMarkerRangeForRange(const PlainTextRange& range, RenderTextControl* textControl) const
{
    if (range.start + range.length > textControl->text().length())
        return VisiblePositionRange();

    VisiblePosition startPosition = textControl->visiblePositionForIndex(range.start);
    startPosition.setAffinity(DOWNSTREAM);
    VisiblePosition endPosition = textControl->visiblePositionForIndex(range.start + range.length);
    return VisiblePositionRange(startPosition, endPosition);
}

static String stringForReplacedNode(Node* replacedNode)
{
    // we should always be given a rendered node and a replaced node, but be safe
    // replaced nodes are either attachments (widgets) or images
    if (!replacedNode || !replacedNode->renderer() || !replacedNode->renderer()->isReplaced() || replacedNode->isTextNode()) {
        ASSERT_NOT_REACHED();
        return String();
    }

    // create an AX object, but skip it if it is not supposed to be seen
    AccessibilityObject* object = replacedNode->renderer()->document()->axObjectCache()->get(replacedNode->renderer());
    if (object->accessibilityIsIgnored())
        return String();

    return String(&objectReplacementCharacter, 1);
}

String AccessibilityObject::doAXStringForTextMarkerRange(const VisiblePositionRange& visiblePositionRange) const
{
    if (visiblePositionRange.isNull())
        return String();

    String resultString;
    TextIterator it(makeRange(visiblePositionRange.start, visiblePositionRange.end).get());
    while (!it.atEnd()) {
        // non-zero length means textual node, zero length means replaced node (AKA "attachments" in AX)
        if (it.length() != 0) {
            resultString.append(it.characters(), it.length());
        } else {
            // locate the node and starting offset for this replaced range
            int exception = 0;
            Node* node = it.range()->startContainer(exception);
            ASSERT(node == it.range()->endContainer(exception));
            int offset = it.range()->startOffset(exception);
            String attachmentString = stringForReplacedNode(node->childNode(offset));

            // append the replacement string
            if (!attachmentString.isNull())
                resultString.append(attachmentString);
        }
        it.advance();
    }

    return resultString.isEmpty() ? String() : resultString;
}

IntRect AccessibilityObject::doAXBoundsForTextMarkerRange(const VisiblePositionRange& visiblePositionRange) const
{
    if (visiblePositionRange.isNull())
        return IntRect();

    // Create a mutable VisiblePositionRange.
    VisiblePositionRange range(visiblePositionRange);
    IntRect rect1 = range.start.caretRect();
    IntRect rect2 = range.end.caretRect();

    // readjust for position at the edge of a line.  This is to exclude line rect that doesn't need to be accounted in the range bounds
    if (rect2.y() != rect1.y()) {
        VisiblePosition endOfFirstLine = endOfLine(range.start);
        if (range.start == endOfFirstLine) {
            range.start.setAffinity(DOWNSTREAM);
            rect1 = range.start.caretRect();
        }
        if (range.end == endOfFirstLine) {
            range.end.setAffinity(UPSTREAM);
            rect2 = range.end.caretRect();
        }
    }

    IntRect ourrect = rect1;
    ourrect.unite(rect2);

    // if the rectangle spans lines and contains multiple text chars, use the range's bounding box intead
    if (rect1.bottom() != rect2.bottom()) {
        RefPtr<Range> dataRange = makeRange(range.start, range.end);
        IntRect boundingBox = dataRange->boundingBox();
        String rangeString = plainText(dataRange.get());
        if (rangeString.length() > 1 && !boundingBox.isEmpty())
            ourrect = boundingBox;
    }

#if PLATFORM(MAC)
    return m_renderer->document()->view()->contentsToScreen(ourrect);
#else
    return ourrect;
#endif
}

int AccessibilityObject::doAXLengthForTextMarkerRange(const VisiblePositionRange& visiblePositionRange) const
{
    // FIXME: Multi-byte support
    String string = doAXStringForTextMarkerRange(visiblePositionRange);
    if (string.isEmpty())
        return -1;

    return string.length();
}

void AccessibilityObject::doSetAXSelectedTextMarkerRange(const VisiblePositionRange& textMarkerRange) const
{
    if (textMarkerRange.start.isNull() || textMarkerRange.end.isNull())
        return;

    // make selection and tell the document to use it
    Selection newSelection = Selection(textMarkerRange.start, textMarkerRange.end);
    m_renderer->document()->frame()->selectionController()->setSelection(newSelection);
}

VisiblePosition AccessibilityObject::doAXTextMarkerForPosition(const IntPoint& point) const
{
    // convert absolute point to view coordinates
    FrameView* frameView = m_renderer->document()->topDocument()->renderer()->view()->frameView();
    RenderObject* renderer = topRenderer();
    Node* innerNode = 0;

    // locate the node containing the point
    IntPoint pointResult;
    while (1) {
        IntPoint ourpoint;
#if PLATFORM(MAC)
        ourpoint = frameView->screenToContents(point);
#else
        ourpoint = point;
#endif
        HitTestRequest request(true, true);
        HitTestResult result(ourpoint);
        renderer->layer()->hitTest(request, result);
        innerNode = result.innerNode();
        if (!innerNode || !innerNode->renderer())
            return VisiblePosition();

        pointResult = result.localPoint();

        // done if hit something other than a widget
        renderer = innerNode->renderer();
        if (!renderer->isWidget())
            break;

        // descend into widget (FRAME, IFRAME, OBJECT...)
        Widget* widget = static_cast<RenderWidget*>(renderer)->widget();
        if (!widget || !widget->isFrameView())
            break;
        Frame* frame = static_cast<FrameView*>(widget)->frame();
        if (!frame)
            break;
        Document* document = frame->document();
        if (!document)
            break;
        renderer = document->renderer();
        frameView = static_cast<FrameView*>(widget);
    }

    return innerNode->renderer()->positionForPoint(pointResult);
}

VisiblePosition AccessibilityObject::doAXNextTextMarkerForTextMarker(const VisiblePosition& visiblePos) const
{
    return visiblePos.next();
}

VisiblePosition AccessibilityObject::doAXPreviousTextMarkerForTextMarker(const VisiblePosition& visiblePos) const
{
    return visiblePos.previous();
}

VisiblePosition AccessibilityObject::doAXNextWordEndTextMarkerForTextMarker(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a word end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return VisiblePosition();

    return endOfWord(nextVisiblePos, LeftWordIfOnBoundary);
}

VisiblePosition AccessibilityObject::doAXPreviousWordStartTextMarkerForTextMarker(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a word start
    VisiblePosition prevVisiblePos = visiblePos.previous();
    if (prevVisiblePos.isNull())
        return VisiblePosition();

    return startOfWord(prevVisiblePos, RightWordIfOnBoundary);
}

VisiblePosition AccessibilityObject::doAXNextLineEndTextMarkerForTextMarker(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // to make sure we move off of a line end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return VisiblePosition();

    VisiblePosition endPosition = endOfLine(nextVisiblePos);

    // as long as the position hasn't reached the end of the doc,  keep searching for a valid line end position
    // There are cases like when the position is next to a floating object that'll return null for end of line. This code will avoid returning null.
    while (endPosition.isNull() && nextVisiblePos.isNotNull()) {
        nextVisiblePos = nextVisiblePos.next();
        endPosition = endOfLine(nextVisiblePos);
    }

    return endPosition;
}

VisiblePosition AccessibilityObject::doAXPreviousLineStartTextMarkerForTextMarker(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a line start
    VisiblePosition prevVisiblePos = visiblePos.previous();
    if (prevVisiblePos.isNull())
        return VisiblePosition();

    VisiblePosition startPosition = startOfLine(prevVisiblePos);

    // as long as the position hasn't reached the beginning of the doc,  keep searching for a valid line start position
    // There are cases like when the position is next to a floating object that'll return null for start of line. This code will avoid returning null.
    if (startPosition.isNull()) {
        while (startPosition.isNull() && prevVisiblePos.isNotNull()) {
            prevVisiblePos = prevVisiblePos.previous();
            startPosition = startOfLine(prevVisiblePos);
        }
    } else
        startPosition = updateAXLineStartForVisiblePosition(startPosition);

    return startPosition;
}

VisiblePosition AccessibilityObject::doAXNextSentenceEndTextMarkerForTextMarker(const VisiblePosition& visiblePos) const
{
    // FIXME: FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a sentence end
    VisiblePosition nextVisiblePos = visiblePos.next();
    if (nextVisiblePos.isNull())
        return VisiblePosition();

    // an empty line is considered a sentence. If it's skipped, then the sentence parser will not
    // see this empty line.  Instead, return the end position of the empty line.
    VisiblePosition endPosition;
    String lineString = plainText(makeRange(startOfLine(visiblePos), endOfLine(visiblePos)).get());
    if (lineString.isEmpty())
        endPosition = nextVisiblePos;
    else
        endPosition = endOfSentence(nextVisiblePos);

    return endPosition;
}

VisiblePosition AccessibilityObject::doAXPreviousSentenceStartTextMarkerForTextMarker(const VisiblePosition& visiblePos) const
{
    // FIXME: FO 2 IMPLEMENT (currently returns incorrect answer)
    // Related? <rdar://problem/3927736> Text selection broken in 8A336
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a sentence start
    VisiblePosition previousVisiblePos = visiblePos.previous();
    if (previousVisiblePos.isNull())
        return VisiblePosition();

    // treat empty line as a separate sentence.
    VisiblePosition startPosition;
    String lineString = plainText(makeRange(startOfLine(previousVisiblePos), endOfLine(previousVisiblePos)).get());
    if (lineString.isEmpty())
        startPosition = previousVisiblePos;
    else
        startPosition = startOfSentence(previousVisiblePos);

    return startPosition;
}

VisiblePosition AccessibilityObject::doAXNextParagraphEndTextMarkerForTextMarker(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a paragraph end
    VisiblePosition nextPos = visiblePos.next();
    if (nextPos.isNull())
        return VisiblePosition();

    return endOfParagraph(nextPos);
}

VisiblePosition AccessibilityObject::doAXPreviousParagraphStartTextMarkerForTextMarker(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return VisiblePosition();

    // make sure we move off of a paragraph start
    VisiblePosition previousPos = visiblePos.previous();
    if (previousPos.isNull())
        return VisiblePosition();

    return startOfParagraph(previousPos);
}

// NOTE: Consider providing this utility method as AX API
VisiblePosition AccessibilityObject::textMarkerForIndex(unsigned indexValue, bool lastIndexOK) const
{
    if (!isTextControl())
        return VisiblePosition();

    RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);

    // lastIndexOK specifies whether the position after the last character is acceptable
    if (indexValue >= textControl->text().length()) {
        if (!lastIndexOK || indexValue > textControl->text().length())
            return VisiblePosition();
    }
    VisiblePosition position = textControl->visiblePositionForIndex(indexValue);
    position.setAffinity(DOWNSTREAM);
    return position;
}

AccessibilityObject* AccessibilityObject::doAXUIElementForTextMarker(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return 0;

    RenderObject* obj = visiblePos.deepEquivalent().node()->renderer();
    if (!obj)
        return 0;

    return obj->document()->axObjectCache()->get(obj);
}

unsigned AccessibilityObject::doAXLineForTextMarker(const VisiblePosition& visiblePos) const
{
    if (visiblePos.isNull())
        return 0;

    unsigned lineCount = 0;
    VisiblePosition currentVisiblePos = visiblePos;
    VisiblePosition savedVisiblePos;

    // move up until we get to the top
    // FIXME: This only takes us to the top of the rootEditableElement, not the top of the
    // top document.
    while (currentVisiblePos.isNotNull() && !(inSameLine(currentVisiblePos, savedVisiblePos))) {
        lineCount += 1;
        savedVisiblePos = currentVisiblePos;
        VisiblePosition prevVisiblePos = previousLinePosition(visiblePos, 0);
        currentVisiblePos = prevVisiblePos;
    }

    return lineCount - 1;
}

// NOTE: Consider providing this utility method as AX API
AccessibilityObject::PlainTextRange AccessibilityObject::rangeForTextMarkerRange(const VisiblePositionRange& positionRange) const
{
    int index1 = indexForTextMarker(positionRange.start);
    int index2 = indexForTextMarker(positionRange.end);
    if (index1 < 0 || index2 < 0 || index1 > index2)
        return PlainTextRange();

    return PlainTextRange(index1, index2 - index1);
}

// NOTE: Consider providing this utility method as AX API
int AccessibilityObject::indexForTextMarker(const VisiblePosition& position) const
{
    if (!isTextControl())
        return -1;

    Node* node = position.deepEquivalent().node();
    if (!node)
        return -1;

    RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
    for (RenderObject* renderer = node->renderer(); renderer && renderer->element(); renderer = renderer->parent()) {
        if (renderer == textControl)
            return textControl->indexForVisiblePosition(position);
    }

    return -1;
}

// Given a line number, the range of characters of the text associated with this accessibility
// object that contains the line number.
AccessibilityObject::PlainTextRange AccessibilityObject::doAXRangeForLine(unsigned lineNumber) const
{
    if (!isTextControl())
        return PlainTextRange();

    // iterate to the specified line
    RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
    VisiblePosition visiblePos = textControl->visiblePositionForIndex(0);
    VisiblePosition savedVisiblePos;
    for (unsigned lineCount = lineNumber; lineCount != 0; lineCount -= 1) {
        savedVisiblePos = visiblePos;
        visiblePos = nextLinePosition(visiblePos, 0);
        if (visiblePos.isNull() || visiblePos == savedVisiblePos)
            return PlainTextRange();
    }

    // make a caret selection for the marker position, then extend it to the line
    // NOTE: ignores results of selectionController.modify because it returns false when
    // starting at an empty line.  The resulting selection in that case
    // will be a caret at visiblePos.
    SelectionController selectionController;
    selectionController.setSelection(Selection(visiblePos));
    selectionController.modify(SelectionController::EXTEND, SelectionController::LEFT, LineBoundary);
    selectionController.modify(SelectionController::EXTEND, SelectionController::RIGHT, LineBoundary);

    // calculate the indices for the selection start and end
    VisiblePosition startPosition = selectionController.selection().visibleStart();
    VisiblePosition endPosition = selectionController.selection().visibleEnd();
    int index1 = textControl->indexForVisiblePosition(startPosition);
    int index2 = textControl->indexForVisiblePosition(endPosition);

    // add one to the end index for a line break not caused by soft line wrap (to match AppKit)
    if (endPosition.affinity() == DOWNSTREAM && endPosition.next().isNotNull())
        index2 += 1;

    // return nil rather than an zero-length range (to match AppKit)
    if (index1 == index2)
        return PlainTextRange();

    return PlainTextRange(index1, index2 - index1);
}

// The composed character range in the text associated with this accessibility object that
// is specified by the given screen coordinates. This parameterized attribute returns the
// complete range of characters (including surrogate pairs of multi-byte glyphs) at the given
// screen coordinates.
// NOTE: This varies from AppKit when the point is below the last line. AppKit returns an
// an error in that case. We return textControl->text().length(), 1. Does this matter?
AccessibilityObject::PlainTextRange AccessibilityObject::doAXRangeForPosition(const IntPoint& point) const
{
    int index = indexForTextMarker(doAXTextMarkerForPosition(point));
    if (index < 0)
        return PlainTextRange();

    return PlainTextRange(index, 1);
}

// The composed character range in the text associated with this accessibility object that
// is specified by the given index value. This parameterized attribute returns the complete
// range of characters (including surrogate pairs of multi-byte glyphs) at the given index.
AccessibilityObject::PlainTextRange AccessibilityObject::doAXRangeForIndex(unsigned index) const
{
    if (!isTextControl())
        return PlainTextRange();

    RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
    String text = textControl->text();
    if (!text.length() || index > text.length() - 1)
        return PlainTextRange();

    return PlainTextRange(index, 1);
}

// Given a character index, the range of text associated with this accessibility object
// over which the style in effect at that character index applies.
AccessibilityObject::PlainTextRange AccessibilityObject::doAXStyleRangeForIndex(unsigned index) const
{
    VisiblePositionRange textMarkerRange = doAXStyleTextMarkerRangeForTextMarker(textMarkerForIndex(index, false));
    return rangeForTextMarkerRange(textMarkerRange);
}

// A substring of the text associated with this accessibility object that is
// specified by the given character range.
String AccessibilityObject::doAXStringForRange(const PlainTextRange& range) const
{
    if (isPasswordField())
        return String();

    if (range.length == 0)
        return "";

    if (!isTextControl())
        return String();

    RenderTextControl* textControl = static_cast<RenderTextControl*>(m_renderer);
    String text = textControl->text();
    if (range.start + range.length > text.length())
        return String();

    return text.substring(range.start, range.length);
}

// The bounding rectangle of the text associated with this accessibility object that is
// specified by the given range. This is the bounding rectangle a sighted user would see
// on the display screen, in pixels.
IntRect AccessibilityObject::doAXBoundsForRange(const PlainTextRange& range) const
{
    if (isTextControl())
        return doAXBoundsForTextMarkerRange(textMarkerRangeForRange(range, static_cast<RenderTextControl*>(m_renderer)));
    return IntRect();
}

// Given an indexed character, the line number of the text associated with this accessibility
// object that contains the character.
unsigned AccessibilityObject::doAXLineForIndex(unsigned index)
{
    return doAXLineForTextMarker(textMarkerForIndex(index, false));
}

AccessibilityObject* AccessibilityObject::doAccessibilityHitTest(const IntPoint& point) const
{
    if (!m_renderer)
        return 0;

    HitTestRequest request(true, true);
    HitTestResult result = HitTestResult(point);
    m_renderer->layer()->hitTest(request, result);
    if (!result.innerNode())
        return 0;
    Node* node = result.innerNode()->shadowAncestorNode();
    RenderObject* obj = node->renderer();
    if (!obj)
        return 0;

    return obj->document()->axObjectCache()->get(obj);
}

AccessibilityObject* AccessibilityObject::focusedUIElement() const
{
    // get the focused node in the page
    Page* page = m_renderer->document()->page();
    if (!page)
        return 0;

    Document* focusedDocument = page->focusController()->focusedOrMainFrame()->document();
    Node* focusedNode = focusedDocument->focusedNode();
    if (!focusedNode || !focusedNode->renderer())
        focusedNode = focusedDocument;

    AccessibilityObject* obj = focusedNode->renderer()->document()->axObjectCache()->get(focusedNode->renderer());

    // the HTML element, for example, is focusable but has an AX object that is ignored
    if (obj->accessibilityIsIgnored())
        obj = obj->parentObjectUnignored();

    return obj;
}

AccessibilityObject* AccessibilityObject::observableObject() const
{
    for (RenderObject* renderer = m_renderer; renderer && renderer->element(); renderer = renderer->parent()) {
        if (renderer->isTextField() || renderer->isTextArea())
            return renderer->document()->axObjectCache()->get(renderer);
    }

    return 0;
}

AccessibilityRole AccessibilityObject::roleValue() const
{
    if (!m_renderer)
        return UnknownRole;

    if (m_areaElement)
        return WebCoreLinkRole;
    if (m_renderer->element() && m_renderer->element()->isLink()) {
        if (isImage())
            return ImageMapRole;
        return WebCoreLinkRole;
    }
    if (m_renderer->isListMarker())
        return ListMarkerRole;
    if (m_renderer->element() && m_renderer->element()->hasTagName(buttonTag))
        return ButtonRole;
    if (m_renderer->isText())
        return StaticTextRole;
    if (isImage()) {
        if (isImageButton())
            return ButtonRole;
        return ImageRole;
    }
    if (isWebArea())
        return WebAreaRole;

    if (m_renderer->isTextField())
        return TextFieldRole;

    if (m_renderer->isTextArea())
        return TextAreaRole;

    if (m_renderer->element() && m_renderer->element()->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(m_renderer->element());
        if (input->inputType() == HTMLInputElement::CHECKBOX)
            return CheckBoxRole;
        if (input->inputType() == HTMLInputElement::RADIO)
            return RadioButtonRole;
        if (input->isTextButton())
            return ButtonRole;
    }

    if (m_renderer->isMenuList())
        return PopUpButtonRole;

    if (isHeading())
        return HeadingRole;

    if (m_renderer->isBlockFlow())
        return GroupRole;

    return UnknownRole;
}

bool AccessibilityObject::canSetFocusAttribute() const
{
    // NOTE: It would be more accurate to ask the document whether setFocusedNode() would
    // do anything.  For example, it setFocusedNode() will do nothing if the current focused
    // node will not relinquish the focus.
    if (!m_renderer->element() || !m_renderer->element()->isEnabled())
        return false;

    switch (roleValue()) {
    case WebCoreLinkRole:
    case TextFieldRole:
    case TextAreaRole:
    case ButtonRole:
    case PopUpButtonRole:
    case CheckBoxRole:
    case RadioButtonRole:
        return true;
    case UnknownRole:
    case SliderRole:
    case TabGroupRole:
    case StaticTextRole:
    case ScrollAreaRole:
    case MenuButtonRole:
    case TableRole:
    case ApplicationRole:
    case GroupRole:
    case RadioGroupRole:
    case ListRole:
    case ScrollBarRole:
    case ValueIndicatorRole:
    case ImageRole:
    case MenuBarRole:
    case MenuRole:
    case MenuItemRole:
    case ColumnRole:
    case RowRole:
    case ToolbarRole:
    case BusyIndicatorRole:
    case ProgressIndicatorRole:
    case WindowRole:
    case DrawerRole:
    case SystemWideRole:
    case OutlineRole:
    case IncrementorRole:
    case BrowserRole:
    case ComboBoxRole:
    case SplitGroupRole:
    case SplitterRole:
    case ColorWellRole:
    case GrowAreaRole:
    case SheetRole:
    case HelpTagRole:
    case MatteRole:
    case RulerRole:
    case RulerMarkerRole:
    case SortButtonRole:
    case LinkRole:
    case DisclosureTriangleRole:
    case GridRole:
    case ImageMapRole:
    case ListMarkerRole:
    case WebAreaRole:
    case HeadingRole:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool AccessibilityObject::canSetValueAttribute() const
{
    return isTextControl();
}

bool AccessibilityObject::canSetTextRangeAttributes() const
{
    return isTextControl();
}

void AccessibilityObject::childrenChanged()
{
    clearChildren();

    if (accessibilityIsIgnored())
        parentObject()->childrenChanged();
}

void AccessibilityObject::clearChildren()
{
    m_children.clear();
}

bool AccessibilityObject::hasChildren() const
{
    return m_children.size();
}

void AccessibilityObject::addChildren()
{
    // nothing to add if there is no RenderObject
    if (!m_renderer)
        return;

    // add all unignored acc children
    for (RefPtr<AccessibilityObject> obj = firstChild(); obj; obj = obj->nextSibling()) {
        if (obj->accessibilityIsIgnored()) {
            obj->addChildren();
            Vector<RefPtr<AccessibilityObject> >children = obj->children();
            unsigned length = children.size();
            for (unsigned i = 0; i < length; ++i)
                m_children.append(children[i]);
        } else
            m_children.append(obj);
    }

    // for a RenderImage, add the <area> elements as individual accessibility objects
    if (isRenderImage() && !m_areaElement) {
        HTMLMapElement* map = static_cast<RenderImage*>(m_renderer)->imageMap();
        if (map) {
            for (Node* current = map->firstChild(); current; current = current->traverseNextNode(map)) {
                // add an <area> element for this child if it has a link
                // NOTE: can't cache these because they all have the same renderer, which is the cache key, right?
                // plus there may be little reason to since they are being added to the handy array
                if (current->isLink()) {
                    RefPtr<AccessibilityObject> obj = new AccessibilityObject(m_renderer);
                    obj->m_areaElement = static_cast<HTMLAreaElement*>(current);
                    m_children.append(obj);
                }
            }
        }
    }
}

unsigned AccessibilityObject::axObjectID() const
{
    return m_id;
}

void AccessibilityObject::setAXObjectID(unsigned axObjectID)
{
    m_id = axObjectID;
}

void AccessibilityObject::removeAXObjectID()
{
    if (!m_id)
        return;

    m_renderer->document()->axObjectCache()->removeAXID(this);
}

} // namespace WebCore
