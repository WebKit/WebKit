/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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

#include "CSSMarkup.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "CSSValuePool.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMTokenList.h"
#include "DocumentFragment.h"
#include "ElementAncestorIterator.h"
#include "EnterKeyHint.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLBDIElement.h"
#include "HTMLBRElement.h"
#include "HTMLButtonElement.h"
#include "HTMLDocument.h"
#include "HTMLElementFactory.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFormControlElement.h"
#include "NodeTraversal.h"
#include "RenderElement.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "SimulatedClick.h"
#include "StyleProperties.h"
#include "Text.h"
#include "XMLNames.h"
#include "markup.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLElement);

using namespace HTMLNames;

Ref<HTMLElement> HTMLElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLElement(tagName, document));
}

String HTMLElement::nodeName() const
{
    // FIXME: Would be nice to have an AtomString lookup based off uppercase
    // ASCII characters that does not have to copy the string on a hit in the hash.
    if (document().isHTMLDocument()) {
        if (LIKELY(!tagQName().hasPrefix()))
            return tagQName().localNameUpper();
        return Element::nodeName().convertToASCIIUppercase();
    }
    return Element::nodeName();
}

static inline CSSValueID unicodeBidiAttributeForDirAuto(HTMLElement& element)
{
    if (element.hasTagName(preTag) || element.hasTagName(textareaTag))
        return CSSValuePlaintext;
    // FIXME: For bdo element, dir="auto" should result in "bidi-override isolate" but we don't support having multiple values in unicode-bidi yet.
    // See https://bugs.webkit.org/show_bug.cgi?id=73164.
    return CSSValueIsolate;
}

unsigned HTMLElement::parseBorderWidthAttribute(const AtomString& value) const
{
    if (auto optionalBorderWidth = parseHTMLNonNegativeInteger(value))
        return optionalBorderWidth.value();

    return hasTagName(tableTag) ? 1 : 0;
}

void HTMLElement::applyBorderAttributeToStyle(const AtomString& value, MutableStyleProperties& style)
{
    addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderWidth, parseBorderWidthAttribute(value), CSSUnitType::CSS_PX);
    addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderStyle, CSSValueSolid);
}

void HTMLElement::mapLanguageAttributeToLocale(const AtomString& value, MutableStyleProperties& style)
{
    if (!value.isEmpty()) {
        // Have to quote so the locale id is treated as a string instead of as a CSS keyword.
        addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitLocale, serializeString(value));
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

static bool isLTROrRTLIgnoringCase(const AtomString& dirAttributeValue)
{
    return equalLettersIgnoringASCIICase(dirAttributeValue, "rtl") || equalLettersIgnoringASCIICase(dirAttributeValue, "ltr");
}

enum class ContentEditableType {
    Inherit,
    True,
    False,
    PlaintextOnly
};

static inline ContentEditableType contentEditableType(const AtomString& value)
{
    if (value.isNull())
        return ContentEditableType::Inherit;
    if (value.isEmpty() || equalLettersIgnoringASCIICase(value, "true"))
        return ContentEditableType::True;
    if (equalLettersIgnoringASCIICase(value, "false"))
        return ContentEditableType::False;
    if (equalLettersIgnoringASCIICase(value, "plaintext-only"))
        return ContentEditableType::PlaintextOnly;

    return ContentEditableType::Inherit;
}

static ContentEditableType contentEditableType(const HTMLElement& element)
{
    return contentEditableType(element.attributeWithoutSynchronization(contenteditableAttr));
}

void HTMLElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == alignAttr) {
        if (equalLettersIgnoringASCIICase(value, "middle"))
            addPropertyToPresentationAttributeStyle(style, CSSPropertyTextAlign, CSSValueCenter);
        else
            addPropertyToPresentationAttributeStyle(style, CSSPropertyTextAlign, value);
    } else if (name == contenteditableAttr) {
        CSSValueID userModifyValue = CSSValueReadWrite;
        switch (contentEditableType(value)) {
        case ContentEditableType::Inherit:
            return;
        case ContentEditableType::False:
            userModifyValue = CSSValueReadOnly;
            break;
        case ContentEditableType::PlaintextOnly:
            userModifyValue = CSSValueReadWritePlaintextOnly;
            FALLTHROUGH;
        case ContentEditableType::True:
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWordWrap, CSSValueBreakWord);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitNbspMode, CSSValueSpace);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyLineBreak, CSSValueAfterWhiteSpace);
#if PLATFORM(IOS_FAMILY)
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitTextSizeAdjust, CSSValueNone);
#endif
            break;
        }
        addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserModify, userModifyValue);
    } else if (name == hiddenAttr) {
        addPropertyToPresentationAttributeStyle(style, CSSPropertyDisplay, CSSValueNone);
    } else if (name == draggableAttr) {
        if (equalLettersIgnoringASCIICase(value, "true")) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserDrag, CSSValueElement);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserSelect, CSSValueNone);
        } else if (equalLettersIgnoringASCIICase(value, "false"))
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWebkitUserDrag, CSSValueNone);
    } else if (name == dirAttr) {
        if (equalLettersIgnoringASCIICase(value, "auto"))
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
        if (!hasAttributeWithoutSynchronization(XMLNames::langAttr))
            mapLanguageAttributeToLocale(value, style);
    } else
        StyledElement::collectStyleForPresentationAttribute(name, value, style);
}

