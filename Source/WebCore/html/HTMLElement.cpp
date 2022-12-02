/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2021 Apple Inc. All rights reserved.
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
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "CommonAtomStrings.h"
#include "CustomElementReactionQueue.h"
#include "DOMTokenList.h"
#include "DocumentFragment.h"
#include "ElementAncestorIterator.h"
#include "ElementInternals.h"
#include "ElementRareData.h"
#include "EnterKeyHint.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventLoop.h"
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
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFormControlElement.h"
#include "ImageOverlay.h"
#include "JSHTMLElement.h"
#include "MediaControlsHost.h"
#include "NodeTraversal.h"
#include "PseudoClassChangeInvalidation.h"
#include "RenderElement.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "SimulatedClick.h"
#include "StyleProperties.h"
#include "Text.h"
#include "UserAgentStyleSheets.h"
#include "XMLNames.h"
#include "markup.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Range.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(IOS_FAMILY)
#include "SelectionGeometry.h"
#endif

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

    return hasTagName(tableTag) && !value.isNull() ? 1 : 0;
}

void HTMLElement::applyBorderAttributeToStyle(const AtomString& value, MutableStyleProperties& style)
{
    addPropertyToPresentationalHintStyle(style, CSSPropertyBorderWidth, parseBorderWidthAttribute(value), CSSUnitType::CSS_PX);
    addPropertyToPresentationalHintStyle(style, CSSPropertyBorderStyle, CSSValueSolid);
}

void HTMLElement::mapLanguageAttributeToLocale(const AtomString& value, MutableStyleProperties& style)
{
    if (!value.isEmpty()) {
        // Have to quote so the locale id is treated as a string instead of as a CSS keyword.
        addPropertyToPresentationalHintStyle(style, CSSPropertyWebkitLocale, serializeString(value));
    } else {
        // The empty string means the language is explicitly unknown.
        addPropertyToPresentationalHintStyle(style, CSSPropertyWebkitLocale, CSSValueAuto);
    }
}

bool HTMLElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == alignAttr || name == contenteditableAttr || name == hiddenAttr || name == langAttr || name.matches(XMLNames::langAttr) || name == draggableAttr || name == dirAttr)
        return true;
    return StyledElement::hasPresentationalHintsForAttribute(name);
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
    if (value.isEmpty() || equalLettersIgnoringASCIICase(value, "true"_s))
        return ContentEditableType::True;
    if (equalLettersIgnoringASCIICase(value, "false"_s))
        return ContentEditableType::False;
    if (equalLettersIgnoringASCIICase(value, "plaintext-only"_s))
        return ContentEditableType::PlaintextOnly;

    return ContentEditableType::Inherit;
}

static ContentEditableType contentEditableType(const HTMLElement& element)
{
    return contentEditableType(element.attributeWithoutSynchronization(contenteditableAttr));
}

enum class TextDirectionDirective {
    Invalid,
    LTR,
    RTL,
    Auto,
};

static inline TextDirectionDirective parseTextDirection(const AtomString& value)
{
    if (equalLettersIgnoringASCIICase(value, "ltr"_s))
        return TextDirectionDirective::LTR;
    if (equalLettersIgnoringASCIICase(value, "rtl"_s))
        return TextDirectionDirective::RTL;
    if (equalLettersIgnoringASCIICase(value, "auto"_s))
        return TextDirectionDirective::Auto;
    return TextDirectionDirective::Invalid;
}

static bool isValidDirValue(const AtomString& value)
{
    return parseTextDirection(value) != TextDirectionDirective::Invalid;
}

static bool elementAffectsDirectionality(const HTMLElement& element)
{
    return is<HTMLBDIElement>(element) || isValidDirValue(element.attributeWithoutSynchronization(dirAttr));
}

static bool elementAffectsDirectionality(const Node& node)
{
    return is<HTMLElement>(node) && elementAffectsDirectionality(downcast<HTMLElement>(node));
}

void HTMLElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == alignAttr) {
        if (equalLettersIgnoringASCIICase(value, "middle"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyTextAlign, CSSValueCenter);
        else
            addPropertyToPresentationalHintStyle(style, CSSPropertyTextAlign, value);
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
            addPropertyToPresentationalHintStyle(style, CSSPropertyOverflowWrap, CSSValueBreakWord);
            addPropertyToPresentationalHintStyle(style, CSSPropertyWebkitNbspMode, CSSValueSpace);
            addPropertyToPresentationalHintStyle(style, CSSPropertyLineBreak, CSSValueAfterWhiteSpace);
#if PLATFORM(IOS_FAMILY)
            addPropertyToPresentationalHintStyle(style, CSSPropertyWebkitTextSizeAdjust, CSSValueNone);
#endif
            break;
        }
        addPropertyToPresentationalHintStyle(style, CSSPropertyWebkitUserModify, userModifyValue);
    } else if (name == hiddenAttr) {
        addPropertyToPresentationalHintStyle(style, CSSPropertyDisplay, CSSValueNone);
    } else if (name == draggableAttr) {
        if (equalLettersIgnoringASCIICase(value, "true"_s)) {
            addPropertyToPresentationalHintStyle(style, CSSPropertyWebkitUserDrag, CSSValueElement);
            if (!isDraggableIgnoringAttributes())
                addPropertyToPresentationalHintStyle(style, CSSPropertyWebkitUserSelect, CSSValueNone);
        } else if (equalLettersIgnoringASCIICase(value, "false"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyWebkitUserDrag, CSSValueNone);
    } else if (name == dirAttr) {
        if (equalLettersIgnoringASCIICase(value, "auto"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyUnicodeBidi, unicodeBidiAttributeForDirAuto(*this));
        else if (equalLettersIgnoringASCIICase(value, "rtl"_s) || equalLettersIgnoringASCIICase(value, "ltr"_s)) {
            addPropertyToPresentationalHintStyle(style, CSSPropertyDirection, value);
            if (!hasTagName(bdiTag) && !hasTagName(bdoTag) && !hasTagName(outputTag))
                addPropertyToPresentationalHintStyle(style, CSSPropertyUnicodeBidi, CSSValueIsolate);
        }
    } else if (name.matches(XMLNames::langAttr))
        mapLanguageAttributeToLocale(value, style);
    else if (name == langAttr) {
        // xml:lang has a higher priority than lang.
        if (!hasAttributeWithoutSynchronization(XMLNames::langAttr))
            mapLanguageAttributeToLocale(value, style);
    } else
        StyledElement::collectPresentationalHintsForAttribute(name, value, style);
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
    static NeverDestroyed map = [] {
        EventHandlerNameMap map;
        JSHTMLElement::forEachEventHandlerContentAttribute([&] (const AtomString& attributeName, const AtomString& eventName) {
            // FIXME: Remove this special case. This has an [EventHandler] line in the IDL but was not historically in this map.
            if (attributeName == oncuechangeAttr.get().localName())
                return;
            map.add(attributeName.impl(), eventName);
        });
        // FIXME: Remove these special cases. These are not in IDL with [EventHandler] but were historically in this map.
        static constexpr std::array table {
            &onautocompleteAttr,
            &onautocompleteerrorAttr,
            &onbeforeloadAttr,
            &onfocusinAttr,
            &onfocusoutAttr,
            &ongesturechangeAttr,
            &ongestureendAttr,
            &ongesturestartAttr,
            &onwebkitbeginfullscreenAttr,
            &onwebkitcurrentplaybacktargetiswirelesschangedAttr,
            &onwebkitendfullscreenAttr,
            &onwebkitfullscreenchangeAttr,
            &onwebkitfullscreenerrorAttr,
            &onwebkitkeyaddedAttr,
            &onwebkitkeyerrorAttr,
            &onwebkitkeymessageAttr,
            &onwebkitneedkeyAttr,
            &onwebkitplaybacktargetavailabilitychangedAttr,
            &onwebkitpresentationmodechangedAttr,
        };
        for (auto& entry : table) {
            auto* name = entry->get().localName().impl();
            map.add(name, AtomString { name, 2, String::MaxLength });
        }
        return map;
    }();
    return eventNameForEventHandlerAttribute(attributeName, map);
}

Node::Editability HTMLElement::editabilityFromContentEditableAttr(const Node& node, PageIsEditable pageIsEditable)
{
    if (pageIsEditable == PageIsEditable::Yes)
        return Editability::CanEditRichly;

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

    RefPtr containingShadowRoot { node.containingShadowRoot() };
    if (containingShadowRoot && containingShadowRoot->mode() == ShadowRootMode::UserAgent)
        return Editability::ReadOnly;

    if (node.document().inDesignMode())
        return Editability::CanEditRichly;

    return Editability::ReadOnly;
}

bool HTMLElement::matchesReadWritePseudoClass() const
{
    return editabilityFromContentEditableAttr(*this, PageIsEditable::No) != Editability::ReadOnly;
}

void HTMLElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == dirAttr) {
        dirAttributeChanged(value);
        return;
    }

    if (name == tabindexAttr) {
        if (auto optionalTabIndex = parseHTMLInteger(value))
            setTabIndexExplicitly(optionalTabIndex.value());
        else
            setTabIndexExplicitly(std::nullopt);
        return;
    }

    if (document().settings().inertAttributeEnabled() && name == inertAttr)
        invalidateStyleInternal();

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

