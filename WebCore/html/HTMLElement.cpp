/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "HTMLElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "DocumentFragment.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "HTMLBRElement.h"
#include "HTMLDocument.h"
#include "HTMLElementFactory.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLTokenizer.h"
#include "RenderWordBreak.h"
#include "Settings.h"
#include "Text.h"
#include "TextIterator.h"
#include "XMLTokenizer.h"
#include "markup.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

using std::min;
using std::max;

HTMLElement::HTMLElement(const QualifiedName& tagName, Document *doc)
    : StyledElement(tagName, doc)
{
}

HTMLElement::~HTMLElement()
{
}

String HTMLElement::nodeName() const
{
    // FIXME: Would be nice to have an atomicstring lookup based off uppercase chars that does not have to copy
    // the string on a hit in the hash.
    if (document()->isHTMLDocument())
        return tagQName().localName().domString().upper();
    return Element::nodeName();
}
    
HTMLTagStatus HTMLElement::endTagRequirement() const
{
    if (hasLocalName(wbrTag))
        return TagStatusForbidden;
    if (hasLocalName(dtTag) || hasLocalName(ddTag))
        return TagStatusOptional;

    // Same values as <span>.  This way custom tag name elements will behave like inline spans.
    return TagStatusRequired;
}

int HTMLElement::tagPriority() const
{
    if (hasLocalName(wbrTag))
        return 0;
    if (hasLocalName(addressTag) || hasLocalName(ddTag) || hasLocalName(dtTag) || hasLocalName(noscriptTag))
        return 3;
    if (hasLocalName(centerTag) || hasLocalName(nobrTag))
        return 5;
    if (hasLocalName(noembedTag) || hasLocalName(noframesTag))
        return 10;

    // Same values as <span>.  This way custom tag name elements will behave like inline spans.
    return 1;
}

PassRefPtr<Node> HTMLElement::cloneNode(bool deep)
{
    RefPtr<HTMLElement> clone = HTMLElementFactory::createHTMLElement(tagQName().localName(), document(), 0, false);
    if (!clone)
        return 0;

    if (namedAttrMap)
        *clone->attributes() = *namedAttrMap;

    clone->copyNonAttributeProperties(this);

    if (deep)
        cloneChildNodes(clone.get());

    return clone.release();
}

bool HTMLElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == alignAttr ||
        attrName == contenteditableAttr) {
        result = eUniversal;
        return false;
    }
    if (attrName == dirAttr) {
        result = hasLocalName(bdoTag) ? eBDO : eUniversal;
        return false;
    }

    return StyledElement::mapToEntry(attrName, result);
}
    
void HTMLElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == idAttr || attr->name() == classAttr || attr->name() == styleAttr)
        return StyledElement::parseMappedAttribute(attr);

    String indexstring;
    if (attr->name() == alignAttr) {
        if (equalIgnoringCase(attr->value(), "middle"))
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, "center");
        else
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, attr->value());
    } else if (attr->name() == contenteditableAttr) {
        setContentEditable(attr);
    } else if (attr->name() == tabindexAttr) {
        indexstring = getAttribute(tabindexAttr);
        if (indexstring.length())
            // Clamp tabindex to the range of 'short' to match Firefox's behavior.
            setTabIndex(max(static_cast<int>(std::numeric_limits<short>::min()), min(indexstring.toInt(), static_cast<int>(std::numeric_limits<short>::max()))));
    } else if (attr->name() == langAttr) {
        // FIXME: Implement
    } else if (attr->name() == dirAttr) {
        addCSSProperty(attr, CSS_PROP_DIRECTION, attr->value());
        addCSSProperty(attr, CSS_PROP_UNICODE_BIDI, hasLocalName(bdoTag) ? CSS_VAL_BIDI_OVERRIDE : CSS_VAL_EMBED);
    }
