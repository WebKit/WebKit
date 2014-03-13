/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2011 Motorola Mobility. All rights reserved.
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

#include "Attribute.h"
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "DOMSettableTokenList.h"
#include "DocumentFragment.h"
#include "ElementAncestorIterator.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLBRElement.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "HTMLElementFactory.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTemplateElement.h"
#include "HTMLTextFormControlElement.h"
#include "NodeTraversal.h"
#include "RenderLineBreak.h"
#include "ScriptController.h"
#include "Settings.h"
#include "StyleProperties.h"
#include "SubframeLoader.h"
#include "Text.h"
#include "XMLNames.h"
#include "markup.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

namespace WebCore {

using namespace HTMLNames;
using namespace WTF;

PassRefPtr<HTMLElement> HTMLElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new HTMLElement(tagName, document));
}

String HTMLElement::nodeName() const
{
    // FIXME: Would be nice to have an atomicstring lookup based off uppercase
    // chars that does not have to copy the string on a hit in the hash.
    // FIXME: We should have a way to detect XHTML elements and replace the hasPrefix() check with it.
    if (document().isHTMLDocument() && !tagQName().hasPrefix())
        return tagQName().localNameUpper();
    return Element::nodeName();
}

bool HTMLElement::ieForbidsInsertHTML() const
{
    // FIXME: Supposedly IE disallows settting innerHTML, outerHTML
    // and createContextualFragment on these tags.  We have no tests to
    // verify this however, so this list could be totally wrong.
    // This list was moved from the previous endTagRequirement() implementation.
    // This is also called from editing and assumed to be the list of tags
    // for which no end tag should be serialized. It's unclear if the list for
    // IE compat and the list for serialization sanity are the same.
    if (hasTagName(areaTag)
        || hasTagName(baseTag)
        || hasTagName(basefontTag)
        || hasTagName(brTag)
        || hasTagName(colTag)
        || hasTagName(embedTag)
        || hasTagName(frameTag)
        || hasTagName(hrTag)
        || hasTagName(imageTag)
        || hasTagName(imgTag)
        || hasTagName(inputTag)
        || hasTagName(isindexTag)
        || hasTagName(linkTag)
        || hasTagName(metaTag)
        || hasTagName(paramTag)
        || hasTagName(sourceTag)
        || hasTagName(wbrTag))
        return true;
    // FIXME: I'm not sure why dashboard mode would want to change the
    // serialization of <canvas>, that seems like a bad idea.
#if ENABLE(DASHBOARD_SUPPORT)
    if (hasTagName(canvasTag)) {
        Settings* settings = document().settings();
        if (settings && settings->usesDashboardBackwardCompatibilityMode())
            return true;
    }
#endif
    return false;
}

static inline CSSValueID unicodeBidiAttributeForDirAuto(HTMLElement& element)
{
    if (element.hasTagName(preTag) || element.hasTagName(textareaTag))
        return CSSValueWebkitPlaintext;
    // FIXME: For bdo element, dir="auto" should result in "bidi-override isolate" but we don't support having multiple values in unicode-bidi yet.
    // See https://bugs.webkit.org/show_bug.cgi?id=73164.
    return CSSValueWebkitIsolate;
}

unsigned HTMLElement::parseBorderWidthAttribute(const AtomicString& value) const
{
    unsigned borderWidth = 0;
    if (value.isEmpty() || !parseHTMLNonNegativeInteger(value, borderWidth))
        return hasTagName(tableTag) ? 1 : borderWidth;
    return borderWidth;
}

void HTMLElement::applyBorderAttributeToStyle(const AtomicString& value, MutableStyleProperties& style)
{
    addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderWidth, parseBorderWidthAttribute(value), CSSPrimitiveValue::CSS_PX);
    addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderStyle, CSSValueSolid);
}

void HTMLElement::mapLanguageAttributeToLocale(const AtomicString& value, MutableStyleProperties& style)
{
    if (!value.isEmpty()) {
        // Have to quote so the locale id is treated as a string instead of as a CSS keyword.
        addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitLocale, quoteCSSString(value));
    } else {
        // The empty string means the language is explicitly unknown.
        addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitLocale, CSSValueAuto);
    }
}

bool HTMLElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == alignAttr || name == contenteditableAttr || name == hiddenAttr || name == langAttr || name.matches(XMLNames::langAttr) || name == draggableAttr || name == dirAttr)
        return true;
    return StyledElement::isPresentationAttribute(name);
}

static bool isLTROrRTLIgnoringCase(const AtomicString& dirAttributeValue)
{
    return equalIgnoringCase(dirAttributeValue, "rtl") || equalIgnoringCase(dirAttributeValue, "ltr");
}

void HTMLElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStyleProperties& style)
{
    if (name == alignAttr) {
        if (equalIgnoringCase(value, "middle"))
            addPropertyToPresentationAttributeStyle(style, CSSPropertyTextAlign, CSSValueCenter);
        else
            addPropertyToPresentationAttributeStyle(style, CSSPropertyTextAlign, value);
    } else if (name == contenteditableAttr) {
        if (value.isEmpty() || equalIgnoringCase(value, "true")) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserModify, CSSValueReadWrite);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWordWrap, CSSValueBreakWord);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitNbspMode, CSSValueSpace);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitLineBreak, CSSValueAfterWhiteSpace);