Node::InsertedIntoAncestorResult HTMLElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    auto result = StyledElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    hideNonce();

    if (RefPtr parent = parentOrShadowHostElement(); parent && parent != document().documentElement() && UNLIKELY(parent->usesEffectiveTextDirection())) {
        if (!elementAffectsDirectionality(*this) && !(is<HTMLInputElement>(*this) && downcast<HTMLInputElement>(*this).isTelephoneField())) {
            setUsesEffectiveTextDirection(true);
            setEffectiveTextDirection(parent->effectiveTextDirection());
        }
    }

    return result;
}

void HTMLElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    StyledElement::removedFromAncestor(removalType, oldParentOfRemovedTree);

    if (UNLIKELY(usesEffectiveTextDirection()) && !isValidDirValue(attributeWithoutSynchronization(dirAttr))) {
        if (auto* parent = parentOrShadowHostElement(); !(parent && parent->usesEffectiveTextDirection()))
            setUsesEffectiveTextDirection(false);
    }
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
    static MainThreadNeverDestroyed<const AtomString> ltrValue("ltr"_s);
    static MainThreadNeverDestroyed<const AtomString> rtlValue("rtl"_s);
    if (equalLettersIgnoringASCIICase(value, "ltr"_s))
        return ltrValue;
    if (equalLettersIgnoringASCIICase(value, "rtl"_s))
        return rtlValue;
    if (equalLettersIgnoringASCIICase(value, "auto"_s))
        return autoAtom();
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

ExceptionOr<void> HTMLElement::setInnerText(String&& text)
{
    // FIXME: This doesn't take whitespace collapsing into account at all.

    if (!text.contains([](UChar c) { return c == '\n' || c == '\r'; })) {
        stringReplaceAll(WTFMove(text));
        return { };
    }

    if (isConnected() && isTextControlInnerTextElement()) {
        if (!text.contains('\r')) {
            stringReplaceAll(WTFMove(text));
            return { };
        }
        String textWithConsistentLineBreaks = makeStringBySimplifyingNewLines(text);
        stringReplaceAll(WTFMove(textWithConsistentLineBreaks));
        return { };
    }

    // FIXME: This should use replaceAll(), after we fix that to work properly for DocumentFragment.
    // Add text nodes and <br> elements.
    auto fragment = textToFragment(document(), WTFMove(text));
    // It's safe to dispatch events on the new fragment since author scripts have no access to it yet.
    ScriptDisallowedScope::EventAllowedScope allowedScope(fragment.get());
    return replaceChildrenWithFragment(*this, WTFMove(fragment));
}