HTMLElement::EventHandlerNameMap HTMLElement::createEventHandlerNameMap()
{
    EventHandlerNameMap map;

    static const QualifiedName* const table[] = {
        &onabortAttr.get(),
        &onanimationendAttr.get(),
        &onanimationiterationAttr.get(),
        &onanimationstartAttr.get(),
        &onanimationcancelAttr.get(),
        &onautocompleteAttr.get(),
        &onautocompleteerrorAttr.get(),
        &onbeforecopyAttr.get(),
        &onbeforecutAttr.get(),
        &onbeforeinputAttr.get(),
        &onbeforeloadAttr.get(),
        &onbeforepasteAttr.get(),
        &onblurAttr.get(),
        &oncanplayAttr.get(),
        &oncanplaythroughAttr.get(),
        &onchangeAttr.get(),
        &onclickAttr.get(),
        &oncontextmenuAttr.get(),
        &oncopyAttr.get(),
        &oncutAttr.get(),
        &ondblclickAttr.get(),
        &ondragAttr.get(),
        &ondragendAttr.get(),
        &ondragenterAttr.get(),
        &ondragleaveAttr.get(),
        &ondragoverAttr.get(),
        &ondragstartAttr.get(),
        &ondropAttr.get(),
        &ondurationchangeAttr.get(),
        &onemptiedAttr.get(),
        &onendedAttr.get(),
        &onerrorAttr.get(),
        &onfocusAttr.get(),
        &onfocusinAttr.get(),
        &onfocusoutAttr.get(),
        &ongesturechangeAttr.get(),
        &ongestureendAttr.get(),
        &ongesturestartAttr.get(),
        &ongotpointercaptureAttr.get(),
        &oninputAttr.get(),
        &oninvalidAttr.get(),
        &onkeydownAttr.get(),
        &onkeypressAttr.get(),
        &onkeyupAttr.get(),
        &onloadAttr.get(),
        &onloadeddataAttr.get(),
        &onloadedmetadataAttr.get(),
        &onloadstartAttr.get(),
        &onlostpointercaptureAttr.get(),
        &onmousedownAttr.get(),
        &onmouseenterAttr.get(),
        &onmouseleaveAttr.get(),
        &onmousemoveAttr.get(),
        &onmouseoutAttr.get(),
        &onmouseoverAttr.get(),
        &onmouseupAttr.get(),
        &onmousewheelAttr.get(),
        &onpasteAttr.get(),
        &onpauseAttr.get(),
        &onplayAttr.get(),
        &onplayingAttr.get(),
        &onpointerdownAttr.get(),
        &onpointermoveAttr.get(),
        &onpointerupAttr.get(),
        &onpointercancelAttr.get(),
        &onpointeroverAttr.get(),
        &onpointeroutAttr.get(),
        &onpointerenterAttr.get(),
        &onpointerleaveAttr.get(),
        &onprogressAttr.get(),
        &onratechangeAttr.get(),
        &onresetAttr.get(),
        &onresizeAttr.get(),
        &onscrollAttr.get(),
        &onsearchAttr.get(),
        &onseekedAttr.get(),
        &onseekingAttr.get(),
        &onselectAttr.get(),
        &onselectstartAttr.get(),
        &onstalledAttr.get(),
        &onsubmitAttr.get(),
        &onsuspendAttr.get(),
        &ontimeupdateAttr.get(),
        &ontoggleAttr.get(),
        &ontouchcancelAttr.get(),
        &ontouchendAttr.get(),
        &ontouchforcechangeAttr.get(),
        &ontouchmoveAttr.get(),
        &ontouchstartAttr.get(),
        &ontransitioncancelAttr.get(),
        &ontransitionendAttr.get(),
        &ontransitionrunAttr.get(),
        &ontransitionstartAttr.get(),
        &onvolumechangeAttr.get(),
        &onwaitingAttr.get(),
        &onwebkitbeginfullscreenAttr.get(),
        &onwebkitcurrentplaybacktargetiswirelesschangedAttr.get(),
        &onwebkitendfullscreenAttr.get(),
        &onwebkitfullscreenchangeAttr.get(),
        &onwebkitfullscreenerrorAttr.get(),
        &onwebkitkeyaddedAttr.get(),
        &onwebkitkeyerrorAttr.get(),
        &onwebkitkeymessageAttr.get(),
        &onwebkitmouseforcechangedAttr.get(),
        &onwebkitmouseforcedownAttr.get(),
        &onwebkitmouseforcewillbeginAttr.get(),
        &onwebkitmouseforceupAttr.get(),
        &onwebkitneedkeyAttr.get(),
        &onwebkitplaybacktargetavailabilitychangedAttr.get(),
        &onwebkitpresentationmodechangedAttr.get(),
        &onwebkitwillrevealbottomAttr.get(),
        &onwebkitwillrevealleftAttr.get(),
        &onwebkitwillrevealrightAttr.get(),
        &onwebkitwillrevealtopAttr.get(),
        &onwheelAttr.get(),
    };

    populateEventHandlerNameMap(map, table);

    struct UnusualMapping {
        const QualifiedName& attributeName;
        const AtomString& eventName;
    };

    const UnusualMapping unusualPairsTable[] = {
        { onwebkitanimationendAttr, eventNames().webkitAnimationEndEvent },
        { onwebkitanimationiterationAttr, eventNames().webkitAnimationIterationEvent },
        { onwebkitanimationstartAttr, eventNames().webkitAnimationStartEvent },
        { onwebkittransitionendAttr, eventNames().webkitTransitionEndEvent },
    };

    for (auto& entry : unusualPairsTable)
        map.add(entry.attributeName.localName().impl(), entry.eventName);

    return map;
}