#if PLATFORM(IOS)
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitTextSizeAdjust, CSSValueNone);
#endif
        } else if (equalIgnoringCase(value, "plaintext-only")) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserModify, CSSValueReadWritePlaintextOnly);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWordWrap, CSSValueBreakWord);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitNbspMode, CSSValueSpace);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitLineBreak, CSSValueAfterWhiteSpace);
#if PLATFORM(IOS)
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitTextSizeAdjust, CSSValueNone);
#endif
        } else if (equalIgnoringCase(value, "false"))
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserModify, CSSValueReadOnly);
    } else if (name == hiddenAttr) {
        addPropertyToPresentationAttributeStyle(style, CSSPropertyDisplay, CSSValueNone);
    } else if (name == draggableAttr) {
        if (equalIgnoringCase(value, "true")) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserDrag, CSSValueElement);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserSelect, CSSValueNone);
        } else if (equalIgnoringCase(value, "false"))
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserDrag, CSSValueNone);
    } else if (name == dirAttr) {
        if (equalIgnoringCase(value, "auto"))
            addPropertyToPresentationAttributeStyle(style, CSSPropertyUnicodeBidi, unicodeBidiAttributeForDirAuto(*this));
        else {
            if (isLTROrRTLIgnoringCase(value))
                addPropertyToPresentationAttributeStyle(style, CSSPropertyDirection, value);
            if (!hasTagName(bdiTag) && !hasTagName(bdoTag) && !hasTagName(outputTag))
                addPropertyToPresentationAttributeStyle(style, CSSPropertyUnicodeBidi, CSSValueEmbed);
        }
    } else if (name.matches(XMLNames::langAttr))
        mapLanguageAttributeToLocale(value, style);
    else if (name == langAttr) {
        // xml:lang has a higher priority than lang.
        if (!fastHasAttribute(XMLNames::langAttr))
            mapLanguageAttributeToLocale(value, style);
    } else
        StyledElement::collectStyleForPresentationAttribute(name, value, style);
}

static NEVER_INLINE void populateEventNameForAttributeLocalNameMap(HashMap<AtomicStringImpl*, AtomicString>& map)
{
    static const QualifiedName* const simpleTable[] = {
        &onabortAttr,
        &onbeforecopyAttr,
        &onbeforecutAttr,
        &onbeforepasteAttr,
        &onblurAttr,
        &oncanplayAttr,
        &oncanplaythroughAttr,
        &onchangeAttr,
        &onclickAttr,
        &oncontextmenuAttr,
        &oncopyAttr,
        &oncutAttr,
        &ondblclickAttr,
        &ondragAttr,
        &ondragendAttr,
        &ondragenterAttr,
        &ondragleaveAttr,
        &ondragoverAttr,
        &ondragstartAttr,
        &ondropAttr,
        &ondurationchangeAttr,
        &onemptiedAttr,
        &onendedAttr,
        &onerrorAttr,
        &onfocusAttr,
        &onfocusinAttr,
        &onfocusoutAttr,
        &oninputAttr,
        &oninvalidAttr,
        &onkeydownAttr,
        &onkeypressAttr,
        &onkeyupAttr,
        &onloadAttr,
        &onloadeddataAttr,
        &onloadedmetadataAttr,
        &onloadstartAttr,
        &onmousedownAttr,
        &onmouseenterAttr,
        &onmouseleaveAttr,
        &onmousemoveAttr,
        &onmouseoutAttr,
        &onmouseoverAttr,
        &onmouseupAttr,
        &onmousewheelAttr,
        &onpasteAttr,
        &onpauseAttr,
        &onplayAttr,
        &onplayingAttr,
        &onprogressAttr,
        &onratechangeAttr,
        &onresetAttr,
        &onscrollAttr,
        &onseekedAttr,
        &onseekingAttr,
        &onselectAttr,
        &onselectstartAttr,
        &onstalledAttr,
        &onsubmitAttr,
        &onsuspendAttr,
        &ontimeupdateAttr,
        &ontouchcancelAttr,
        &ontouchendAttr,
        &ontouchmoveAttr,
        &ontouchstartAttr,
        &onvolumechangeAttr,
        &onwaitingAttr,
#if ENABLE(WILL_REVEAL_EDGE_EVENTS)
        &onwebkitwillrevealbottomAttr,
        &onwebkitwillrevealleftAttr,
        &onwebkitwillrevealrightAttr,
        &onwebkitwillrevealtopAttr,
#endif
        &onwheelAttr,
#if ENABLE(IOS_GESTURE_EVENTS)
        &ongesturechangeAttr,
        &ongestureendAttr,
        &ongesturestartAttr,
#endif
#if ENABLE(FULLSCREEN_API)
        &onwebkitfullscreenchangeAttr,
        &onwebkitfullscreenerrorAttr,
#endif
    };

    for (unsigned i = 0, size = WTF_ARRAY_LENGTH(simpleTable); i < size; ++i) {
        // FIXME: Would be nice to check these against the actual event names in eventNames().
        // Not obvious how to do that simply, though.
        const AtomicString& attributeName = simpleTable[i]->localName();

        // Remove the "on" prefix. Requires some memory allocation and computing a hash, but
        // by not using pointers from eventNames(), simpleTable can be initialized at compile time.
        AtomicString eventName = attributeName.string().substring(2);

        map.add(attributeName.impl(), eventName);
    }

    struct CustomMapping {
        const QualifiedName& attributeName;
        const AtomicString& eventName;
    };

    const CustomMapping customTable[] = {
        { ontransitionendAttr, eventNames().webkitTransitionEndEvent },
        { onwebkitanimationendAttr, eventNames().webkitAnimationEndEvent },
        { onwebkitanimationiterationAttr, eventNames().webkitAnimationIterationEvent },
        { onwebkitanimationstartAttr, eventNames().webkitAnimationStartEvent },
        { onwebkittransitionendAttr, eventNames().webkitTransitionEndEvent },
    };

    for (unsigned i = 0, size = WTF_ARRAY_LENGTH(customTable); i < size; ++i)
        map.add(customTable[i].attributeName.localName().impl(), customTable[i].eventName);
}

void HTMLElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == HTMLNames::idAttr || name == HTMLNames::classAttr || name == HTMLNames::styleAttr)
        return StyledElement::parseAttribute(name, value);

    if (name == dirAttr)
        dirAttributeChanged(value);
    else if (name == tabindexAttr) {
        int tabindex = 0;
        if (value.isEmpty())
            clearTabIndexExplicitlyIfNeeded();
        else if (parseHTMLInteger(value, tabindex)) {
            // Clamp tabindex to the range of 'short' to match Firefox's behavior.
            setTabIndexExplicitly(std::max(static_cast<int>(std::numeric_limits<short>::min()), std::min(tabindex, static_cast<int>(std::numeric_limits<short>::max()))));
        }
    } else if (name.namespaceURI().isNull()) {
        // FIXME: Can we do this even faster by checking the local name "on" prefix before we do anything with the map?
        static NeverDestroyed<HashMap<AtomicStringImpl*, AtomicString>> eventNamesGlobal;
        auto& eventNames = eventNamesGlobal.get();
        if (eventNames.isEmpty())
            populateEventNameForAttributeLocalNameMap(eventNames);
        const AtomicString& eventName = eventNames.get(name.localName().impl());
        if (!eventName.isNull())
            setAttributeEventListener(eventName, name, value);
    }
}

String HTMLElement::innerHTML() const
{
    return createMarkup(*this, ChildrenOnly);
}

String HTMLElement::outerHTML() const
{
    return createMarkup(*this);
}

void HTMLElement::setInnerHTML(const String& html, ExceptionCode& ec)
{
    if (RefPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(html, this, AllowScriptingContent, ec)) {
        ContainerNode* container = this;
#if ENABLE(TEMPLATE_ELEMENT)
        if (hasTagName(templateTag))
            container = toHTMLTemplateElement(this)->content();
#endif
        replaceChildrenWithFragment(*container, fragment.release(), ec);
    }
}

static void mergeWithNextTextNode(Text& node, ExceptionCode& ec)
{
    Node* next = node.nextSibling();
    if (!next || !next->isTextNode())
        return;

    Ref<Text> textNode(node);
    Ref<Text> textNext(toText(*next));
    textNode->appendData(textNext->data(), ec);
    if (ec)
        return;
    textNext->remove(ec);
}