ExceptionOr<void> HTMLElement::setOuterText(String&& text)
{
    RefPtr<ContainerNode> parent = parentNode();
    if (!parent)
        return Exception { NoModificationAllowedError };

    RefPtr<Node> prev = previousSibling();
    RefPtr<Node> next = nextSibling();
    RefPtr<Node> newChild;

    // Convert text to fragment with <br> tags instead of linebreaks if needed.
    if (text.contains([](UChar c) { return c == '\n' || c == '\r'; }))
        newChild = textToFragment(document(), WTFMove(text));
    else
        newChild = Text::create(document(), WTFMove(text));

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

void HTMLElement::applyAspectRatioFromWidthAndHeightAttributesToStyle(StringView widthAttribute, StringView heightAttribute, MutableStyleProperties& style)
{
    if (!document().settings().aspectRatioOfImgFromWidthAndHeightEnabled())
        return;

    auto dimensionWidth = parseHTMLDimension(widthAttribute);
    if (!dimensionWidth || dimensionWidth->type != HTMLDimension::Type::Pixel)
        return;
    auto dimensionHeight = parseHTMLDimension(heightAttribute);
    if (!dimensionHeight || dimensionHeight->type != HTMLDimension::Type::Pixel)
        return;

    addParsedWidthAndHeightToAspectRatioList(dimensionWidth->number, dimensionHeight->number, style);
}

void HTMLElement::applyAspectRatioWithoutDimensionalRulesFromWidthAndHeightAttributesToStyle(StringView widthAttribute, StringView heightAttribute, MutableStyleProperties& style)
{
    if (!document().settings().aspectRatioOfImgFromWidthAndHeightEnabled())
        return;

    auto dimensionWidth = parseHTMLNonNegativeInteger(widthAttribute);
    if (!dimensionWidth)
        return;
    auto dimensionHeight = parseHTMLNonNegativeInteger(heightAttribute);
    if (!dimensionHeight)
        return;

    addParsedWidthAndHeightToAspectRatioList(dimensionWidth.value(), dimensionHeight.value(), style);
}

void HTMLElement::addParsedWidthAndHeightToAspectRatioList(double width, double height, MutableStyleProperties& style)
{
    auto ratioList = CSSValueList::createSlashSeparated();
    ratioList->append(CSSValuePool::singleton().createValue(width, CSSUnitType::CSS_NUMBER));
    ratioList->append(CSSValuePool::singleton().createValue(height, CSSUnitType::CSS_NUMBER));
    auto list = CSSValueList::createSpaceSeparated();
    list->append(CSSValuePool::singleton().createIdentifierValue(CSSValueAuto));
    list->append(ratioList);

    style.setProperty(CSSPropertyAspectRatio, RefPtr<CSSValue>(WTFMove(list)));
}

void HTMLElement::applyAlignmentAttributeToStyle(const AtomString& alignment, MutableStyleProperties& style)
{
    // Vertical alignment with respect to the current baseline of the text
    // right or left means floating images.
    CSSValueID floatValue = CSSValueInvalid;
    CSSValueID verticalAlignValue = CSSValueInvalid;

    if (equalLettersIgnoringASCIICase(alignment, "absmiddle"_s))
        verticalAlignValue = CSSValueMiddle;
    else if (equalLettersIgnoringASCIICase(alignment, "absbottom"_s))
        verticalAlignValue = CSSValueBottom;
    else if (equalLettersIgnoringASCIICase(alignment, "left"_s)) {
        floatValue = CSSValueLeft;
        verticalAlignValue = CSSValueTop;
    } else if (equalLettersIgnoringASCIICase(alignment, "right"_s)) {
        floatValue = CSSValueRight;
        verticalAlignValue = CSSValueTop;
    } else if (equalLettersIgnoringASCIICase(alignment, "top"_s))
        verticalAlignValue = CSSValueTop;
    else if (equalLettersIgnoringASCIICase(alignment, "middle"_s))
        verticalAlignValue = CSSValueWebkitBaselineMiddle;
    else if (equalLettersIgnoringASCIICase(alignment, "center"_s))
        verticalAlignValue = CSSValueMiddle;
    else if (equalLettersIgnoringASCIICase(alignment, "bottom"_s))
        verticalAlignValue = CSSValueBaseline;
    else if (equalLettersIgnoringASCIICase(alignment, "texttop"_s))
        verticalAlignValue = CSSValueTextTop;

    if (floatValue != CSSValueInvalid)
        addPropertyToPresentationalHintStyle(style, CSSPropertyFloat, floatValue);

    if (verticalAlignValue != CSSValueInvalid)
        addPropertyToPresentationalHintStyle(style, CSSPropertyVerticalAlign, verticalAlignValue);
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
        return trueAtom();
    case ContentEditableType::False:
        return falseAtom();
    case ContentEditableType::PlaintextOnly:
        return plaintextOnlyAtom();
    }
    return "inherit"_s;
}

ExceptionOr<void> HTMLElement::setContentEditable(const String& enabled)
{
    if (equalLettersIgnoringASCIICase(enabled, "true"_s))
        setAttributeWithoutSynchronization(contenteditableAttr, trueAtom());
    else if (equalLettersIgnoringASCIICase(enabled, "false"_s))
        setAttributeWithoutSynchronization(contenteditableAttr, falseAtom());
    else if (equalLettersIgnoringASCIICase(enabled, "plaintext-only"_s))
        setAttributeWithoutSynchronization(contenteditableAttr, plaintextOnlyAtom());
    else if (equalLettersIgnoringASCIICase(enabled, "inherit"_s))
        removeAttribute(contenteditableAttr);
    else
        return Exception { SyntaxError };
    return { };
}

bool HTMLElement::draggable() const
{
    auto& value = attributeWithoutSynchronization(draggableAttr);
    if (isDraggableIgnoringAttributes())
        return !equalLettersIgnoringASCIICase(value, "false"_s);

    return equalLettersIgnoringASCIICase(value, "true"_s);
}

void HTMLElement::setDraggable(bool value)
{
    setAttributeWithoutSynchronization(draggableAttr, value ? trueAtom() : falseAtom());
}

bool HTMLElement::spellcheck() const
{
    return isSpellCheckingEnabled();
}