void HTMLElement::populateEventHandlerNameMap(EventHandlerNameMap& map, const QualifiedName* const table[], size_t tableSize)
{
    for (size_t i = 0; i < tableSize; ++i) {
        auto* entry = table[i];

        // FIXME: Would be nice to check these against the actual event names in eventNames().
        // Not obvious how to do that simply, though.
        auto& attributeName = entry->localName();

        // Remove the "on" prefix. Requires some memory allocation and computing a hash, but by not
        // using pointers from eventNames(), the passed-in table can be initialized at compile time.
        AtomString eventName = attributeName.string().substring(2);

        map.add(attributeName.impl(), WTFMove(eventName));
    }
}

const AtomString& HTMLElement::eventNameForEventHandlerAttribute(const QualifiedName& attributeName, const EventHandlerNameMap& map)
{
    ASSERT(!attributeName.localName().isNull());

    // Event handler attributes have no namespace.
    if (!attributeName.namespaceURI().isNull())
        return nullAtom();

    // Fast early return for names that don't start with "on".
    AtomStringImpl& localName = *attributeName.localName().impl();
    if (localName.length() < 3 || localName[0] != 'o' || localName[1] != 'n')
        return nullAtom();

    auto it = map.find(&localName);
    return it == map.end() ? nullAtom() : it->value;
}

const AtomString& HTMLElement::eventNameForEventHandlerAttribute(const QualifiedName& attributeName)
{
    static NeverDestroyed<EventHandlerNameMap> map = createEventHandlerNameMap();
    return eventNameForEventHandlerAttribute(attributeName, map.get());
}

Node::Editability HTMLElement::editabilityFromContentEditableAttr(const Node& node)
{
    if (auto* startElement = is<Element>(node) ? &downcast<Element>(node) : node.parentElement()) {
        for (auto& element : lineageOfType<HTMLElement>(*startElement)) {
            switch (contentEditableType(element)) {
            case ContentEditableType::True:
                return Editability::CanEditRichly;
            case ContentEditableType::PlaintextOnly:
                return Editability::CanEditPlainText;
            case ContentEditableType::False:
                return Editability::ReadOnly;
            case ContentEditableType::Inherit:
                break;
            }
        }
    }

    auto containingShadowRoot = makeRefPtr(node.containingShadowRoot());
    if (containingShadowRoot && containingShadowRoot->mode() == ShadowRootMode::UserAgent)
        return Editability::ReadOnly;

    auto& document = node.document();
    if (is<HTMLDocument>(document))
        return downcast<HTMLDocument>(document).inDesignMode() ? Editability::CanEditRichly : Editability::ReadOnly;

    return Editability::ReadOnly;
}

bool HTMLElement::matchesReadWritePseudoClass() const
{
    return editabilityFromContentEditableAttr(*this) != Editability::ReadOnly;
}

void HTMLElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == dirAttr) {
        dirAttributeChanged(value);
        return;
    }

    if (name == tabindexAttr) {
        if (value.isEmpty())
            clearTabIndexExplicitlyIfNeeded();
        else if (auto optionalTabIndex = parseHTMLInteger(value))
            setTabIndexExplicitly(optionalTabIndex.value());
        return;
    }
    
    if (name == inputmodeAttr) {
        auto& document = this->document();
        if (this == document.focusedElement()) {
            if (auto* page = document.page())
                page->chrome().client().focusedElementDidChangeInputMode(*this, canonicalInputMode());
        }
    }

    auto& eventName = eventNameForEventHandlerAttribute(name);
    if (!eventName.isNull())
        setAttributeEventListener(eventName, name, value);
}

static Ref<DocumentFragment> textToFragment(Document& document, const String& text)
{
    auto fragment = DocumentFragment::create(document);

    // It's safe to dispatch events on the new fragment since author scripts have no access to it yet.
    ScriptDisallowedScope::EventAllowedScope allowedScope(fragment);

    for (unsigned start = 0, length = text.length(); start < length; ) {
        // Find next line break.
        UChar c = 0;
        unsigned i;
        for (i = start; i < length; i++) {
            c = text[i];
            if (c == '\r' || c == '\n')
                break;
        }

        // If text is not the empty string, then append a new Text node whose data is text and node document is document to fragment.
        if (i > start)
            fragment->appendChild(Text::create(document, text.substring(start, i - start)));

        if (i == length)
            break;

        fragment->appendChild(HTMLBRElement::create(document));
        // Make sure \r\n doesn't result in two line breaks.
        if (c == '\r' && i + 1 < length && text[i + 1] == '\n')
            ++i;

        start = i + 1; // Character after line break.
    }

    return fragment;
}