// standard events
    else if (attr->name() == onclickAttr) {
        setHTMLEventListener(clickEvent, attr);
    } else if (attr->name() == oncontextmenuAttr) {
        setHTMLEventListener(contextmenuEvent, attr);
    } else if (attr->name() == ondblclickAttr) {
        setHTMLEventListener(dblclickEvent, attr);
    } else if (attr->name() == onmousedownAttr) {
        setHTMLEventListener(mousedownEvent, attr);
    } else if (attr->name() == onmousemoveAttr) {
        setHTMLEventListener(mousemoveEvent, attr);
    } else if (attr->name() == onmouseoutAttr) {
        setHTMLEventListener(mouseoutEvent, attr);
    } else if (attr->name() == onmouseoverAttr) {
        setHTMLEventListener(mouseoverEvent, attr);
    } else if (attr->name() == onmouseupAttr) {
        setHTMLEventListener(mouseupEvent, attr);
    } else if (attr->name() == onmousewheelAttr) {
        setHTMLEventListener(mousewheelEvent, attr);
    } else if (attr->name() == onfocusAttr) {
        setHTMLEventListener(focusEvent, attr);
    } else if (attr->name() == onblurAttr) {
        setHTMLEventListener(blurEvent, attr);
    } else if (attr->name() == onkeydownAttr) {
        setHTMLEventListener(keydownEvent, attr);
    } else if (attr->name() == onkeypressAttr) {
        setHTMLEventListener(keypressEvent, attr);
    } else if (attr->name() == onkeyupAttr) {
        setHTMLEventListener(keyupEvent, attr);
    } else if (attr->name() == onscrollAttr) {
        setHTMLEventListener(scrollEvent, attr);
    } else if (attr->name() == onbeforecutAttr) {
        setHTMLEventListener(beforecutEvent, attr);
    } else if (attr->name() == oncutAttr) {
        setHTMLEventListener(cutEvent, attr);
    } else if (attr->name() == onbeforecopyAttr) {
        setHTMLEventListener(beforecopyEvent, attr);
    } else if (attr->name() == oncopyAttr) {
        setHTMLEventListener(copyEvent, attr);
    } else if (attr->name() == onbeforepasteAttr) {
        setHTMLEventListener(beforepasteEvent, attr);
    } else if (attr->name() == onpasteAttr) {
        setHTMLEventListener(pasteEvent, attr);
    } else if (attr->name() == ondragenterAttr) {
        setHTMLEventListener(dragenterEvent, attr);
    } else if (attr->name() == ondragoverAttr) {
        setHTMLEventListener(dragoverEvent, attr);
    } else if (attr->name() == ondragleaveAttr) {
        setHTMLEventListener(dragleaveEvent, attr);
    } else if (attr->name() == ondropAttr) {
        setHTMLEventListener(dropEvent, attr);
    } else if (attr->name() == ondragstartAttr) {
        setHTMLEventListener(dragstartEvent, attr);
    } else if (attr->name() == ondragAttr) {
        setHTMLEventListener(dragEvent, attr);
    } else if (attr->name() == ondragendAttr) {
        setHTMLEventListener(dragendEvent, attr);
    } else if (attr->name() == onselectstartAttr) {
        setHTMLEventListener(selectstartEvent, attr);
    } else if (attr->name() == onsubmitAttr) {
        setHTMLEventListener(submitEvent, attr);
    } else if (attr->name() == onerrorAttr) {
        setHTMLEventListener(errorEvent, attr);
    }
}

String HTMLElement::innerHTML() const
{
    return createMarkup(this, ChildrenOnly);
}

String HTMLElement::outerHTML() const
{
    return createMarkup(this);
}