void HTMLElement::setSpellcheck(bool enable)
{
    setAttributeWithoutSynchronization(spellcheckAttr, enable ? trueAtom() : falseAtom());
}

void HTMLElement::click()
{
    simulateClick(*this, nullptr, SendNoEvents, DoNotShowPressedLook, SimulatedClickSource::Bindings);
}

bool HTMLElement::accessKeyAction(bool sendMouseEvents)
{
    if (isFocusable())
        focus();
    return dispatchSimulatedClick(nullptr, sendMouseEvents ? SendMouseUpDownEvents : SendNoEvents);
}

String HTMLElement::accessKeyLabel() const
{
    const auto& accessKey = attributeWithoutSynchronization(accesskeyAttr);
    if (accessKey.isEmpty())
        return String();

    StringBuilder result;

#if PLATFORM(COCOA)
    auto modifiers = EventHandler::accessKeyModifiers();
    if (modifiers.contains(PlatformEvent::Modifier::ControlKey))
        result.append(upArrowhead);
    if (modifiers.contains(PlatformEvent::Modifier::AltKey))
        result.append(WTF::Unicode::optionKey);
#else
    // Currently accessKeyModifier in non-cocoa platforms is hardcoded to Alt, so no reason to do extra work here.
    // If this ever becomes configurable, make this code use EventHandler::accessKeyModifiers().
    result.append("Alt+");
#endif

    result.append(accessKey);
    return result.toString();
}

String HTMLElement::title() const
{
    return attributeWithoutSynchronization(titleAttr);
}

bool HTMLElement::translate() const
{
    for (auto& element : lineageOfType<HTMLElement>(*this)) {
        const AtomString& value = element.attributeWithoutSynchronization(translateAttr);
        if (equalLettersIgnoringASCIICase(value, "yes"_s) || (value.isEmpty() && !value.isNull()))
            return true;
        if (equalLettersIgnoringASCIICase(value, "no"_s))
            return false;
    }
    // Default on the root element is translate=yes.
    return true;
}

void HTMLElement::setTranslate(bool enable)
{
    setAttributeWithoutSynchronization(translateAttr, enable ? "yes"_s : "no"_s);
}

bool HTMLElement::rendererIsEverNeeded()
{
    if (hasTagName(noscriptTag)) {
        RefPtr<Frame> frame = document().frame();
        if (frame && frame->script().canExecuteScripts(NotAboutToExecuteScript))
            return false;
    } else if (hasTagName(noembedTag)) {
        RefPtr<Frame> frame = document().frame();
        if (frame && frame->arePluginsEnabled())
            return false;
    }
    return StyledElement::rendererIsEverNeeded();
}

HTMLFormElement* HTMLElement::form() const
{
    return HTMLFormElement::findClosestFormAncestor(*this);
}

FormAssociatedElement* HTMLElement::asFormAssociatedElement()
{
    return nullptr;
}

FormListedElement* HTMLElement::asFormListedElement()
{
    return nullptr;
}