// Returns the conforming 'dir' value associated with the state the attribute is in (in its canonical case), if any,
// or the empty string if the attribute is in a state that has no associated keyword value or if the attribute is
// not in a defined state (e.g. the attribute is missing and there is no missing value default).
// http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#limited-to-only-known-values
static inline const AtomString& toValidDirValue(const AtomString& value)
{
    static MainThreadNeverDestroyed<const AtomString> ltrValue("ltr", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> rtlValue("rtl", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> autoValue("auto", AtomString::ConstructFromLiteral);
    if (equalLettersIgnoringASCIICase(value, "ltr"))
        return ltrValue;
    if (equalLettersIgnoringASCIICase(value, "rtl"))
        return rtlValue;
    if (equalLettersIgnoringASCIICase(value, "auto"))
        return autoValue;
    return nullAtom();
}

const AtomString& HTMLElement::dir() const
{
    return toValidDirValue(attributeWithoutSynchronization(dirAttr));
}

void HTMLElement::setDir(const AtomString& value)
{
    setAttributeWithoutSynchronization(dirAttr, value);
}

ExceptionOr<void> HTMLElement::setInnerText(const String& text)
{
    // FIXME: This doesn't take whitespace collapsing into account at all.

    if (!text.contains('\n') && !text.contains('\r')) {
        if (text.isEmpty())
            replaceAllChildren(nullptr);
        else
            replaceAllChildren(document().createTextNode(text));
        return { };
    }

    // FIXME: Do we need to be able to detect preserveNewline style even when there's no renderer?
    // FIXME: Can the renderer be out of date here? Do we need to call updateStyleIfNeeded?
    // For example, for the contents of textarea elements that are display:none?
    auto* r = renderer();
    if ((r && r->style().preserveNewline()) || (isConnected() && isTextControlInnerTextElement())) {
        if (!text.contains('\r')) {
            replaceAllChildren(document().createTextNode(text));
            return { };
        }
        String textWithConsistentLineBreaks = text;
        textWithConsistentLineBreaks.replace("\r\n", "\n");
        textWithConsistentLineBreaks.replace('\r', '\n');
        replaceAllChildren(document().createTextNode(textWithConsistentLineBreaks));
        return { };
    }

    // Add text nodes and <br> elements.
    auto fragment = textToFragment(document(), text);
    // FIXME: This should use replaceAllChildren() once it accepts DocumentFragments as input.
    // It's safe to dispatch events on the new fragment since author scripts have no access to it yet.
    ScriptDisallowedScope::EventAllowedScope allowedScope(fragment.get());
    return replaceChildrenWithFragment(*this, WTFMove(fragment));
}

ExceptionOr<void> HTMLElement::setOuterText(const String& text)
{
    RefPtr<ContainerNode> parent = parentNode();
    if (!parent)
        return Exception { NoModificationAllowedError };

    RefPtr<Node> prev = previousSibling();
    RefPtr<Node> next = nextSibling();
    RefPtr<Node> newChild;

    // Convert text to fragment with <br> tags instead of linebreaks if needed.
    if (text.contains('\r') || text.contains('\n'))
        newChild = textToFragment(document(), text);
    else
        newChild = Text::create(document(), text);

    if (!parentNode())
        return Exception { HierarchyRequestError };

    auto replaceResult = parent->replaceChild(*newChild, *this);
    if (replaceResult.hasException())
        return replaceResult.releaseException();

    RefPtr<Node> node = next ? next->previousSibling() : nullptr;
    if (is<Text>(node)) {
        auto result = mergeWithNextTextNode(downcast<Text>(*node));
        if (result.hasException())
            return result.releaseException();
    }
    if (is<Text>(prev)) {
        auto result = mergeWithNextTextNode(downcast<Text>(*prev));
        if (result.hasException())
            return result.releaseException();
    }
    return { };
}

void HTMLElement::applyAlignmentAttributeToStyle(const AtomString& alignment, MutableStyleProperties& style)
{
    // Vertical alignment with respect to the current baseline of the text
    // right or left means floating images.
    CSSValueID floatValue = CSSValueInvalid;
    CSSValueID verticalAlignValue = CSSValueInvalid;

    if (equalLettersIgnoringASCIICase(alignment, "absmiddle"))
        verticalAlignValue = CSSValueMiddle;
    else if (equalLettersIgnoringASCIICase(alignment, "absbottom"))
        verticalAlignValue = CSSValueBottom;
    else if (equalLettersIgnoringASCIICase(alignment, "left")) {
        floatValue = CSSValueLeft;
        verticalAlignValue = CSSValueTop;
    } else if (equalLettersIgnoringASCIICase(alignment, "right")) {
        floatValue = CSSValueRight;
        verticalAlignValue = CSSValueTop;
    } else if (equalLettersIgnoringASCIICase(alignment, "top"))
        verticalAlignValue = CSSValueTop;
    else if (equalLettersIgnoringASCIICase(alignment, "middle"))
        verticalAlignValue = CSSValueWebkitBaselineMiddle;
    else if (equalLettersIgnoringASCIICase(alignment, "center"))
        verticalAlignValue = CSSValueMiddle;
    else if (equalLettersIgnoringASCIICase(alignment, "bottom"))
        verticalAlignValue = CSSValueBaseline;
    else if (equalLettersIgnoringASCIICase(alignment, "texttop"))
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
    switch (contentEditableType(*this)) {
    case ContentEditableType::Inherit:
        return "inherit"_s;
    case ContentEditableType::True:
        return "true"_s;
    case ContentEditableType::False:
        return "false"_s;
    case ContentEditableType::PlaintextOnly:
        return "plaintext-only"_s;
    }
    return "inherit"_s;
}

static const AtomString& trueName()
{
    static MainThreadNeverDestroyed<const AtomString> trueValue("true", AtomString::ConstructFromLiteral);
    return trueValue.get();
}

static const AtomString& falseName()
{
    static MainThreadNeverDestroyed<const AtomString> falseValue("false", AtomString::ConstructFromLiteral);
    return falseValue.get();
}

static const AtomString& plaintextOnlyName()
{
    static MainThreadNeverDestroyed<const AtomString> plaintextOnlyValue("plaintext-only", AtomString::ConstructFromLiteral);
    return plaintextOnlyValue.get();
}

ExceptionOr<void> HTMLElement::setContentEditable(const String& enabled)
{
    if (equalLettersIgnoringASCIICase(enabled, "true"))
        setAttributeWithoutSynchronization(contenteditableAttr, trueName());
    else if (equalLettersIgnoringASCIICase(enabled, "false"))
        setAttributeWithoutSynchronization(contenteditableAttr, falseName());
    else if (equalLettersIgnoringASCIICase(enabled, "plaintext-only"))
        setAttributeWithoutSynchronization(contenteditableAttr, plaintextOnlyName());
    else if (equalLettersIgnoringASCIICase(enabled, "inherit"))
        removeAttribute(contenteditableAttr);
    else
        return Exception { SyntaxError };
    return { };
}

bool HTMLElement::draggable() const
{
    return equalLettersIgnoringASCIICase(attributeWithoutSynchronization(draggableAttr), "true");
}

void HTMLElement::setDraggable(bool value)
{
    setAttributeWithoutSynchronization(draggableAttr, value ? trueName() : falseName());
}

bool HTMLElement::spellcheck() const
{
    return isSpellCheckingEnabled();
}

void HTMLElement::setSpellcheck(bool enable)
{
    setAttributeWithoutSynchronization(spellcheckAttr, enable ? trueName() : falseName());
}

void HTMLElement::click()
{
    simulateClick(*this, nullptr, SendNoEvents, DoNotShowPressedLook, SimulatedClickSource::Bindings);
}

bool HTMLElement::accessKeyAction(bool sendMouseEvents)
{
    return dispatchSimulatedClick(nullptr, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
}

String HTMLElement::title() const
{
    return attributeWithoutSynchronization(titleAttr);
}

bool HTMLElement::translate() const
{
    for (auto& element : lineageOfType<HTMLElement>(*this)) {
        const AtomString& value = element.attributeWithoutSynchronization(translateAttr);
        if (equalLettersIgnoringASCIICase(value, "yes") || (value.isEmpty() && !value.isNull()))
            return true;
        if (equalLettersIgnoringASCIICase(value, "no"))
            return false;
    }
    // Default on the root element is translate=yes.
    return true;
}

void HTMLElement::setTranslate(bool enable)
{
    setAttributeWithoutSynchronization(translateAttr, enable ? "yes" : "no");
}

bool HTMLElement::rendererIsEverNeeded()
{
    if (hasTagName(noscriptTag)) {
        RefPtr<Frame> frame = document().frame();
        if (frame && frame->script().canExecuteScripts(NotAboutToExecuteScript))
            return false;
    } else if (hasTagName(noembedTag)) {
        RefPtr<Frame> frame = document().frame();
        if (frame && frame->loader().arePluginsEnabled())
            return false;
    }
    return StyledElement::rendererIsEverNeeded();
}

RenderPtr<RenderElement> HTMLElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return RenderElement::createFor(*this, WTFMove(style));
}

HTMLFormElement* HTMLElement::form() const
{
    return HTMLFormElement::findClosestFormAncestor(*this);
}

FormNamedItem* HTMLElement::asFormNamedItem()
{
    return nullptr;
}

FormAssociatedElement* HTMLElement::asFormAssociatedElement()
{
    return nullptr;
}

static bool elementAffectsDirectionality(const HTMLElement& element)
{
    return is<HTMLBDIElement>(element) || element.hasAttributeWithoutSynchronization(dirAttr);
}

static bool elementAffectsDirectionality(const Node& node)
{
    return is<HTMLElement>(node) && elementAffectsDirectionality(downcast<HTMLElement>(node));
}

static void setHasDirAutoFlagRecursively(Node* firstNode, bool flag, Node* lastNode = nullptr)
{
    firstNode->setSelfOrAncestorHasDirAutoAttribute(flag);

    RefPtr<Node> node = firstNode->firstChild();

    while (node) {
        if (node->selfOrAncestorHasDirAutoAttribute() == flag)
            return;

        if (elementAffectsDirectionality(*node)) {
            if (node == lastNode)
                return;
            node = NodeTraversal::nextSkippingChildren(*node, firstNode);
            continue;
        }
        node->setSelfOrAncestorHasDirAutoAttribute(flag);
        if (node == lastNode)
            return;
        node = NodeTraversal::next(*node, firstNode);
    }
}

void HTMLElement::childrenChanged(const ChildChange& change)
{
    StyledElement::childrenChanged(change);
    adjustDirectionalityIfNeededAfterChildrenChanged(change.previousSiblingElement, change.type);
}

bool HTMLElement::hasDirectionAuto() const
{
    const AtomString& direction = attributeWithoutSynchronization(dirAttr);
    return (hasTagName(bdiTag) && direction.isNull()) || equalLettersIgnoringASCIICase(direction, "auto");
}

TextDirection HTMLElement::directionalityIfhasDirAutoAttribute(bool& isAuto) const
{
    if (!(selfOrAncestorHasDirAutoAttribute() && hasDirectionAuto())) {
        isAuto = false;
        return TextDirection::LTR;
    }

    isAuto = true;
    return directionality();
}

TextDirection HTMLElement::directionality(Node** strongDirectionalityTextNode) const
{
    if (isTextField()) {
        HTMLTextFormControlElement& textElement = downcast<HTMLTextFormControlElement>(const_cast<HTMLElement&>(*this));
        bool hasStrongDirectionality;
        UCharDirection textDirection = textElement.value().defaultWritingDirection(&hasStrongDirectionality);
        if (strongDirectionalityTextNode)
            *strongDirectionalityTextNode = hasStrongDirectionality ? &textElement : nullptr;
        return (textDirection == U_LEFT_TO_RIGHT) ? TextDirection::LTR : TextDirection::RTL;
    }

    RefPtr<Node> node = firstChild();
    while (node) {
        // Skip bdi, script, style and text form controls.
        if (equalLettersIgnoringASCIICase(node->nodeName(), "bdi") || node->hasTagName(scriptTag) || node->hasTagName(styleTag)
            || (is<Element>(*node) && downcast<Element>(*node).isTextField())) {
            node = NodeTraversal::nextSkippingChildren(*node, this);
            continue;
        }

        // Skip elements with valid dir attribute
        if (is<Element>(*node)) {
            auto& dirAttributeValue = downcast<Element>(*node).attributeWithoutSynchronization(dirAttr);
            if (isLTROrRTLIgnoringCase(dirAttributeValue) || equalLettersIgnoringASCIICase(dirAttributeValue, "auto")) {
                node = NodeTraversal::nextSkippingChildren(*node, this);
                continue;
            }
        }

        if (node->isTextNode()) {
            bool hasStrongDirectionality;
            UCharDirection textDirection = node->textContent(true).defaultWritingDirection(&hasStrongDirectionality);
            if (hasStrongDirectionality) {
                if (strongDirectionalityTextNode)
                    *strongDirectionalityTextNode = node.get();
                return (textDirection == U_LEFT_TO_RIGHT) ? TextDirection::LTR : TextDirection::RTL;
            }
        }
        node = NodeTraversal::next(*node, this);
    }
    if (strongDirectionalityTextNode)
        *strongDirectionalityTextNode = nullptr;
    return TextDirection::LTR;
}

void HTMLElement::dirAttributeChanged(const AtomString& value)
{
    RefPtr<Element> parent = parentElement();

    if (is<HTMLElement>(parent) && parent->selfOrAncestorHasDirAutoAttribute())
        downcast<HTMLElement>(*parent).adjustDirectionalityIfNeededAfterChildAttributeChanged(this);

    if (equalLettersIgnoringASCIICase(value, "auto"))
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
    for (auto& element : lineageOfType<HTMLElement>(*this)) {
        if (elementAffectsDirectionality(element)) {
            element.invalidateStyleForSubtree();
            break;
        }
    }
}

void HTMLElement::calculateAndAdjustDirectionality()
{
    Node* strongDirectionalityTextNode;
    TextDirection textDirection = directionality(&strongDirectionalityTextNode);
    setHasDirAutoFlagRecursively(this, true, strongDirectionalityTextNode);
    if (renderer() && renderer()->style().direction() != textDirection)
        invalidateStyleForSubtree();
}

void HTMLElement::adjustDirectionalityIfNeededAfterChildrenChanged(Element* beforeChange, ChildChangeType changeType)
{
    // FIXME: This function looks suspicious.

    if (!selfOrAncestorHasDirAutoAttribute())
        return;

    RefPtr<Node> oldMarkedNode;
    if (beforeChange)
        oldMarkedNode = changeType == ElementInserted ? ElementTraversal::nextSibling(*beforeChange) : beforeChange->nextSibling();

    while (oldMarkedNode && elementAffectsDirectionality(*oldMarkedNode))
        oldMarkedNode = oldMarkedNode->nextSibling();
    if (oldMarkedNode)
        setHasDirAutoFlagRecursively(oldMarkedNode.get(), false);

    for (auto& elementToAdjust : lineageOfType<HTMLElement>(*this)) {
        if (elementAffectsDirectionality(elementToAdjust)) {
            elementToAdjust.calculateAndAdjustDirectionality();
            return;
        }
    }
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

// Color parsing that matches HTML's "rules for parsing a legacy color value"
// https://html.spec.whatwg.org/#rules-for-parsing-a-legacy-colour-value
static Optional<SimpleColor> parseLegacyColorValue(StringView string)
{
    // An empty string doesn't apply a color.
    if (string.isEmpty())
        return WTF::nullopt;

    string = string.stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>);
    if (string.isEmpty())
        return Color::black;

    // "transparent" doesn't apply a color either.
    if (equalLettersIgnoringASCIICase(string, "transparent"))
        return WTF::nullopt;

    if (auto namedColor = CSSParser::parseNamedColor(string))
        return namedColor;

    if (string.length() == 4 && string[0] == '#' && isASCIIHexDigit(string[1]) && isASCIIHexDigit(string[2]) && isASCIIHexDigit(string[3]))
        return makeSimpleColor(toASCIIHexValue(string[1]) * 0x11, toASCIIHexValue(string[2]) * 0x11, toASCIIHexValue(string[3]) * 0x11);

    // Per spec, only look at the first 128 digits of the string.
    constexpr unsigned maxColorLength = 128;

    // We'll pad the buffer with two extra 0s later, so reserve two more than the max.
    Vector<char, maxColorLength + 2> digitBuffer;

    // Grab the first 128 characters, replacing non-hex characters with 0.
    // Non-BMP characters are replaced with "00" due to them appearing as two "characters" in the String.
    unsigned i = 0;
    if (string[0] == '#') // Skip a leading #.
        i = 1;
    for (; i < string.length() && digitBuffer.size() < maxColorLength; i++) {
        if (!isASCIIHexDigit(string[i]))
            digitBuffer.append('0');
        else
            digitBuffer.append(string[i]);
    }

    if (digitBuffer.isEmpty())
        return Color::black;

    // Pad the buffer out to at least the next multiple of three in size.
    digitBuffer.append('0');
    digitBuffer.append('0');

    if (digitBuffer.size() < 6)
        return makeSimpleColor(toASCIIHexValue(digitBuffer[0]), toASCIIHexValue(digitBuffer[1]), toASCIIHexValue(digitBuffer[2]));

    // Split the digits into three components, then search the last 8 digits of each component.
    ASSERT(digitBuffer.size() >= 6);
    unsigned componentLength = digitBuffer.size() / 3;
    unsigned componentSearchWindowLength = std::min(componentLength, 8U);
    unsigned redIndex = componentLength - componentSearchWindowLength;
    unsigned greenIndex = componentLength * 2 - componentSearchWindowLength;
    unsigned blueIndex = componentLength * 3 - componentSearchWindowLength;
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
    return makeSimpleColor(redValue, greenValue, blueValue);
}

void HTMLElement::addHTMLColorToStyle(MutableStyleProperties& style, CSSPropertyID propertyID, const String& attributeValue)
{
    if (auto color = parseLegacyColorValue(attributeValue))
        style.setProperty(propertyID, CSSValuePool::singleton().createColorValue(*color));
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

bool HTMLElement::canBeActuallyDisabled() const
{
    return is<HTMLButtonElement>(*this)
        || is<HTMLInputElement>(*this)
        || is<HTMLSelectElement>(*this)
        || is<HTMLTextAreaElement>(*this)
        || is<HTMLOptGroupElement>(*this)
        || is<HTMLOptionElement>(*this)
        || is<HTMLFieldSetElement>(*this);
}

bool HTMLElement::isActuallyDisabled() const
{
    return canBeActuallyDisabled() && isDisabledFormControl();
}

#if ENABLE(AUTOCAPITALIZE)

const AtomString& HTMLElement::autocapitalize() const
{
    return stringForAutocapitalizeType(autocapitalizeType());
}

AutocapitalizeType HTMLElement::autocapitalizeType() const
{
    return autocapitalizeTypeForAttributeValue(attributeWithoutSynchronization(HTMLNames::autocapitalizeAttr));
}

void HTMLElement::setAutocapitalize(const AtomString& value)
{
    setAttributeWithoutSynchronization(autocapitalizeAttr, value);
}

#endif

#if ENABLE(AUTOCORRECT)

bool HTMLElement::shouldAutocorrect() const
{
    auto& autocorrectValue = attributeWithoutSynchronization(HTMLNames::autocorrectAttr);
    // Unrecognized values fall back to "on".
    return !equalLettersIgnoringASCIICase(autocorrectValue, "off");
}

void HTMLElement::setAutocorrect(bool autocorrect)
{
    static MainThreadNeverDestroyed<const AtomString> onName("on", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> offName("off", AtomString::ConstructFromLiteral);
    setAttributeWithoutSynchronization(autocorrectAttr, autocorrect ? onName.get() : offName.get());
}

#endif

InputMode HTMLElement::canonicalInputMode() const
{
    auto mode = inputModeForAttributeValue(attributeWithoutSynchronization(inputmodeAttr));
    if (mode == InputMode::Unspecified) {
        if (document().quirks().needsInputModeNoneImplicitly(*this))
            return InputMode::None;
    }
    return mode;
}

const AtomString& HTMLElement::inputMode() const
{
    return stringForInputMode(canonicalInputMode());
}

void HTMLElement::setInputMode(const AtomString& value)
{
    setAttributeWithoutSynchronization(inputmodeAttr, value);
}

EnterKeyHint HTMLElement::canonicalEnterKeyHint() const
{
    return enterKeyHintForAttributeValue(attributeWithoutSynchronization(enterkeyhintAttr));
}

String HTMLElement::enterKeyHint() const
{
    return attributeValueForEnterKeyHint(canonicalEnterKeyHint());
}

void HTMLElement::setEnterKeyHint(const String& value)
{
    setAttributeWithoutSynchronization(enterkeyhintAttr, value);
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