void HTMLElement::setOuterHTML(const String& html, ExceptionCode& ec)
{
    Element* p = parentElement();
    if (!p || !p->isHTMLElement()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
    RefPtr<HTMLElement> parent = toHTMLElement(p);
    RefPtr<Node> prev = previousSibling();
    RefPtr<Node> next = nextSibling();

    RefPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(html, parent.get(), AllowScriptingContent, ec);
    if (ec)
        return;
      
    parent->replaceChild(fragment.release(), this, ec);
    RefPtr<Node> node = next ? next->previousSibling() : nullptr;
    if (!ec && node && node->isTextNode())
        mergeWithNextTextNode(toText(*node), ec);
    if (!ec && prev && prev->isTextNode())
        mergeWithNextTextNode(toText(*prev), ec);
}

PassRefPtr<DocumentFragment> HTMLElement::textToFragment(const String& text, ExceptionCode& ec)
{
    RefPtr<DocumentFragment> fragment = DocumentFragment::create(document());
    unsigned int i, length = text.length();
    UChar c = 0;
    for (unsigned int start = 0; start < length; ) {

        // Find next line break.
        for (i = start; i < length; i++) {
            c = text[i];
            if (c == '\r' || c == '\n')
                break;
        }

        fragment->appendChild(Text::create(document(), text.substring(start, i - start)), ec);
        if (ec)
            return nullptr;

        if (c == '\r' || c == '\n') {
            fragment->appendChild(HTMLBRElement::create(document()), ec);
            if (ec)
                return nullptr;
            // Make sure \r\n doesn't result in two line breaks.
            if (c == '\r' && i + 1 < length && text[i + 1] == '\n')
                i++;
        }

        start = i + 1; // Character after line break.
    }

    return fragment;
}

static inline bool shouldProhibitSetInnerOuterText(const HTMLElement& element)
{
    return element.hasTagName(colTag)
        || element.hasTagName(colgroupTag)
        || element.hasTagName(framesetTag)
        || element.hasTagName(headTag)
        || element.hasTagName(htmlTag)
        || element.hasTagName(tableTag)
        || element.hasTagName(tbodyTag)
        || element.hasTagName(tfootTag)
        || element.hasTagName(theadTag)
        || element.hasTagName(trTag);
}

void HTMLElement::setInnerText(const String& text, ExceptionCode& ec)
{
    if (ieForbidsInsertHTML()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
    if (shouldProhibitSetInnerOuterText(*this)) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // FIXME: This doesn't take whitespace collapsing into account at all.

    if (!text.contains('\n') && !text.contains('\r')) {
        if (text.isEmpty()) {
            removeChildren();
            return;
        }
        replaceChildrenWithText(*this, text, ec);
        return;
    }

    // FIXME: Do we need to be able to detect preserveNewline style even when there's no renderer?
    // FIXME: Can the renderer be out of date here? Do we need to call updateStyleIfNeeded?
    // For example, for the contents of textarea elements that are display:none?
    auto r = renderer();
    if ((r && r->style().preserveNewline()) || (inDocument() && isTextControlInnerTextElement())) {
        if (!text.contains('\r')) {
            replaceChildrenWithText(*this, text, ec);
            return;
        }
        String textWithConsistentLineBreaks = text;
        textWithConsistentLineBreaks.replace("\r\n", "\n");
        textWithConsistentLineBreaks.replace('\r', '\n');
        replaceChildrenWithText(*this, textWithConsistentLineBreaks, ec);
        return;
    }

    // Add text nodes and <br> elements.
    ec = 0;
    RefPtr<DocumentFragment> fragment = textToFragment(text, ec);
    if (!ec)
        replaceChildrenWithFragment(*this, fragment.release(), ec);
}

void HTMLElement::setOuterText(const String& text, ExceptionCode& ec)
{
    if (ieForbidsInsertHTML()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
    if (shouldProhibitSetInnerOuterText(*this)) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    RefPtr<ContainerNode> parent = parentNode();
    if (!parent) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    RefPtr<Node> prev = previousSibling();
    RefPtr<Node> next = nextSibling();
    RefPtr<Node> newChild;
    ec = 0;
    
    // Convert text to fragment with <br> tags instead of linebreaks if needed.
    if (text.contains('\r') || text.contains('\n'))
        newChild = textToFragment(text, ec);
    else
        newChild = Text::create(document(), text);

    if (!this || !parentNode())
        ec = HIERARCHY_REQUEST_ERR;
    if (ec)
        return;
    parent->replaceChild(newChild.release(), this, ec);

    RefPtr<Node> node = next ? next->previousSibling() : nullptr;
    if (!ec && node && node->isTextNode())
        mergeWithNextTextNode(toText(*node), ec);
    if (!ec && prev && prev->isTextNode())
        mergeWithNextTextNode(toText(*prev), ec);
}

Node* HTMLElement::insertAdjacent(const String& where, Node* newChild, ExceptionCode& ec)
{
    // In Internet Explorer if the element has no parent and where is "beforeBegin" or "afterEnd",
    // a document fragment is created and the elements appended in the correct order. This document
    // fragment isn't returned anywhere.
    //
    // This is impossible for us to implement as the DOM tree does not allow for such structures,
    // Opera also appears to disallow such usage.

    if (equalIgnoringCase(where, "beforeBegin")) {
        ContainerNode* parent = this->parentNode();
        return (parent && parent->insertBefore(newChild, this, ec)) ? newChild : nullptr;
    }

    if (equalIgnoringCase(where, "afterBegin"))
        return insertBefore(newChild, firstChild(), ec) ? newChild : nullptr;

    if (equalIgnoringCase(where, "beforeEnd"))
        return appendChild(newChild, ec) ? newChild : nullptr;

    if (equalIgnoringCase(where, "afterEnd")) {
        ContainerNode* parent = this->parentNode();
        return (parent && parent->insertBefore(newChild, nextSibling(), ec)) ? newChild : nullptr;
    }
    
    // IE throws COM Exception E_INVALIDARG; this is the best DOM exception alternative.
    ec = NOT_SUPPORTED_ERR;
    return nullptr;
}

Element* HTMLElement::insertAdjacentElement(const String& where, Element* newChild, ExceptionCode& ec)
{
    if (!newChild) {
        // IE throws COM Exception E_INVALIDARG; this is the best DOM exception alternative.
        ec = TYPE_MISMATCH_ERR;
        return nullptr;
    }

    Node* returnValue = insertAdjacent(where, newChild, ec);
    ASSERT_WITH_SECURITY_IMPLICATION(!returnValue || returnValue->isElementNode());
    return toElement(returnValue); 
}

// Step 3 of http://www.whatwg.org/specs/web-apps/current-work/multipage/apis-in-html-documents.html#insertadjacenthtml()
static Element* contextElementForInsertion(const String& where, Element* element, ExceptionCode& ec)
{
    if (equalIgnoringCase(where, "beforeBegin") || equalIgnoringCase(where, "afterEnd")) {
        ContainerNode* parent = element->parentNode();
        if (parent && !parent->isElementNode()) {
            ec = NO_MODIFICATION_ALLOWED_ERR;
            return nullptr;
        }
        ASSERT_WITH_SECURITY_IMPLICATION(!parent || parent->isElementNode());
        return toElement(parent);
    }
    if (equalIgnoringCase(where, "afterBegin") || equalIgnoringCase(where, "beforeEnd"))
        return element;
    ec =  SYNTAX_ERR;
    return nullptr;
}

void HTMLElement::insertAdjacentHTML(const String& where, const String& markup, ExceptionCode& ec)
{
    Element* contextElement = contextElementForInsertion(where, this, ec);
    if (!contextElement)
        return;
    RefPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(markup, contextElement, AllowScriptingContent, ec);
    if (!fragment)
        return;
    insertAdjacent(where, fragment.get(), ec);
}

void HTMLElement::insertAdjacentText(const String& where, const String& text, ExceptionCode& ec)
{
    RefPtr<Text> textNode = document().createTextNode(text);
    insertAdjacent(where, textNode.get(), ec);
}

void HTMLElement::applyAlignmentAttributeToStyle(const AtomicString& alignment, MutableStyleProperties& style)
{
    // Vertical alignment with respect to the current baseline of the text
    // right or left means floating images.
    CSSValueID floatValue = CSSValueInvalid;
    CSSValueID verticalAlignValue = CSSValueInvalid;

    if (equalIgnoringCase(alignment, "absmiddle"))
        verticalAlignValue = CSSValueMiddle;
    else if (equalIgnoringCase(alignment, "absbottom"))
        verticalAlignValue = CSSValueBottom;
    else if (equalIgnoringCase(alignment, "left")) {
        floatValue = CSSValueLeft;
        verticalAlignValue = CSSValueTop;
    } else if (equalIgnoringCase(alignment, "right")) {
        floatValue = CSSValueRight;
        verticalAlignValue = CSSValueTop;
    } else if (equalIgnoringCase(alignment, "top"))
        verticalAlignValue = CSSValueTop;
    else if (equalIgnoringCase(alignment, "middle"))
        verticalAlignValue = CSSValueWebkitBaselineMiddle;
    else if (equalIgnoringCase(alignment, "center"))
        verticalAlignValue = CSSValueMiddle;
    else if (equalIgnoringCase(alignment, "bottom"))
        verticalAlignValue = CSSValueBaseline;
    else if (equalIgnoringCase(alignment, "texttop"))
        verticalAlignValue = CSSValueTextTop;

    if (floatValue != CSSValueInvalid)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFloat, floatValue);

    if (verticalAlignValue != CSSValueInvalid)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyVerticalAlign, verticalAlignValue);
}

bool HTMLElement::hasCustomFocusLogic() const
{
    return false;
}

bool HTMLElement::supportsFocus() const
{
    return Element::supportsFocus() || (hasEditableStyle() && parentNode() && !parentNode()->hasEditableStyle());
}

String HTMLElement::contentEditable() const
{
    const AtomicString& value = fastGetAttribute(contenteditableAttr);

    if (value.isNull())
        return ASCIILiteral("inherit");
    if (value.isEmpty() || equalIgnoringCase(value, "true"))
        return ASCIILiteral("true");
    if (equalIgnoringCase(value, "false"))
        return ASCIILiteral("false");
    if (equalIgnoringCase(value, "plaintext-only"))
        return ASCIILiteral("plaintext-only");

    return ASCIILiteral("inherit");
}

void HTMLElement::setContentEditable(const String& enabled, ExceptionCode& ec)
{
    if (equalIgnoringCase(enabled, "true"))
        setAttribute(contenteditableAttr, AtomicString("true", AtomicString::ConstructFromLiteral));
    else if (equalIgnoringCase(enabled, "false"))
        setAttribute(contenteditableAttr, AtomicString("false", AtomicString::ConstructFromLiteral));
    else if (equalIgnoringCase(enabled, "plaintext-only"))
        setAttribute(contenteditableAttr, AtomicString("plaintext-only", AtomicString::ConstructFromLiteral));
    else if (equalIgnoringCase(enabled, "inherit"))
        removeAttribute(contenteditableAttr);
    else
        ec = SYNTAX_ERR;
}

bool HTMLElement::draggable() const
{
    return equalIgnoringCase(fastGetAttribute(draggableAttr), "true");
}

void HTMLElement::setDraggable(bool value)
{
    setAttribute(draggableAttr, value
        ? AtomicString("true", AtomicString::ConstructFromLiteral)
        : AtomicString("false", AtomicString::ConstructFromLiteral));
}

bool HTMLElement::spellcheck() const
{
    return isSpellCheckingEnabled();
}

void HTMLElement::setSpellcheck(bool enable)
{
    setAttribute(spellcheckAttr, enable
        ? AtomicString("true", AtomicString::ConstructFromLiteral)
        : AtomicString("false", AtomicString::ConstructFromLiteral));
}

void HTMLElement::click()
{
    dispatchSimulatedClick(nullptr, SendNoEvents, DoNotShowPressedLook);
}

void HTMLElement::accessKeyAction(bool sendMouseEvents)
{
    dispatchSimulatedClick(nullptr, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
}

String HTMLElement::title() const
{
    return fastGetAttribute(titleAttr);
}

short HTMLElement::tabIndex() const
{
    if (supportsFocus())
        return Element::tabIndex();
    return -1;
}

void HTMLElement::setTabIndex(int value)
{
    setIntegralAttribute(tabindexAttr, value);
}

TranslateAttributeMode HTMLElement::translateAttributeMode() const
{
    const AtomicString& value = fastGetAttribute(translateAttr);

    if (value.isNull())
        return TranslateAttributeInherit;
    if (equalIgnoringCase(value, "yes") || value.isEmpty())
        return TranslateAttributeYes;
    if (equalIgnoringCase(value, "no"))
        return TranslateAttributeNo;

    return TranslateAttributeInherit;
}

bool HTMLElement::translate() const
{
    for (auto& element : lineageOfType<HTMLElement>(*this)) {
        TranslateAttributeMode mode = element.translateAttributeMode();
        if (mode == TranslateAttributeInherit)
            continue;
        ASSERT(mode == TranslateAttributeYes || mode == TranslateAttributeNo);
        return mode == TranslateAttributeYes;
    }

    // Default on the root element is translate=yes.
    return true;
}

void HTMLElement::setTranslate(bool enable)
{
    setAttribute(translateAttr, enable ? "yes" : "no");
}

PassRefPtr<HTMLCollection> HTMLElement::children()
{
    return ensureCachedHTMLCollection(NodeChildren);
}

bool HTMLElement::rendererIsNeeded(const RenderStyle& style)
{
    if (hasTagName(noscriptTag)) {
        Frame* frame = document().frame();
        if (frame && frame->script().canExecuteScripts(NotAboutToExecuteScript))
            return false;
    } else if (hasTagName(noembedTag)) {
        Frame* frame = document().frame();
        if (frame && frame->loader().subframeLoader().allowPlugins(NotAboutToInstantiatePlugin))
            return false;
    }
    return StyledElement::rendererIsNeeded(style);
}

RenderPtr<RenderElement> HTMLElement::createElementRenderer(PassRef<RenderStyle> style)
{
    if (hasTagName(wbrTag))
        return createRenderer<RenderLineBreak>(*this, std::move(style));
    return RenderElement::createFor(*this, std::move(style));
}

HTMLFormElement* HTMLElement::virtualForm() const
{
    return HTMLFormElement::findClosestFormAncestor(*this);
}

static inline bool elementAffectsDirectionality(const Node& node)
{
    return node.isHTMLElement() && (toHTMLElement(node).hasTagName(bdiTag) || toHTMLElement(node).fastHasAttribute(dirAttr));
}

static void setHasDirAutoFlagRecursively(Node* firstNode, bool flag, Node* lastNode = nullptr)
{
    firstNode->setSelfOrAncestorHasDirAutoAttribute(flag);

    Node* node = firstNode->firstChild();

    while (node) {
        if (node->selfOrAncestorHasDirAutoAttribute() == flag)
            return;

        if (elementAffectsDirectionality(*node)) {
            if (node == lastNode)
                return;
            node = NodeTraversal::nextSkippingChildren(node, firstNode);
            continue;
        }
        node->setSelfOrAncestorHasDirAutoAttribute(flag);
        if (node == lastNode)
            return;
        node = NodeTraversal::next(node, firstNode);
    }
}

void HTMLElement::childrenChanged(const ChildChange& change)
{
    StyledElement::childrenChanged(change);
    adjustDirectionalityIfNeededAfterChildrenChanged(change.previousSiblingElement, change.type);
}

bool HTMLElement::hasDirectionAuto() const
{
    const AtomicString& direction = fastGetAttribute(dirAttr);
    return (hasTagName(bdiTag) && direction.isNull()) || equalIgnoringCase(direction, "auto");
}

TextDirection HTMLElement::directionalityIfhasDirAutoAttribute(bool& isAuto) const
{
    if (!(selfOrAncestorHasDirAutoAttribute() && hasDirectionAuto())) {
        isAuto = false;
        return LTR;
    }

    isAuto = true;
    return directionality();
}

TextDirection HTMLElement::directionality(Node** strongDirectionalityTextNode) const
{
    if (isHTMLTextFormControlElement(*this)) {
        HTMLTextFormControlElement* textElement = toHTMLTextFormControlElement(const_cast<HTMLElement*>(this));
        bool hasStrongDirectionality;
        UCharDirection textDirection = textElement->value().defaultWritingDirection(&hasStrongDirectionality);
        if (strongDirectionalityTextNode)
            *strongDirectionalityTextNode = hasStrongDirectionality ? textElement : nullptr;
        return (textDirection == U_LEFT_TO_RIGHT) ? LTR : RTL;
    }

    Node* node = firstChild();
    while (node) {
        // Skip bdi, script, style and text form controls.
        if (equalIgnoringCase(node->nodeName(), "bdi") || node->hasTagName(scriptTag) || node->hasTagName(styleTag) 
            || (node->isElementNode() && toElement(node)->isTextFormControl())) {
            node = NodeTraversal::nextSkippingChildren(node, this);
            continue;
        }

        // Skip elements with valid dir attribute
        if (node->isElementNode()) {
            AtomicString dirAttributeValue = toElement(node)->fastGetAttribute(dirAttr);
            if (isLTROrRTLIgnoringCase(dirAttributeValue) || equalIgnoringCase(dirAttributeValue, "auto")) {
                node = NodeTraversal::nextSkippingChildren(node, this);
                continue;
            }
        }

        if (node->isTextNode()) {
            bool hasStrongDirectionality;
            UCharDirection textDirection = node->textContent(true).defaultWritingDirection(&hasStrongDirectionality);
            if (hasStrongDirectionality) {
                if (strongDirectionalityTextNode)
                    *strongDirectionalityTextNode = node;
                return (textDirection == U_LEFT_TO_RIGHT) ? LTR : RTL;
            }
        }
        node = NodeTraversal::next(node, this);
    }
    if (strongDirectionalityTextNode)
        *strongDirectionalityTextNode = nullptr;
    return LTR;
}

void HTMLElement::dirAttributeChanged(const AtomicString& value)
{
    Element* parent = parentElement();

    if (parent && parent->isHTMLElement() && parent->selfOrAncestorHasDirAutoAttribute())
        toHTMLElement(parent)->adjustDirectionalityIfNeededAfterChildAttributeChanged(this);

    if (equalIgnoringCase(value, "auto"))
        calculateAndAdjustDirectionality();
}

void HTMLElement::adjustDirectionalityIfNeededAfterChildAttributeChanged(Element* child)
{
    ASSERT(selfOrAncestorHasDirAutoAttribute());
    Node* strongDirectionalityTextNode;
    TextDirection textDirection = directionality(&strongDirectionalityTextNode);
    setHasDirAutoFlagRecursively(child, false);
    if (!renderer() || renderer()->style().direction() == textDirection)
        return;
    for (auto& elementToAdjust : elementLineage(this)) {
        if (elementAffectsDirectionality(elementToAdjust)) {
            elementToAdjust.setNeedsStyleRecalc();
            return;
        }
    }
}

void HTMLElement::calculateAndAdjustDirectionality()
{
    Node* strongDirectionalityTextNode;
    TextDirection textDirection = directionality(&strongDirectionalityTextNode);
    setHasDirAutoFlagRecursively(this, true, strongDirectionalityTextNode);
    if (renderer() && renderer()->style().direction() != textDirection)
        setNeedsStyleRecalc();
}

void HTMLElement::adjustDirectionalityIfNeededAfterChildrenChanged(Element* beforeChange, ChildChangeType changeType)
{
    // FIXME: This function looks suspicious.
    if (document().renderView() && (changeType == ElementRemoved || changeType == TextRemoved)) {
        Node* node = beforeChange ? beforeChange->nextSibling() : nullptr;
        for (; node; node = node->nextSibling()) {
            if (elementAffectsDirectionality(*node))
                continue;

            setHasDirAutoFlagRecursively(node, false);
        }
    }

    if (!selfOrAncestorHasDirAutoAttribute())
        return;

    Node* oldMarkedNode = nullptr;
    if (beforeChange)
        oldMarkedNode = changeType == ElementInserted ? ElementTraversal::nextSibling(beforeChange) : beforeChange->nextSibling();

    while (oldMarkedNode && elementAffectsDirectionality(*oldMarkedNode))
        oldMarkedNode = oldMarkedNode->nextSibling();
    if (oldMarkedNode)
        setHasDirAutoFlagRecursively(oldMarkedNode, false);

    for (auto& elementToAdjust : lineageOfType<HTMLElement>(*this)) {
        if (elementAffectsDirectionality(elementToAdjust)) {
            elementToAdjust.calculateAndAdjustDirectionality();
            return;
        }
    }
}

bool HTMLElement::isURLAttribute(const Attribute& attribute) const
{
    return StyledElement::isURLAttribute(attribute);
}

void HTMLElement::addHTMLLengthToStyle(MutableStyleProperties& style, CSSPropertyID propertyID, const String& value)
{
    // FIXME: This function should not spin up the CSS parser, but should instead just figure out the correct
    // length unit and make the appropriate parsed value.

    if (StringImpl* string = value.impl()) {
        unsigned parsedLength = 0;

        while (parsedLength < string->length() && (*string)[parsedLength] <= ' ')
            ++parsedLength;

        for (; parsedLength < string->length(); ++parsedLength) {
            UChar cc = (*string)[parsedLength];
            if (cc > '9')
                break;
            if (cc < '0') {
                if (cc == '%' || cc == '*')
                    ++parsedLength;
                if (cc != '.')
                    break;
            }
        }

        if (parsedLength != string->length()) {
            addPropertyToPresentationAttributeStyle(style, propertyID, string->substring(0, parsedLength));
            return;
        }
    }

    addPropertyToPresentationAttributeStyle(style, propertyID, value);
}

static RGBA32 parseColorStringWithCrazyLegacyRules(const String& colorString)
{
    // Per spec, only look at the first 128 digits of the string.
    const size_t maxColorLength = 128;
    // We'll pad the buffer with two extra 0s later, so reserve two more than the max.
    Vector<char, maxColorLength+2> digitBuffer;

    size_t i = 0;
    // Skip a leading #.
    if (colorString[0] == '#')
        i = 1;

    // Grab the first 128 characters, replacing non-hex characters with 0.
    // Non-BMP characters are replaced with "00" due to them appearing as two "characters" in the String.
    for (; i < colorString.length() && digitBuffer.size() < maxColorLength; i++) {
        if (!isASCIIHexDigit(colorString[i]))
            digitBuffer.append('0');
        else
            digitBuffer.append(colorString[i]);
    }

    if (!digitBuffer.size())
        return Color::black;

    // Pad the buffer out to at least the next multiple of three in size.
    digitBuffer.append('0');
    digitBuffer.append('0');

    if (digitBuffer.size() < 6)
        return makeRGB(toASCIIHexValue(digitBuffer[0]), toASCIIHexValue(digitBuffer[1]), toASCIIHexValue(digitBuffer[2]));

    // Split the digits into three components, then search the last 8 digits of each component.
    ASSERT(digitBuffer.size() >= 6);
    size_t componentLength = digitBuffer.size() / 3;
    size_t componentSearchWindowLength = std::min<size_t>(componentLength, 8);
    size_t redIndex = componentLength - componentSearchWindowLength;
    size_t greenIndex = componentLength * 2 - componentSearchWindowLength;
    size_t blueIndex = componentLength * 3 - componentSearchWindowLength;
    // Skip digits until one of them is non-zero, or we've only got two digits left in the component.
    while (digitBuffer[redIndex] == '0' && digitBuffer[greenIndex] == '0' && digitBuffer[blueIndex] == '0' && (componentLength - redIndex) > 2) {
        redIndex++;
        greenIndex++;
        blueIndex++;
    }
    ASSERT(redIndex + 1 < componentLength);
    ASSERT(greenIndex >= componentLength);
    ASSERT(greenIndex + 1 < componentLength * 2);
    ASSERT(blueIndex >= componentLength * 2);
    ASSERT_WITH_SECURITY_IMPLICATION(blueIndex + 1 < digitBuffer.size());

    int redValue = toASCIIHexValue(digitBuffer[redIndex], digitBuffer[redIndex + 1]);
    int greenValue = toASCIIHexValue(digitBuffer[greenIndex], digitBuffer[greenIndex + 1]);
    int blueValue = toASCIIHexValue(digitBuffer[blueIndex], digitBuffer[blueIndex + 1]);
    return makeRGB(redValue, greenValue, blueValue);
}

// Color parsing that matches HTML's "rules for parsing a legacy color value"
void HTMLElement::addHTMLColorToStyle(MutableStyleProperties& style, CSSPropertyID propertyID, const String& attributeValue)
{
    // An empty string doesn't apply a color. (One containing only whitespace does, which is why this check occurs before stripping.)
    if (attributeValue.isEmpty())
        return;

    String colorString = attributeValue.stripWhiteSpace();

    // "transparent" doesn't apply a color either.
    if (equalIgnoringCase(colorString, "transparent"))
        return;

    // If the string is a named CSS color or a 3/6-digit hex color, use that.
    Color parsedColor(colorString);
    if (!parsedColor.isValid())
        parsedColor.setRGB(parseColorStringWithCrazyLegacyRules(colorString));

    style.setProperty(propertyID, cssValuePool().createColorValue(parsedColor.rgb()));
}

bool HTMLElement::willRespondToMouseMoveEvents()
{
    return !isDisabledFormControl() && Element::willRespondToMouseMoveEvents();
}

bool HTMLElement::willRespondToMouseWheelEvents()
{
    return !isDisabledFormControl() && Element::willRespondToMouseWheelEvents();
}

bool HTMLElement::willRespondToMouseClickEvents()
{
    return !isDisabledFormControl() && Element::willRespondToMouseClickEvents();
}

} // namespace WebCore

#ifndef NDEBUG

// For use in the debugger
void dumpInnerHTML(WebCore::HTMLElement*);

void dumpInnerHTML(WebCore::HTMLElement* element)
{
    printf("%s\n", element->innerHTML().ascii().data());
}

#endif