static void setHasDirAutoFlagRecursively(Node* firstNode, bool flag, Node* lastNode = nullptr)
{
    firstNode->setSelfOrPrecedingNodesAffectDirAuto(flag);

    RefPtr<Node> node = firstNode->firstChild();

    while (node) {
        if (elementAffectsDirectionality(*node)) {
            if (node == lastNode)
                return;
            node = NodeTraversal::nextSkippingChildren(*node, firstNode);
            continue;
        }
        node->setSelfOrPrecedingNodesAffectDirAuto(flag);
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
    return (hasTagName(bdiTag) && !isValidDirValue(direction)) || equalLettersIgnoringASCIICase(direction, "auto"_s);
}

std::optional<TextDirection> HTMLElement::directionalityIfDirIsAuto() const
{
    if (!(selfOrPrecedingNodesAffectDirAuto() && hasDirectionAuto()))
        return std::nullopt;
    return computeDirectionalityFromText().direction;
}

auto HTMLElement::computeDirectionalityFromText() const -> TextDirectionWithStrongDirectionalityNode
{
    if (auto* textControl = dynamicDowncast<HTMLTextFormControlElement>(const_cast<HTMLElement*>(this))) {
        auto* inputElement = dynamicDowncast<HTMLInputElement>(textControl);
        if (!inputElement || (inputElement->isTextType() && !inputElement->isPasswordField())) {
            auto direction = textControl->value().defaultWritingDirection();
            if (!direction)
                return { TextDirection::LTR, nullptr };
            return { *direction == U_LEFT_TO_RIGHT ? TextDirection::LTR : TextDirection::RTL, textControl };
        }
    }

    RefPtr<Node> node = firstChild();
    while (node) {
        // Skip bdi, script, style and text form controls.
        if (node->hasTagName(bdiTag) || node->hasTagName(scriptTag) || node->hasTagName(styleTag)
            || (is<Element>(*node) && downcast<Element>(*node).isTextField())) {
            node = NodeTraversal::nextSkippingChildren(*node, this);
            continue;
        }

        // Skip elements with valid dir attribute
        if (is<Element>(*node)) {
            if (isValidDirValue(downcast<Element>(*node).attributeWithoutSynchronization(dirAttr))) {
                node = NodeTraversal::nextSkippingChildren(*node, this);
                continue;
            }
        }

        if (node->isTextNode()) {
            if (auto direction = node->textContent(true).defaultWritingDirection())
                return { *direction == U_LEFT_TO_RIGHT ? TextDirection::LTR : TextDirection::RTL, node };
        }
        node = NodeTraversal::next(*node, this);
    }
    return { TextDirection::LTR, nullptr };
}

void HTMLElement::dirAttributeChanged(const AtomString& value)
{
    RefPtr<Element> parent = parentOrShadowHostElement();
    bool isValid = true;

    switch (parseTextDirection(value)) {
    case TextDirectionDirective::Invalid:
        isValid = false;
        if (selfOrPrecedingNodesAffectDirAuto() && (!parent || !parent->selfOrPrecedingNodesAffectDirAuto()) && !is<HTMLBDIElement>(*this))
            setHasDirAutoFlagRecursively(this, false);
        if (parent && parent->usesEffectiveTextDirection() && !(is<HTMLInputElement>(*this) && downcast<HTMLInputElement>(*this).isTelephoneField())) {
            setUsesEffectiveTextDirection(true);
            updateEffectiveDirectionality(parent->effectiveTextDirection());
        } else
            setUsesEffectiveTextDirection(false);
        break;
    case TextDirectionDirective::LTR:
        if (selfOrPrecedingNodesAffectDirAuto())
            setHasDirAutoFlagRecursively(this, false);
        setUsesEffectiveTextDirection(true);
        updateEffectiveDirectionality(TextDirection::LTR);
        break;
    case TextDirectionDirective::RTL:
        if (selfOrPrecedingNodesAffectDirAuto())
            setHasDirAutoFlagRecursively(this, false);
        setUsesEffectiveTextDirection(true);
        updateEffectiveDirectionality(TextDirection::RTL);
        break;
    case TextDirectionDirective::Auto:
        setUsesEffectiveTextDirection(true);
        updateEffectiveDirectionalityOfDirAuto();
        break;
    }

    if (is<HTMLElement>(parent) && parent->selfOrPrecedingNodesAffectDirAuto()) {
        if (isValid)
            setHasDirAutoFlagRecursively(this, false);
        downcast<HTMLElement>(*parent).adjustDirectionalityIfNeededAfterChildAttributeChanged(this);
    }
}

void HTMLElement::updateEffectiveDirectionality(TextDirection direction)
{
    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClassDir, Style::PseudoClassChangeInvalidation::AnyValue);
    setUsesEffectiveTextDirection(true);
    setEffectiveTextDirection(direction);
    auto updateEffectiveTextDirectionOfShadowRoot = [&](HTMLElement& element) {
        if (RefPtr shadowRootOfElement = element.shadowRoot()) {
            for (auto& element : childrenOfType<HTMLElement>(*shadowRootOfElement))
                element.updateEffectiveDirectionality(direction);
        }
    };
    updateEffectiveTextDirectionOfShadowRoot(*this);
    for (auto it = descendantsOfType<HTMLElement>(*this).begin(); it;) {
        auto& element = *it;
        if (isValidDirValue(element.attributeWithoutSynchronization(dirAttr))) {
            it.traverseNextSkippingChildren();
            continue;
        }
        updateEffectiveTextDirectionOfShadowRoot(element);
        Style::PseudoClassChangeInvalidation styleInvalidation(element, CSSSelector::PseudoClassDir, Style::PseudoClassChangeInvalidation::AnyValue);
        element.setUsesEffectiveTextDirection(true);
        element.setEffectiveTextDirection(direction);
        it.traverseNext();
    }
}

void HTMLElement::adjustDirectionalityIfNeededAfterChildAttributeChanged(Element*)
{
    ASSERT(selfOrPrecedingNodesAffectDirAuto());
    for (auto& element : lineageOfType<HTMLElement>(*this)) {
        if (elementAffectsDirectionality(element)) {
            ASSERT(element.hasDirectionAuto());
            element.updateEffectiveDirectionalityOfDirAuto();
            break;
        }
    }
}