PassRefPtr<DocumentFragment> HTMLElement::createContextualFragment(const String &html)
{
    // the following is in accordance with the definition as used by IE
    if (endTagRequirement() == TagStatusForbidden)
        return 0;

    if (hasLocalName(colTag) || hasLocalName(colgroupTag) || hasLocalName(framesetTag) ||
        hasLocalName(headTag) || hasLocalName(styleTag) || hasLocalName(titleTag))
        return 0;

    RefPtr<DocumentFragment> fragment = new DocumentFragment(document());
    
    if (document()->isHTMLDocument())
         parseHTMLDocumentFragment(html, fragment.get());
    else {
        if (!parseXMLDocumentFragment(html, fragment.get(), this))
            // FIXME: We should propagate a syntax error exception out here.
            return 0;
    }

    // Exceptions are ignored because none ought to happen here.
    int ignoredExceptionCode;

    // we need to pop <html> and <body> elements and remove <head> to
    // accommodate folks passing complete HTML documents to make the
    // child of an element.

    RefPtr<Node> nextNode;
    for (RefPtr<Node> node = fragment->firstChild(); node; node = nextNode) {
        nextNode = node->nextSibling();
        if (node->hasTagName(htmlTag) || node->hasTagName(bodyTag)) {
            Node *firstChild = node->firstChild();
            if (firstChild)
                nextNode = firstChild;
            RefPtr<Node> nextChild;
            for (RefPtr<Node> child = firstChild; child; child = nextChild) {
                nextChild = child->nextSibling();
                node->removeChild(child.get(), ignoredExceptionCode);
                ASSERT(!ignoredExceptionCode);
                fragment->insertBefore(child, node.get(), ignoredExceptionCode);
                ASSERT(!ignoredExceptionCode);
            }
            fragment->removeChild(node.get(), ignoredExceptionCode);
            ASSERT(!ignoredExceptionCode);
        } else if (node->hasTagName(headTag)) {
            fragment->removeChild(node.get(), ignoredExceptionCode);
            ASSERT(!ignoredExceptionCode);
        }
    }

    return fragment.release();
}

static inline bool hasOneChild(ContainerNode* node)
{
    Node* firstChild = node->firstChild();
    return firstChild && !firstChild->nextSibling();
}

static inline bool hasOneTextChild(ContainerNode* node)
{
    return hasOneChild(node) && node->firstChild()->isTextNode();
}

static void replaceChildrenWithFragment(HTMLElement* element, PassRefPtr<DocumentFragment> fragment, ExceptionCode& ec)
{
    if (!fragment->firstChild()) {
        element->removeChildren();
        return;
    }

    if (hasOneTextChild(element) && hasOneTextChild(fragment.get())) {
        static_cast<Text*>(element->firstChild())->setData(static_cast<Text*>(fragment->firstChild())->string(), ec);
        return;
    }

    if (hasOneChild(element)) {
        element->replaceChild(fragment, element->firstChild(), ec);
        return;
    }

    element->removeChildren();
    element->appendChild(fragment, ec);
}

static void replaceChildrenWithText(HTMLElement* element, const String& text, ExceptionCode& ec)
{
    if (hasOneTextChild(element)) {
        static_cast<Text*>(element->firstChild())->setData(text, ec);
        return;
    }

    RefPtr<Text> textNode = new Text(element->document(), text);

    if (hasOneChild(element)) {
        element->replaceChild(textNode.release(), element->firstChild(), ec);
        return;
    }

    element->removeChildren();
    element->appendChild(textNode.release(), ec);
}

void HTMLElement::setInnerHTML(const String& html, ExceptionCode& ec)
{
    RefPtr<DocumentFragment> fragment = createContextualFragment(html);
    if (!fragment) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    replaceChildrenWithFragment(this, fragment.release(), ec);
}

