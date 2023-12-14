/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2021 Google Inc. All rights reserved.
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
#include "CSSParser.h"
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
#include "Editor.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementInternals.h"
#include "ElementRareData.h"
#include "EnterKeyHint.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FrameLoader.h"
#include "HTMLBDIElement.h"
#include "HTMLBRElement.h"
#include "HTMLButtonElement.h"
#include "HTMLDialogElement.h"
#include "HTMLDocument.h"
#include "HTMLElementFactory.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLMaybeFormAssociatedCustomElement.h"
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
#include "LabelsNodeList.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "MediaControlsHost.h"
#include "MutableStyleProperties.h"
#include "NodeName.h"
#include "NodeTraversal.h"
#include "PopoverData.h"
#include "PseudoClassChangeInvalidation.h"
#include "RenderElement.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "SimulatedClick.h"
#include "StyleProperties.h"
#include "Text.h"
#include "ToggleEvent.h"
#include "TypedElementDescendantIteratorInlines.h"
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
            return tagQName().localNameUppercase();
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
    switch (name.nodeName()) {
    case AttributeNames::alignAttr:
    case AttributeNames::contenteditableAttr:
    case AttributeNames::hiddenAttr:
    case AttributeNames::langAttr:
    case AttributeNames::XML::langAttr:
    case AttributeNames::draggableAttr:
    case AttributeNames::dirAttr:
        return true;
    default:
        break;
    }
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
    auto* htmlElement = dynamicDowncast<HTMLElement>(node);
    return htmlElement && elementAffectsDirectionality(*htmlElement);
}

void HTMLElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    switch (name.nodeName()) {
    case AttributeNames::alignAttr:
        if (equalLettersIgnoringASCIICase(value, "middle"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyTextAlign, CSSValueCenter);
        else
            addPropertyToPresentationalHintStyle(style, CSSPropertyTextAlign, value);
        break;
    case AttributeNames::contenteditableAttr: {
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
        break;
    }
    case AttributeNames::hiddenAttr:
        addPropertyToPresentationalHintStyle(style, CSSPropertyDisplay, CSSValueNone);
        break;
    case AttributeNames::draggableAttr:
        if (equalLettersIgnoringASCIICase(value, "true"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyWebkitUserDrag, CSSValueElement);
        else if (equalLettersIgnoringASCIICase(value, "false"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyWebkitUserDrag, CSSValueNone);
        break;
    case AttributeNames::dirAttr:
        if (equalLettersIgnoringASCIICase(value, "auto"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyUnicodeBidi, unicodeBidiAttributeForDirAuto(*this));
        else if (equalLettersIgnoringASCIICase(value, "rtl"_s) || equalLettersIgnoringASCIICase(value, "ltr"_s)) {
            addPropertyToPresentationalHintStyle(style, CSSPropertyDirection, value);
            if (!hasTagName(bdiTag) && !hasTagName(bdoTag) && !hasTagName(outputTag))
                addPropertyToPresentationalHintStyle(style, CSSPropertyUnicodeBidi, CSSValueIsolate);
        }
        break;
    case AttributeNames::XML::langAttr:
        mapLanguageAttributeToLocale(value, style);
        break;
    case AttributeNames::langAttr:
        // xml:lang has a higher priority than lang.
        if (!hasAttributeWithoutSynchronization(XMLNames::langAttr))
            mapLanguageAttributeToLocale(value, style);
        break;
    default:
        StyledElement::collectPresentationalHintsForAttribute(name, value, style);
        break;
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
    static NeverDestroyed map = [] {
        EventHandlerNameMap map;
        JSHTMLElement::forEachEventHandlerContentAttribute([&] (const AtomString& attributeName, const AtomString& eventName) {
            // FIXME: Remove this special case. This has an [EventHandler] line in the IDL but was not historically in this map.
            if (attributeName == oncuechangeAttr.get().localName())
                return;
            map.add(attributeName, eventName);
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
            auto& name = entry->get().localName();
            map.add(name, AtomString { name.impl(), 2, String::MaxLength });
        }
        return map;
    }();
    return eventNameForEventHandlerAttribute(attributeName, map);
}

Node::Editability HTMLElement::editabilityFromContentEditableAttr(const Node& node, PageIsEditable pageIsEditable)
{
    if (pageIsEditable == PageIsEditable::Yes)
        return Editability::CanEditRichly;

    auto* startElement = dynamicDowncast<Element>(node);
    if (!startElement)
        startElement = node.parentElement();
    if (startElement) {
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

    if (node.document().inDesignMode() && node.isInDocumentTree())
        return Editability::CanEditRichly;

    return Editability::ReadOnly;
}

bool HTMLElement::matchesReadWritePseudoClass() const
{
    return editabilityFromContentEditableAttr(*this, PageIsEditable::No) != Editability::ReadOnly;
}

void HTMLElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    StyledElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);

    switch (name.nodeName()) {
    case AttributeNames::dirAttr:
        dirAttributeChanged(newValue);
        return;
    case AttributeNames::tabindexAttr:
        if (auto optionalTabIndex = parseHTMLInteger(newValue))
            setTabIndexExplicitly(optionalTabIndex.value());
        else
            setTabIndexExplicitly(std::nullopt);
        return;
    case AttributeNames::inertAttr:
        invalidateStyleInternal();
        return;
    case AttributeNames::inputmodeAttr:
        if (Ref document = this->document(); this == document->focusedElement()) {
            if (RefPtr page = document->page())
                page->chrome().client().focusedElementDidChangeInputMode(*this, canonicalInputMode());
        }
        return;
    case AttributeNames::popoverAttr:
        if (document().settings().popoverAttributeEnabled() && !document().quirks().shouldDisablePopoverAttributeQuirk())
            popoverAttributeChanged(newValue);
        return;
    case AttributeNames::spellcheckAttr: {
        if (!document().hasEverHadSelectionInsideTextFormControl())
            return;

        bool oldEffectiveAttributeValue = !equalLettersIgnoringASCIICase(oldValue, "false"_s);
        bool newEffectiveAttributeValue = !equalLettersIgnoringASCIICase(newValue, "false"_s);

        if (oldEffectiveAttributeValue == newEffectiveAttributeValue)
            return;

        effectiveSpellcheckAttributeChanged(newEffectiveAttributeValue);
        return;
    }
    default:
        break;
    }

    auto& eventName = eventNameForEventHandlerAttribute(name);
    if (!eventName.isNull())
        setAttributeEventListener(eventName, name, newValue);
}

Node::InsertedIntoAncestorResult HTMLElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    auto result = StyledElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    hideNonce();

    if (RefPtr parent = parentOrShadowHostElement(); parent && UNLIKELY(parent->usesEffectiveTextDirection())) {
        auto* input = dynamicDowncast<HTMLInputElement>(*this);
        if (!elementAffectsDirectionality(*this) && !(input && input->isTelephoneField())) {
            setUsesEffectiveTextDirection(true);
            setEffectiveTextDirection(parent->effectiveTextDirection());
        }
    }

    return result;
}

void HTMLElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    if (popoverData())
        hidePopoverInternal(FocusPreviousElement::No, FireEvents::No);

    StyledElement::removedFromAncestor(removalType, oldParentOfRemovedTree);

    if (UNLIKELY(usesEffectiveTextDirection()) && !isValidDirValue(attributeWithoutSynchronization(dirAttr))) {
        if (auto* parent = parentOrShadowHostElement(); !(parent && parent->usesEffectiveTextDirection()))
            setUsesEffectiveTextDirection(false);
    }
}

static Ref<DocumentFragment> textToFragment(Document& document, const String& text)
{
    Ref fragment = DocumentFragment::create(document);

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
    Ref fragment = textToFragment(document(), WTFMove(text));
    // It's safe to dispatch events on the new fragment since author scripts have no access to it yet.
    ScriptDisallowedScope::EventAllowedScope allowedScope(fragment.get());
    return replaceChildrenWithFragment(*this, WTFMove(fragment));
}

ExceptionOr<void> HTMLElement::setOuterText(String&& text)
{
    RefPtr parent = parentNode();
    if (!parent)
        return Exception { ExceptionCode::NoModificationAllowedError };

    RefPtr prev = previousSibling();
    RefPtr next = nextSibling();
    RefPtr<Node> newChild;

    // Convert text to fragment with <br> tags instead of linebreaks if needed.
    if (text.contains([](UChar c) { return c == '\n' || c == '\r'; }))
        newChild = textToFragment(document(), WTFMove(text));
    else
        newChild = Text::create(document(), WTFMove(text));

    if (!parentNode())
        return Exception { ExceptionCode::HierarchyRequestError };

    auto replaceResult = parent->replaceChild(*newChild, *this);
    if (replaceResult.hasException())
        return replaceResult.releaseException();

    if (RefPtr node = dynamicDowncast<Text>(next ? next->previousSibling() : nullptr)) {
        auto result = mergeWithNextTextNode(*node);
        if (result.hasException())
            return result.releaseException();
    }
    if (RefPtr previousText = dynamicDowncast<Text>(WTFMove(prev))) {
        auto result = mergeWithNextTextNode(*previousText);
        if (result.hasException())
            return result.releaseException();
    }
    return { };
}

void HTMLElement::applyAspectRatioFromWidthAndHeightAttributesToStyle(StringView widthAttribute, StringView heightAttribute, MutableStyleProperties& style)
{
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
    style.setProperty(CSSPropertyAspectRatio, CSSValueList::createSpaceSeparated(CSSPrimitiveValue::create(CSSValueAuto),
        CSSValueList::createSlashSeparated(CSSPrimitiveValue::create(width), CSSPrimitiveValue::create(height))));
}

void HTMLElement::applyAlignmentAttributeToStyle(const AtomString& alignment, MutableStyleProperties& style)
{
    // Vertical alignment with respect to the current baseline of the text
    // right or left means floating images.
    CSSValueID floatValue = CSSValueInvalid;
    CSSValueID verticalAlignValue = CSSValueInvalid;

    if (equalLettersIgnoringASCIICase(alignment, "absmiddle"_s) || equalLettersIgnoringASCIICase(alignment, "abscenter"_s))
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
        return Exception { ExceptionCode::SyntaxError };
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

void HTMLElement::effectiveSpellcheckAttributeChanged(bool newValue)
{
    for (auto it = descendantsOfType<HTMLElement>(*this).begin(); it;) {
        Ref element = *it;

        auto& value = element->attributeWithoutSynchronization(HTMLNames::spellcheckAttr);
        if (!value.isNull()) {
            it.traverseNextSkippingChildren();
            continue;
        }

        if (element->isTextFormControlElement()) {
            element->effectiveSpellcheckAttributeChanged(newValue);
            it.traverseNextSkippingChildren();
            continue;
        }

        it.traverseNext();
    }
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
    if (hasTagName(noembedTag)) {
        RefPtr frame { document().frame() };
        if (frame && frame->arePluginsEnabled())
            return false;
    }
    return StyledElement::rendererIsEverNeeded();
}

FormAssociatedElement* HTMLElement::asFormAssociatedElement()
{
    return nullptr;
}

static void setHasDirAutoFlagRecursively(Node* firstNode, bool flag, Node* lastNode = nullptr)
{
    firstNode->setSelfOrPrecedingNodesAffectDirAuto(flag);

    RefPtr node = firstNode->firstChild();

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
    if (UNLIKELY(document().isDirAttributeDirty()))
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
    if (RefPtr textControl = dynamicDowncast<HTMLTextFormControlElement>(const_cast<HTMLElement*>(this))) {
        if (textControl->dirAutoUsesValue()) {
            auto direction = textControl->value().defaultWritingDirection();
            if (!direction)
                return { TextDirection::LTR, nullptr };
            return { *direction == U_LEFT_TO_RIGHT ? TextDirection::LTR : TextDirection::RTL, WTFMove(textControl) };
        }
    }

    RefPtr node = firstChild();
    while (node) {
        // Skip bdi, script, style and text form controls.
        auto* element = dynamicDowncast<Element>(*node);
        if (node->hasTagName(bdiTag) || node->hasTagName(scriptTag) || node->hasTagName(styleTag)
            || (element && element->isTextField())) {
            node = NodeTraversal::nextSkippingChildren(*node, this);
            continue;
        }

        // Skip elements with valid dir attribute
        if (element) {
            if (isValidDirValue(element->attributeWithoutSynchronization(dirAttr))) {
                node = NodeTraversal::nextSkippingChildren(*element, this);
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
    RefPtr parent = parentOrShadowHostElement();
    bool isValid = true;

    protectedDocument()->setIsDirAttributeDirty();

    auto direction = parseTextDirection(value);
    switch (direction) {
    case TextDirectionDirective::Invalid:
        isValid = false;
        if (selfOrPrecedingNodesAffectDirAuto() && (!parent || !parent->selfOrPrecedingNodesAffectDirAuto()) && !is<HTMLBDIElement>(*this))
            setHasDirAutoFlagRecursively(this, false);
        if (auto* input = dynamicDowncast<HTMLInputElement>(*this); parent && parent->usesEffectiveTextDirection() && !(input && input->isTelephoneField()))
            updateEffectiveDirectionality(parent->effectiveTextDirection());
        else
            updateEffectiveDirectionality(std::nullopt);
        break;
    case TextDirectionDirective::LTR:
        if (selfOrPrecedingNodesAffectDirAuto())
            setHasDirAutoFlagRecursively(this, false);
        updateEffectiveDirectionality(TextDirection::LTR);
        break;
    case TextDirectionDirective::RTL:
        if (selfOrPrecedingNodesAffectDirAuto())
            setHasDirAutoFlagRecursively(this, false);
        updateEffectiveDirectionality(TextDirection::RTL);
        break;
    case TextDirectionDirective::Auto:
        setUsesEffectiveTextDirection(true);
        updateEffectiveDirectionalityOfDirAuto();
        break;
    }

    if (auto* htmlParent = dynamicDowncast<HTMLElement>(parent.get()); htmlParent && htmlParent->selfOrPrecedingNodesAffectDirAuto()) {
        if (isValid && direction != TextDirectionDirective::Auto)
            setHasDirAutoFlagRecursively(this, false);
        htmlParent->adjustDirectionalityIfNeededAfterChildAttributeChanged(this);
    }
}

void HTMLElement::updateEffectiveDirectionality(std::optional<TextDirection> direction)
{
    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClassType::Dir, Style::PseudoClassChangeInvalidation::AnyValue);
    auto effectiveDirection = direction.value_or(TextDirection::LTR);
    setUsesEffectiveTextDirection(!!direction);
    if (direction)
        setEffectiveTextDirection(effectiveDirection);
    auto updateEffectiveTextDirectionOfShadowRoot = [&](HTMLElement& element) {
        if (RefPtr shadowRootOfElement = element.shadowRoot()) {
            for (Ref element : childrenOfType<HTMLElement>(*shadowRootOfElement))
                element->updateEffectiveDirectionality(direction);
        }
    };
    updateEffectiveTextDirectionOfShadowRoot(*this);
    for (auto it = descendantsOfType<HTMLElement>(*this).begin(); it;) {
        Ref element = *it;
        if (isValidDirValue(element->attributeWithoutSynchronization(dirAttr))) {
            it.traverseNextSkippingChildren();
            continue;
        }
        updateEffectiveTextDirectionOfShadowRoot(element);
        Style::PseudoClassChangeInvalidation styleInvalidation(element, CSSSelector::PseudoClassType::Dir, Style::PseudoClassChangeInvalidation::AnyValue);
        element->setUsesEffectiveTextDirection(!!direction);
        if (direction)
            element->setEffectiveTextDirection(effectiveDirection);
        it.traverseNext();
    }
}

void HTMLElement::adjustDirectionalityIfNeededAfterChildAttributeChanged(Element*)
{
    ASSERT(selfOrPrecedingNodesAffectDirAuto());
    for (Ref element : lineageOfType<HTMLElement>(*this)) {
        if (elementAffectsDirectionality(element)) {
            ASSERT(element->hasDirectionAuto());
            element->updateEffectiveDirectionalityOfDirAuto();
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

void HTMLElement::updateTextDirectionalityAfterInputTypeChange()
{
    dirAttributeChanged(attributeWithoutSynchronization(dirAttr));
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

    for (Ref elementToAdjust : lineageOfType<HTMLElement>(*this)) {
        if (elementAffectsDirectionality(elementToAdjust)) {
            ASSERT(elementToAdjust->hasDirectionAuto());
            elementToAdjust->updateEffectiveDirectionalityOfDirAuto();
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

    string = string.trim(isASCIIWhitespace<UChar>);
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

bool HTMLElement::willRespondToMouseClickEventsWithEditability(Editability editability) const
{
    return !isDisabledFormControl() && Element::willRespondToMouseClickEventsWithEditability(editability);
}

bool HTMLElement::canBeActuallyDisabled() const
{
    if (is<HTMLButtonElement>(*this)
        || is<HTMLInputElement>(*this)
        || is<HTMLSelectElement>(*this)
        || is<HTMLTextAreaElement>(*this)
        || is<HTMLOptGroupElement>(*this)
        || is<HTMLOptionElement>(*this)
        || is<HTMLFieldSetElement>(*this))
        return true;
    auto* customElement = dynamicDowncast<HTMLMaybeFormAssociatedCustomElement>(*this);
    return customElement && customElement->isFormAssociatedCustomElement();
}

bool HTMLElement::isActuallyDisabled() const
{
    return canBeActuallyDisabled() && isDisabledFormControl();
}

RefPtr<NodeList> HTMLElement::labels()
{
    if (!isLabelable())
        return nullptr;

    return ensureRareData().ensureNodeLists().addCacheWithAtomName<LabelsNodeList>(*this, starAtom());
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
    return inputModeForAttributeValue(attributeWithoutSynchronization(inputmodeAttr));
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
    CheckedPtr queue = reactionQueue();
    if (!queue)
        return Exception { ExceptionCode::NotSupportedError, "attachInternals is only supported on a custom element instance"_s };

    if (queue->isElementInternalsDisabled())
        return Exception { ExceptionCode::NotSupportedError, "attachInternals is disabled"_s };

    if (queue->isElementInternalsAttached())
        return Exception { ExceptionCode::NotSupportedError, "There is already an existing ElementInternals"_s };

    if (!isPrecustomizedOrDefinedCustomElement())
        return Exception { ExceptionCode::NotSupportedError, "Custom element is in an invalid state"_s };

    queue->setElementInternalsAttached();
    return ElementInternals::create(*this);
}

static ExceptionOr<bool> checkPopoverValidity(HTMLElement& element, PopoverVisibilityState expectedState, Document* expectedDocument = nullptr)
{
    if (element.popoverState() == PopoverState::None)
        return Exception { ExceptionCode::NotSupportedError, "Element does not have the popover attribute"_s };

    if (element.popoverData()->visibilityState() != expectedState)
        return false;

    if (!element.isConnected())
        return Exception { ExceptionCode::InvalidStateError, "Element is not connected"_s };

    if (expectedDocument && &element.document() != expectedDocument)
        return Exception { ExceptionCode::InvalidStateError, "Invalid when the document changes while showing or hiding a popover element"_s };

    if (auto* dialog = dynamicDowncast<HTMLDialogElement>(element); dialog && dialog->isModal())
        return Exception { ExceptionCode::InvalidStateError, "Element is a modal <dialog> element"_s };

#if ENABLE(FULLSCREEN_API)
    if (element.hasFullscreenFlag())
        return Exception { ExceptionCode::InvalidStateError, "Element is fullscreen"_s };
#endif

    return true;
}

// https://html.spec.whatwg.org/#topmost-popover-ancestor
// Consider both DOM ancestors and popovers where the given popover was invoked from as ancestors.
// Use top layer positions to disambiguate the topmost one when both exist.
static HTMLElement* topmostPopoverAncestor(HTMLElement& newPopover)
{
    // Store positions to avoid having to do O(n) search for every popover invoker.
    HashMap<Ref<const HTMLElement>, size_t> topLayerPositions;
    size_t i = 0;
    for (auto& element : newPopover.document().autoPopoverList())
        topLayerPositions.add(element, i++);

    topLayerPositions.add(newPopover, i);

    RefPtr<HTMLElement> topmostAncestor;

    auto checkAncestor = [&](Element* candidate) {
        if (!candidate)
            return;

        // https://html.spec.whatwg.org/#nearest-inclusive-open-popover
        auto nearestInclusiveOpenPopover = [](Element& candidate) -> HTMLElement* {
            for (RefPtr element = &candidate; element; element = element->parentElementInComposedTree()) {
                if (auto* htmlElement = dynamicDowncast<HTMLElement>(element.get())) {
                    if (htmlElement->popoverState() == PopoverState::Auto && htmlElement->popoverData()->visibilityState() == PopoverVisibilityState::Showing)
                        return htmlElement;
                }
            }
            return nullptr;
        };

        auto* candidateAncestor = nearestInclusiveOpenPopover(*candidate);
        if (!candidateAncestor)
            return;
        if (!topmostAncestor || topLayerPositions.get(*topmostAncestor) < topLayerPositions.get(*candidateAncestor))
            topmostAncestor = candidateAncestor;
    };

    checkAncestor(newPopover.parentElementInComposedTree());

    checkAncestor(newPopover.popoverData()->invoker());

    return topmostAncestor.get();
}

// https://html.spec.whatwg.org/#popover-focusing-steps
static void runPopoverFocusingSteps(HTMLElement& popover)
{
    if (RefPtr dialog = dynamicDowncast<HTMLDialogElement>(popover)) {
        dialog->runFocusingSteps();
        return;
    }

    RefPtr control = popover.hasAttribute(autofocusAttr) ? &popover : popover.findAutofocusDelegate();

    if (!control)
        return;

    control->runFocusingStepsForAutofocus();

    if (!control->document().isSameOriginAsTopDocument())
        return;

    Ref topDocument = control->document().topDocument();
    topDocument->clearAutofocusCandidates();
    topDocument->setAutofocusProcessed();
}

void HTMLElement::queuePopoverToggleEventTask(PopoverVisibilityState oldState, PopoverVisibilityState newState)
{
    if (auto queuedEventData = popoverData()->queuedToggleEventData())
        oldState = queuedEventData->oldState;
    popoverData()->setQueuedToggleEventData({ oldState, newState });
    queueTaskKeepingThisNodeAlive(TaskSource::DOMManipulation, [this, newState] {
        if (!popoverData())
            return;
        auto queuedEventData = popoverData()->queuedToggleEventData();
        if (!queuedEventData || queuedEventData->newState != newState)
            return;
        popoverData()->clearQueuedToggleEventData();
        auto stringForState = [](PopoverVisibilityState state) {
            return state == PopoverVisibilityState::Hidden ? "closed"_s : "open"_s;
        };
        dispatchEvent(ToggleEvent::create(eventNames().toggleEvent, { EventInit { }, stringForState(queuedEventData->oldState), stringForState(queuedEventData->newState) }, Event::IsCancelable::No));
    });
}

ExceptionOr<void> HTMLElement::showPopover(const HTMLFormControlElement* invoker)
{
    auto check = checkPopoverValidity(*this, PopoverVisibilityState::Hidden);
    if (check.hasException())
        return check.releaseException();
    if (!check.returnValue())
        return { };

    if (popoverData())
        popoverData()->setInvoker(invoker);

    ASSERT(!isInTopLayer());

    PopoverData::ScopedStartShowingOrHiding showOrHidingPopoverScope(*this);
    auto fireEvents = showOrHidingPopoverScope.wasShowingOrHiding() ? FireEvents::No : FireEvents::Yes;

    Ref document = this->document();
    Ref event = ToggleEvent::create(eventNames().beforetoggleEvent, { EventInit { }, "closed"_s, "open"_s }, Event::IsCancelable::Yes);
    dispatchEvent(event);
    if (event->defaultPrevented() || event->defaultHandled())
        return { };

    check = checkPopoverValidity(*this, PopoverVisibilityState::Hidden, document.ptr());
    if (check.hasException())
        return check.releaseException();
    if (!check.returnValue())
        return { };

    ASSERT(popoverData());

    bool shouldRestoreFocus = false;

    if (popoverState() == PopoverState::Auto) {
        auto originalState = popoverState();
        RefPtr ancestor = topmostPopoverAncestor(*this);
        document->hideAllPopoversUntil(ancestor.get(), FocusPreviousElement::No, fireEvents);

        if (popoverState() != originalState)
            return Exception { ExceptionCode::InvalidStateError, "The value of the popover attribute was changed while hiding the popover."_s };

        check = checkPopoverValidity(*this, PopoverVisibilityState::Hidden, document.ptr());
        if (check.hasException())
            return check.releaseException();
        if (!check.returnValue())
            return { };

        shouldRestoreFocus = !document->topmostAutoPopover();
    }

    RefPtr previouslyFocusedElement = document->focusedElement();

    addToTopLayer();

    popoverData()->setPreviouslyFocusedElement(nullptr);

    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClassType::PopoverOpen, true);
    popoverData()->setVisibilityState(PopoverVisibilityState::Showing);

    runPopoverFocusingSteps(*this);

    if (shouldRestoreFocus) {
        ASSERT(popoverState() == PopoverState::Auto);
        popoverData()->setPreviouslyFocusedElement(previouslyFocusedElement.get());
    }

    queuePopoverToggleEventTask(PopoverVisibilityState::Hidden, PopoverVisibilityState::Showing);

#if ENABLE(ACCESSIBILITY)
    if (CheckedPtr cache = document->existingAXObjectCache())
        cache->onPopoverToggle(*this);
#endif

    return { };
}

ExceptionOr<void> HTMLElement::hidePopoverInternal(FocusPreviousElement focusPreviousElement, FireEvents fireEvents)
{
    auto check = checkPopoverValidity(*this, PopoverVisibilityState::Showing);
    if (check.hasException())
        return check.releaseException();
    if (!check.returnValue())
        return { };

    ASSERT(popoverData());

    PopoverData::ScopedStartShowingOrHiding showOrHidingPopoverScope(*this);
    if (showOrHidingPopoverScope.wasShowingOrHiding())
        fireEvents = FireEvents::No;

    if (popoverState() == PopoverState::Auto) {
        // Unable to protect the document as it may have started destruction.
        document().hideAllPopoversUntil(this, focusPreviousElement, fireEvents);

        check = checkPopoverValidity(*this, PopoverVisibilityState::Showing);
        if (check.hasException())
            return check.releaseException();
        if (!check.returnValue())
            return { };
    }

    popoverData()->setInvoker(nullptr);

    if (fireEvents == FireEvents::Yes)
        dispatchEvent(ToggleEvent::create(eventNames().beforetoggleEvent, { EventInit { }, "open"_s, "closed"_s }, Event::IsCancelable::No));

    check = checkPopoverValidity(*this, PopoverVisibilityState::Showing);
    if (check.hasException())
        return check.releaseException();
    if (!check.returnValue())
        return { };

    ASSERT(popoverData());

    removeFromTopLayer();

    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClassType::PopoverOpen, false);
    popoverData()->setVisibilityState(PopoverVisibilityState::Hidden);

    if (fireEvents == FireEvents::Yes)
        queuePopoverToggleEventTask(PopoverVisibilityState::Showing, PopoverVisibilityState::Hidden);

    if (RefPtr element = popoverData()->previouslyFocusedElement()) {
        if (focusPreviousElement == FocusPreviousElement::Yes && containsIncludingShadowDOM(document().focusedElement())) {
            FocusOptions options;
            options.preventScroll = true;
            element->focus(options);
        }
        popoverData()->setPreviouslyFocusedElement(nullptr);
    }

#if ENABLE(ACCESSIBILITY)
    if (CheckedPtr cache = document().existingAXObjectCache())
        cache->onPopoverToggle(*this);
#endif

    return { };
}

ExceptionOr<void> HTMLElement::hidePopover()
{
    return hidePopoverInternal(FocusPreviousElement::Yes, FireEvents::Yes);
}

ExceptionOr<bool> HTMLElement::togglePopover(std::optional<bool> force)
{
    if (isPopoverShowing() && !force.value_or(false)) {
        auto returnValue = hidePopover();
        if (returnValue.hasException())
            return returnValue.releaseException();
    } else if (!isPopoverShowing() && force.value_or(true)) {
        auto returnValue = showPopover();
        if (returnValue.hasException())
            return returnValue.releaseException();
    } else {
        auto check = checkPopoverValidity(*this, popoverData() ? popoverData()->visibilityState() : PopoverVisibilityState::Showing);
        if (check.hasException())
            return check.releaseException();
    }
    return isPopoverShowing();
}

void HTMLElement::popoverAttributeChanged(const AtomString& value)
{
    auto computePopoverState = [](const AtomString& value) -> PopoverState {
        if (!value || value.isNull())
            return PopoverState::None;

        if (value == emptyString() || equalIgnoringASCIICase(value, autoAtom()))
            return PopoverState::Auto;

        return PopoverState::Manual;
    };

    auto newPopoverState = computePopoverState(value);

    auto oldPopoverState = popoverState();
    if (newPopoverState == oldPopoverState)
        return;

    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClassType::PopoverOpen, false);

    if (isPopoverShowing()) {
        hidePopoverInternal(FocusPreviousElement::Yes, FireEvents::Yes);
        newPopoverState = computePopoverState(attributeWithoutSynchronization(popoverAttr));
    }

    if (newPopoverState == PopoverState::None)
        clearPopoverData();
    else
        ensurePopoverData().setPopoverState(newPopoverState);
}

const AtomString& HTMLElement::popover() const
{
    switch (popoverState()) {
    case PopoverState::None:
        return nullAtom();
    case PopoverState::Auto:
        return autoAtom();
    case PopoverState::Manual:
        return manualAtom();
    }
    return nullAtom();
}

PopoverState HTMLElement::popoverState() const
{
    return popoverData() ? popoverData()->popoverState() : PopoverState::None;
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