void HTMLElement::updateEffectiveDirectionalityOfDirAuto()
{
    auto result = computeDirectionalityFromText();
    setHasDirAutoFlagRecursively(this, true, result.strongDirectionalityNode.get());
    updateEffectiveDirectionality(result.direction);
    if (renderer() && renderer()->style().direction() != result.direction)
        invalidateStyleForSubtree();
}

void HTMLElement::adjustDirectionalityIfNeededAfterChildrenChanged(Element* beforeChange, ChildChange::Type changeType)
{
    // FIXME: This function looks suspicious.

    if (!selfOrPrecedingNodesAffectDirAuto())
        return;

    RefPtr<Node> oldMarkedNode;
    if (beforeChange)
        oldMarkedNode = changeType == ChildChange::Type::ElementInserted ? ElementTraversal::nextSibling(*beforeChange) : beforeChange->nextSibling();

    while (oldMarkedNode && elementAffectsDirectionality(*oldMarkedNode))
        oldMarkedNode = oldMarkedNode->nextSibling();
    if (oldMarkedNode)
        setHasDirAutoFlagRecursively(oldMarkedNode.get(), false);

    for (auto& elementToAdjust : lineageOfType<HTMLElement>(*this)) {
        if (elementAffectsDirectionality(elementToAdjust)) {
            ASSERT(elementToAdjust.hasDirectionAuto());
            elementToAdjust.updateEffectiveDirectionalityOfDirAuto();
            return;
        }
    }
}

void HTMLElement::addHTMLLengthToStyle(MutableStyleProperties& style, CSSPropertyID propertyID, StringView value, AllowPercentage allowPercentage, UseCSSPXAsUnitType useCSSPX, IsMultiLength isMultiLength, AllowZeroValue allowZeroValue)
{
    auto dimensionValue = isMultiLength == IsMultiLength::No ? parseHTMLDimension(value) : parseHTMLMultiLength(value);
    if (!dimensionValue || (!dimensionValue->number && allowZeroValue == AllowZeroValue::No))
        return;
    if (dimensionValue->type == HTMLDimension::Type::Percentage) {
        if (allowPercentage == AllowPercentage::Yes)
            addPropertyToPresentationalHintStyle(style, propertyID, dimensionValue->number, CSSUnitType::CSS_PERCENTAGE);
        return;
    }
    if (useCSSPX == UseCSSPXAsUnitType::Yes)
        addPropertyToPresentationalHintStyle(style, propertyID, dimensionValue->number, CSSUnitType::CSS_PX);
    else
        addPropertyToPresentationalHintStyle(style, propertyID, dimensionValue->number, CSSUnitType::CSS_NUMBER);
}

// https://www.w3.org/TR/html4/sgml/dtd.html#Length, including pixel and percentage values.
void HTMLElement::addHTMLLengthToStyle(MutableStyleProperties& style, CSSPropertyID propertyID, StringView value, AllowZeroValue allowZeroValue)
{
    addHTMLLengthToStyle(style, propertyID, value, AllowPercentage::Yes, UseCSSPXAsUnitType::Yes, IsMultiLength::No, allowZeroValue);
}

// https://www.w3.org/TR/html4/sgml/dtd.html#MultiLength, including pixel, percentage, and relative values.
void HTMLElement::addHTMLMultiLengthToStyle(MutableStyleProperties& style, CSSPropertyID propertyID, StringView value)
{
    addHTMLLengthToStyle(style, propertyID, value, AllowPercentage::Yes, UseCSSPXAsUnitType::Yes, IsMultiLength::Yes);
}

// https://www.w3.org/TR/html4/sgml/dtd.html#Pixels, including pixel value.
void HTMLElement::addHTMLPixelsToStyle(MutableStyleProperties& style, CSSPropertyID propertyID, StringView value)
{
    addHTMLLengthToStyle(style, propertyID, value, AllowPercentage::No, UseCSSPXAsUnitType::Yes, IsMultiLength::No);
}

// This is specific to <marquee> attributes, including pixel and CSS_NUMBER values.
void HTMLElement::addHTMLNumberToStyle(MutableStyleProperties& style, CSSPropertyID propertyID, StringView value)
{
    addHTMLLengthToStyle(style, propertyID, value, AllowPercentage::Yes, UseCSSPXAsUnitType::No, IsMultiLength::No);
}