void HTMLElement::setOuterHTML(const String& html, ExceptionCode& ec)
{
    Node* p = parent();
    if (!p || !p->isHTMLElement()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    HTMLElement* parent = static_cast<HTMLElement*>(p);
    RefPtr<DocumentFragment> fragment = parent->createContextualFragment(html);
    if (!fragment) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // FIXME: Why doesn't this have code to merge neighboring text nodes the way setOuterText does?

    parent->replaceChild(fragment.release(), this, ec);
}

void HTMLElement::setInnerText(const String& text, ExceptionCode& ec)
{
    // follow the IE specs about when this is allowed
    if (endTagRequirement() == TagStatusForbidden) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
    if (hasLocalName(colTag) || hasLocalName(colgroupTag) || hasLocalName(framesetTag) ||
        hasLocalName(headTag) || hasLocalName(htmlTag) || hasLocalName(tableTag) || 
        hasLocalName(tbodyTag) || hasLocalName(tfootTag) || hasLocalName(theadTag) ||
        hasLocalName(trTag)) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // FIXME: This doesn't take whitespace collapsing into account at all.

    if (!text.contains('\n') && !text.contains('\r')) {
        if (text.isEmpty()) {
            removeChildren();
            return;
        }
        replaceChildrenWithText(this, text, ec);
        return;
    }

    // FIXME: Do we need to be able to detect preserveNewline style even when there's no renderer?
    // FIXME: Can the renderer be out of date here? Do we need to call updateRendering?
    // For example, for the contents of textarea elements that are display:none?
    RenderObject* r = renderer();
    if (r && r->style()->preserveNewline()) {
        if (!text.contains('\r')) {
            replaceChildrenWithText(this, text, ec);
            return;
        }
        String textWithConsistentLineBreaks = text;
        textWithConsistentLineBreaks.replace("\r\n", "\n");
        textWithConsistentLineBreaks.replace('\r', '\n');
        replaceChildrenWithText(this, textWithConsistentLineBreaks, ec);
        return;
    }

    // Add text nodes and <br> elements.
    ec = 0;
    RefPtr<DocumentFragment> fragment = new DocumentFragment(document());
    int lineStart = 0;
    UChar prev = 0;
    int length = text.length();
    for (int i = 0; i < length; ++i) {
        UChar c = text[i];
        if (c == '\n' || c == '\r') {
            if (i > lineStart) {
                fragment->appendChild(new Text(document(), text.substring(lineStart, i - lineStart)), ec);
                if (ec)
                    return;
            }
            if (!(c == '\n' && i != 0 && prev == '\r')) {
                fragment->appendChild(new HTMLBRElement(document()), ec);
                if (ec)
                    return;
            }
            lineStart = i + 1;
        }
        prev = c;
    }
    if (length > lineStart)
        fragment->appendChild(new Text(document(), text.substring(lineStart, length - lineStart)), ec);
    replaceChildrenWithFragment(this, fragment.release(), ec);
}

void HTMLElement::setOuterText(const String &text, ExceptionCode& ec)
{
    // follow the IE specs about when this is allowed
    if (endTagRequirement() == TagStatusForbidden) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
    if (hasLocalName(colTag) || hasLocalName(colgroupTag) || hasLocalName(framesetTag) ||
        hasLocalName(headTag) || hasLocalName(htmlTag) || hasLocalName(tableTag) || 
        hasLocalName(tbodyTag) || hasLocalName(tfootTag) || hasLocalName(theadTag) ||
        hasLocalName(trTag)) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    Node* parent = parentNode();
    if (!parent) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // FIXME: This creates a new text node even when the text is empty.
    // FIXME: This creates a single text node even when the text has CR and LF
    // characters in it. Instead it should create <br> elements.
    RefPtr<Text> t = new Text(document(), text);
    ec = 0;
    parent->replaceChild(t, this, ec);
    if (ec)
        return;

    // is previous node a text node? if so, merge into it
    Node* prev = t->previousSibling();
    if (prev && prev->isTextNode()) {
        Text* textPrev = static_cast<Text*>(prev);
        textPrev->appendData(t->data(), ec);
        if (ec)
            return;
        t->remove(ec);
        if (ec)
            return;
        t = textPrev;
    }

    // is next node a text node? if so, merge it in
    Node* next = t->nextSibling();
    if (next && next->isTextNode()) {
        Text* textNext = static_cast<Text*>(next);
        t->appendData(textNext->data(), ec);
        if (ec)
            return;
        textNext->remove(ec);
        if (ec)
            return;
    }
}

void HTMLElement::addHTMLAlignment(MappedAttribute* attr)
{
    // vertical alignment with respect to the current baseline of the text
    // right or left means floating images
    int propfloat = -1;
    int propvalign = -1;
    const AtomicString& alignment = attr->value();
    if (equalIgnoringCase(alignment, "absmiddle")) {
        propvalign = CSS_VAL_MIDDLE;
    } else if (equalIgnoringCase(alignment, "absbottom")) {
        propvalign = CSS_VAL_BOTTOM;
    } else if (equalIgnoringCase(alignment, "left")) {
        propfloat = CSS_VAL_LEFT;
        propvalign = CSS_VAL_TOP;
    } else if (equalIgnoringCase(alignment, "right")) {
        propfloat = CSS_VAL_RIGHT;
        propvalign = CSS_VAL_TOP;
    } else if (equalIgnoringCase(alignment, "top")) {
        propvalign = CSS_VAL_TOP;
    } else if (equalIgnoringCase(alignment, "middle")) {
        propvalign = CSS_VAL__WEBKIT_BASELINE_MIDDLE;
    } else if (equalIgnoringCase(alignment, "center")) {
        propvalign = CSS_VAL_MIDDLE;
    } else if (equalIgnoringCase(alignment, "bottom")) {
        propvalign = CSS_VAL_BASELINE;
    } else if (equalIgnoringCase(alignment, "texttop")) {
        propvalign = CSS_VAL_TEXT_TOP;
    }
    
    if ( propfloat != -1 )
        addCSSProperty( attr, CSS_PROP_FLOAT, propfloat );
    if ( propvalign != -1 )
        addCSSProperty( attr, CSS_PROP_VERTICAL_ALIGN, propvalign );
}

bool HTMLElement::isFocusable() const
{
    return isContentEditable() && parent() && !parent()->isContentEditable();
}

bool HTMLElement::isContentEditable() const 
{
    if (document()->frame() && document()->frame()->isContentEditable())
        return true;

    document()->updateRendering();

    if (!renderer()) {
        if (parentNode())
            return parentNode()->isContentEditable();
        else
            return false;
    }
    
    return renderer()->style()->userModify() == READ_WRITE || renderer()->style()->userModify() == READ_WRITE_PLAINTEXT_ONLY;
}

bool HTMLElement::isContentRichlyEditable() const
{
    if (document()->frame() && document()->frame()->isContentEditable())
        return true;

    document()->updateRendering();

    if (!renderer()) {
        if (parentNode())
            return parentNode()->isContentEditable();
        else
            return false;
    }
    
    return renderer()->style()->userModify() == READ_WRITE;
}

String HTMLElement::contentEditable() const 
{
    document()->updateRendering();

    if (!renderer())
        return "false";
    
    switch (renderer()->style()->userModify()) {
        case READ_WRITE:
            return "true";
        case READ_ONLY:
            return "false";
        case READ_WRITE_PLAINTEXT_ONLY:
            return "plaintext-only";
        default:
            return "inherit";
    }
}

void HTMLElement::setContentEditable(MappedAttribute* attr) 
{
    const AtomicString& enabled = attr->value();
    if (enabled.isEmpty() || equalIgnoringCase(enabled, "true")) {
        addCSSProperty(attr, CSS_PROP__WEBKIT_USER_MODIFY, CSS_VAL_READ_WRITE);
        addCSSProperty(attr, CSS_PROP_WORD_WRAP, CSS_VAL_BREAK_WORD);
        addCSSProperty(attr, CSS_PROP__WEBKIT_NBSP_MODE, CSS_VAL_SPACE);
        addCSSProperty(attr, CSS_PROP__WEBKIT_LINE_BREAK, CSS_VAL_AFTER_WHITE_SPACE);
    } else if (equalIgnoringCase(enabled, "false")) {
        addCSSProperty(attr, CSS_PROP__WEBKIT_USER_MODIFY, CSS_VAL_READ_ONLY);
        attr->decl()->removeProperty(CSS_PROP_WORD_WRAP, false);
        attr->decl()->removeProperty(CSS_PROP__WEBKIT_NBSP_MODE, false);
        attr->decl()->removeProperty(CSS_PROP__WEBKIT_LINE_BREAK, false);
    } else if (equalIgnoringCase(enabled, "inherit")) {
        addCSSProperty(attr, CSS_PROP__WEBKIT_USER_MODIFY, CSS_VAL_INHERIT);
        attr->decl()->removeProperty(CSS_PROP_WORD_WRAP, false);
        attr->decl()->removeProperty(CSS_PROP__WEBKIT_NBSP_MODE, false);
        attr->decl()->removeProperty(CSS_PROP__WEBKIT_LINE_BREAK, false);
    } else if (equalIgnoringCase(enabled, "plaintext-only")) {
        addCSSProperty(attr, CSS_PROP__WEBKIT_USER_MODIFY, CSS_VAL_READ_WRITE_PLAINTEXT_ONLY);
        addCSSProperty(attr, CSS_PROP_WORD_WRAP, CSS_VAL_BREAK_WORD);
        addCSSProperty(attr, CSS_PROP__WEBKIT_NBSP_MODE, CSS_VAL_SPACE);
        addCSSProperty(attr, CSS_PROP__WEBKIT_LINE_BREAK, CSS_VAL_AFTER_WHITE_SPACE);
    }
}

void HTMLElement::setContentEditable(const String &enabled)
{
    if (enabled == "inherit") {
        ExceptionCode ec;
        removeAttribute(contenteditableAttr, ec);
    }
    else
        setAttribute(contenteditableAttr, enabled.isEmpty() ? "true" : enabled);
}

void HTMLElement::click()
{
    dispatchSimulatedClick(0, false, false);
}

// accessKeyAction is used by the accessibility support code
// to send events to elements that our JavaScript caller does
// does not.  The elements JS is interested in have subclasses
// that override this method to direct the click appropriately.
// Here in the base class, then, we only send the click if
// the caller wants it to go to any HTMLElement, and we say
// to send the mouse events in addition to the click.
void HTMLElement::accessKeyAction(bool sendToAnyElement)
{
    if (sendToAnyElement)
        dispatchSimulatedClick(0, true);
}

String HTMLElement::toString() const
{
    if (!hasChildNodes() && document()->isHTMLDocument()) {
        String result = openTagStartToString();
        result += ">";

        if (endTagRequirement() == TagStatusRequired) {
            result += "</";
            result += nodeName();
            result += ">";
        }

        return result;
    }

    return Element::toString();
}

String HTMLElement::id() const
{
    return getAttribute(idAttr);
}

void HTMLElement::setId(const String& value)
{
    setAttribute(idAttr, value);
}

String HTMLElement::title() const
{
    return getAttribute(titleAttr);
}

void HTMLElement::setTitle(const String& value)
{
    setAttribute(titleAttr, value);
}

String HTMLElement::lang() const
{
    return getAttribute(langAttr);
}

void HTMLElement::setLang(const String& value)
{
    setAttribute(langAttr, value);
}

String HTMLElement::dir() const
{
    return getAttribute(dirAttr);
}

void HTMLElement::setDir(const String &value)
{
    setAttribute(dirAttr, value);
}

String HTMLElement::className() const
{
    return getAttribute(classAttr);
}

void HTMLElement::setClassName(const String &value)
{
    setAttribute(classAttr, value);
}

PassRefPtr<HTMLCollection> HTMLElement::children()
{
    return new HTMLCollection(this, HTMLCollection::NodeChildren);
}

// DOM Section 1.1.1
bool HTMLElement::childAllowed(Node *newChild)
{
    if (!Element::childAllowed(newChild))
        return false;

    // For XML documents, we are non-validating and do not check against a DTD, even for HTML elements.
    if (!document()->isHTMLDocument())
        return true;

    // Future-proof for XML content inside HTML documents (we may allow this some day).
    if (newChild->isElementNode() && !newChild->isHTMLElement())
        return true;

    // Elements with forbidden tag status can never have children
    if (endTagRequirement() == TagStatusForbidden)
        return false;

    // Comment nodes are always allowed.
    if (newChild->isCommentNode())
        return true;

    // Now call checkDTD.
    return checkDTD(newChild);
}

// DTD Stuff
// This unfortunate function is only needed when checking against the DTD.  Other languages (like SVG) won't need this.
bool HTMLElement::isRecognizedTagName(const QualifiedName& tagName)
{
    static HashSet<AtomicStringImpl*> tagList;
    if (tagList.isEmpty()) {
        size_t tagCount = 0;
        WebCore::QualifiedName** tags = HTMLNames::getHTMLTags(&tagCount);
        for (size_t i = 0; i < tagCount; i++)
            tagList.add(tags[i]->localName().impl());
    }
    return tagList.contains(tagName.localName().impl());
}

// The terms inline and block are used here loosely.  Don't make the mistake of assuming all inlines or all blocks
// need to be in these two lists.
HashSet<AtomicStringImpl*>* inlineTagList()
{
    static HashSet<AtomicStringImpl*> tagList;
    if (tagList.isEmpty()) {
        tagList.add(ttTag.localName().impl());
        tagList.add(iTag.localName().impl());
        tagList.add(bTag.localName().impl());
        tagList.add(uTag.localName().impl());
        tagList.add(sTag.localName().impl());
        tagList.add(strikeTag.localName().impl());
        tagList.add(bigTag.localName().impl());
        tagList.add(smallTag.localName().impl());
        tagList.add(emTag.localName().impl());
        tagList.add(strongTag.localName().impl());
        tagList.add(dfnTag.localName().impl());
        tagList.add(codeTag.localName().impl());
        tagList.add(sampTag.localName().impl());
        tagList.add(kbdTag.localName().impl());
        tagList.add(varTag.localName().impl());
        tagList.add(citeTag.localName().impl());
        tagList.add(abbrTag.localName().impl());
        tagList.add(acronymTag.localName().impl());
        tagList.add(aTag.localName().impl());
        tagList.add(canvasTag.localName().impl());
        tagList.add(imgTag.localName().impl());
        tagList.add(appletTag.localName().impl());
        tagList.add(objectTag.localName().impl());
        tagList.add(embedTag.localName().impl());
        tagList.add(fontTag.localName().impl());
        tagList.add(basefontTag.localName().impl());
        tagList.add(brTag.localName().impl());
        tagList.add(scriptTag.localName().impl());
        tagList.add(mapTag.localName().impl());
        tagList.add(qTag.localName().impl());
        tagList.add(subTag.localName().impl());
        tagList.add(supTag.localName().impl());
        tagList.add(spanTag.localName().impl());
        tagList.add(bdoTag.localName().impl());
        tagList.add(iframeTag.localName().impl());
        tagList.add(inputTag.localName().impl());
        tagList.add(keygenTag.localName().impl());
        tagList.add(selectTag.localName().impl());
        tagList.add(textareaTag.localName().impl());
        tagList.add(labelTag.localName().impl());
        tagList.add(buttonTag.localName().impl());
        tagList.add(insTag.localName().impl());
        tagList.add(delTag.localName().impl());
        tagList.add(nobrTag.localName().impl());
        tagList.add(wbrTag.localName().impl());
        tagList.add(audioTag.localName().impl());
        tagList.add(videoTag.localName().impl());
    }
    return &tagList;
}

HashSet<AtomicStringImpl*>* blockTagList()
{
    static HashSet<AtomicStringImpl*> tagList;
    if (tagList.isEmpty()) {
        tagList.add(addressTag.localName().impl());
        tagList.add(blockquoteTag.localName().impl());
        tagList.add(centerTag.localName().impl());
        tagList.add(ddTag.localName().impl());
        tagList.add(dirTag.localName().impl());
        tagList.add(divTag.localName().impl());
        tagList.add(dlTag.localName().impl());
        tagList.add(dtTag.localName().impl());
        tagList.add(fieldsetTag.localName().impl());
        tagList.add(formTag.localName().impl());
        tagList.add(h1Tag.localName().impl());
        tagList.add(h2Tag.localName().impl());
        tagList.add(h3Tag.localName().impl());
        tagList.add(h4Tag.localName().impl());
        tagList.add(h5Tag.localName().impl());
        tagList.add(h6Tag.localName().impl());
        tagList.add(hrTag.localName().impl());
        tagList.add(isindexTag.localName().impl());
        tagList.add(layerTag.localName().impl());
        tagList.add(liTag.localName().impl());
        tagList.add(listingTag.localName().impl());
        tagList.add(marqueeTag.localName().impl());
        tagList.add(menuTag.localName().impl());
        tagList.add(noembedTag.localName().impl());
        tagList.add(noframesTag.localName().impl());
        tagList.add(nolayerTag.localName().impl());
        tagList.add(noscriptTag.localName().impl());
        tagList.add(olTag.localName().impl());
        tagList.add(pTag.localName().impl());
        tagList.add(plaintextTag.localName().impl());
        tagList.add(preTag.localName().impl());
        tagList.add(tableTag.localName().impl());
        tagList.add(ulTag.localName().impl());
        tagList.add(xmpTag.localName().impl());
    }
    return &tagList;
}

bool HTMLElement::inEitherTagList(const Node* newChild)
{
    if (newChild->isTextNode())
        return true;
        
    if (newChild->isHTMLElement()) {
        const HTMLElement* child = static_cast<const HTMLElement*>(newChild);
        if (inlineTagList()->contains(child->tagQName().localName().impl()))
            return true;
        if (blockTagList()->contains(child->tagQName().localName().impl()))
            return true;
        return !isRecognizedTagName(child->tagQName()); // Accept custom html tags
    }

    return false;
}

bool HTMLElement::inInlineTagList(const Node* newChild)
{
    if (newChild->isTextNode())
        return true;

    if (newChild->isHTMLElement()) {
        const HTMLElement* child = static_cast<const HTMLElement*>(newChild);
        if (inlineTagList()->contains(child->tagQName().localName().impl()))
            return true;
        return !isRecognizedTagName(child->tagQName()); // Accept custom html tags
    }

    return false;
}

bool HTMLElement::inBlockTagList(const Node* newChild)
{
    if (newChild->isTextNode())
        return true;
            
    if (newChild->isHTMLElement()) {
        const HTMLElement* child = static_cast<const HTMLElement*>(newChild);
        return (blockTagList()->contains(child->tagQName().localName().impl()));
    }

    return false;
}

bool HTMLElement::checkDTD(const Node* newChild)
{
    if (hasLocalName(addressTag) && newChild->hasTagName(pTag))
        return true;
    return inEitherTagList(newChild);
}

void HTMLElement::setHTMLEventListener(const AtomicString& eventType, Attribute* attr)
{
    Element::setHTMLEventListener(eventType,
        document()->createHTMLEventListener(attr->localName().domString(), attr->value(), this));
}
    
bool HTMLElement::rendererIsNeeded(RenderStyle *style)
{
    if (hasLocalName(noscriptTag)) {
        Settings* settings = document()->settings();
        if (settings && settings->isJavaScriptEnabled())
            return false;
    }
    return (document()->documentElement() == this) || (style->display() != NONE);
}
    
RenderObject* HTMLElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    if (hasLocalName(wbrTag))
        return new (arena) RenderWordBreak(this);
    return RenderObject::createObject(this, style);
}

HTMLFormElement* HTMLElement::findFormAncestor() const
{
    for (Node* ancestor = parentNode(); ancestor; ancestor = ancestor->parentNode())
        if (ancestor->hasTagName(formTag))
            return static_cast<HTMLFormElement*>(ancestor);
    return 0;
}

HTMLFormElement* HTMLElement::virtualForm() const
{
    return findFormAncestor();
}

} // namespace WebCore

#ifndef NDEBUG
void dumpInnerHTML(WebCore::HTMLElement* element)
{
    printf("%s\n", element->innerHTML().ascii().data());
}
#endif