// Color parsing that matches HTML's "rules for parsing a legacy color value"
// https://html.spec.whatwg.org/#rules-for-parsing-a-legacy-colour-value
std::optional<SRGBA<uint8_t>> HTMLElement::parseLegacyColorValue(StringView string)
{
    // An empty string doesn't apply a color.
    if (string.isEmpty())
        return std::nullopt;

    string = string.stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>);
    if (string.isEmpty())
        return Color::black;

    // "transparent" doesn't apply a color either.
    if (equalLettersIgnoringASCIICase(string, "transparent"_s))
        return std::nullopt;

    if (auto namedColor = CSSParser::parseNamedColor(string))
        return namedColor;

    if (string.length() == 4 && string[0] == '#' && isASCIIHexDigit(string[1]) && isASCIIHexDigit(string[2]) && isASCIIHexDigit(string[3]))
        return { { static_cast<uint8_t>(toASCIIHexValue(string[1]) * 0x11), static_cast<uint8_t>(toASCIIHexValue(string[2]) * 0x11), static_cast<uint8_t>(toASCIIHexValue(string[3]) * 0x11) } };

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
        return { { toASCIIHexValue(digitBuffer[0]), toASCIIHexValue(digitBuffer[1]), toASCIIHexValue(digitBuffer[2]) } };

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

    uint8_t redValue = toASCIIHexValue(digitBuffer[redIndex], digitBuffer[redIndex + 1]);
    uint8_t greenValue = toASCIIHexValue(digitBuffer[greenIndex], digitBuffer[greenIndex + 1]);
    uint8_t blueValue = toASCIIHexValue(digitBuffer[blueIndex], digitBuffer[blueIndex + 1]);
    return { { redValue, greenValue, blueValue } };
}

void HTMLElement::addHTMLColorToStyle(MutableStyleProperties& style, CSSPropertyID propertyID, const AtomString& attributeValue)
{
    if (auto color = parseLegacyColorValue(attributeValue))
        style.setProperty(propertyID, CSSValuePool::singleton().createColorValue(*color));
}

bool HTMLElement::willRespondToMouseMoveEvents() const
{
    return !isDisabledFormControl() && Element::willRespondToMouseMoveEvents();
}

bool HTMLElement::willRespondToMouseWheelEvents() const
{
    return !isDisabledFormControl() && Element::willRespondToMouseWheelEvents();
}

bool HTMLElement::willRespondToMouseClickEventsWithEditability(Editability editability) const
{
    return !isDisabledFormControl() && Element::willRespondToMouseClickEventsWithEditability(editability);
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
    return !equalLettersIgnoringASCIICase(autocorrectValue, "off"_s);
}

void HTMLElement::setAutocorrect(bool autocorrect)
{
    setAttributeWithoutSynchronization(autocorrectAttr, autocorrect ? onAtom() : offAtom());
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

void HTMLElement::setEnterKeyHint(const AtomString& value)
{
    setAttributeWithoutSynchronization(enterkeyhintAttr, value);
}

bool HTMLElement::shouldExtendSelectionToTargetNode(const Node& targetNode, const VisibleSelection& selectionBeforeUpdate)
{
    if (auto range = selectionBeforeUpdate.range(); range && ImageOverlay::isInsideOverlay(*range))
        return ImageOverlay::isOverlayText(targetNode);

    return true;
}

ExceptionOr<Ref<ElementInternals>> HTMLElement::attachInternals()
{
    auto* queue = reactionQueue();
    if (!queue)
        return Exception { NotSupportedError, "attachInternals is only supported on a custom element instance"_s };

    if (queue->isElementInternalsDisabled())
        return Exception { NotSupportedError, "attachInternals is disabled"_s };

    if (queue->isElementInternalsAttached())
        return Exception { NotSupportedError, "There is already an existing ElementInternals"_s };

    if (!isPrecustomizedOrDefinedCustomElement())
        return Exception { NotSupportedError, "Custom element is in an invalid state"_s };

    queue->setElementInternalsAttached();
    return ElementInternals::create(*this);
}

#if PLATFORM(IOS_FAMILY)

SelectionRenderingBehavior HTMLElement::selectionRenderingBehavior(const Node* node)
{
    return ImageOverlay::isOverlayText(node) ? SelectionRenderingBehavior::UseIndividualQuads : SelectionRenderingBehavior::CoalesceBoundingRects;
}

#endif // PLATFORM(IOS_FAMILY)

} // namespace WebCore

#ifndef NDEBUG

// For use in the debugger
void dumpInnerHTML(WebCore::HTMLElement*);

void dumpInnerHTML(WebCore::HTMLElement* element)
{
    printf("%s\n", element->innerHTML().ascii().data());
}

#endif
