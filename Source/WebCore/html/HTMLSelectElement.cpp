/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2026 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2010-2022 Google Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "HTMLSelectElement.h"

#include "AXObjectCache.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "CommonAtomStrings.h"
#include "ContainerNodeInlines.h"
#include "CSSFontSelector.h"
#include "DOMFormData.h"
#include "DocumentInlines.h"
#include "DocumentPage.h"
#include "DocumentSecurityOrigin.h"
#include "ElementChildIteratorInlines.h"
#include "ElementTraversal.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "FormController.h"
#include "GenericCachedHTMLCollection.h"
#include "HTMLButtonElement.h"
#include "HTMLDataListElement.h"
#include "HTMLDivElement.h"
#include "HTMLFormElement.h"
#include "HTMLHRElement.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionsCollectionInlines.h"
#include "HTMLParserIdioms.h"
#include "HTMLSelectedContentElement.h"
#include "HTMLSlotElement.h"
#include "KeyboardEvent.h"
#include "LocalDOMWindow.h"
#include "LocalFrameInlines.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "NodeName.h"
#include "NodeRareData.h"
#include "PseudoClassChangeInvalidation.h"
#include "RenderListBox.h"
#include "RenderMenuList.h"
#include "RenderTheme.h"
#include "ScriptDisallowedScope.h"
#include "SelectFallbackButtonElement.h"
#include "SelectPopoverElement.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SlotAssignment.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

#if !PLATFORM(IOS_FAMILY)
#include <WebCore/PopupMenu.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLSelectElement);

using namespace WTF::Unicode;

using namespace HTMLNames;

static const AtomString& buttonSlotName()
{
    static MainThreadNeverDestroyed<const AtomString> buttonSlot("buttonSlot"_s);
    return buttonSlot;
}

static bool NODELETE isFirstElementChildButton(const Node& child)
{
    return is<HTMLButtonElement>(child) && !child.previousElementSibling();
}

class SelectSlotAssignment final : public NamedSlotAssignment {
private:
    void hostChildElementDidChange(const Element&, ShadowRoot&) final;
    const AtomString& NODELETE slotNameForHostChild(const Node&) const final;
};

void SelectSlotAssignment::hostChildElementDidChange(const Element& childElement, ShadowRoot& shadowRoot)
{
    if (is<HTMLButtonElement>(childElement)) {
        // Don't check whether this is the first button element
        // since we don't know the answer when this function is called inside Element::removedFrom.
        didChangeSlot(buttonSlotName(), shadowRoot);
    } else
        didChangeSlot(NamedSlotAssignment::defaultSlotName(), shadowRoot);
}

SUPPRESS_NODELETE const AtomString& SelectSlotAssignment::slotNameForHostChild(const Node& child) const
{
    return isFirstElementChildButton(child) ? buttonSlotName() : NamedSlotAssignment::defaultSlotName();
}

// https://html.spec.whatwg.org/#dom-htmloptionscollection-length
static constexpr unsigned maxSelectItems = 100000;

HTMLSelectElement::HTMLSelectElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : HTMLFormControlElement(tagName, document, form)
    , m_typeAhead(this)
    , m_size(0)
    , m_lastOnChangeIndex(-1)
    , m_activeSelectionAnchorIndex(-1)
    , m_activeSelectionEndIndex(-1)
    , m_isProcessingUserDrivenChange(false)
    , m_multiple(false)
    , m_activeSelectionState(false)
    , m_allowsNonContiguousSelection(false)
    , m_shouldRecalcListItems(false)
{
    ASSERT(hasTagName(selectTag));
}

Ref<HTMLSelectElement> HTMLSelectElement::create(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
{
    ASSERT(tagName.matches(selectTag));
    Ref select = adoptRef(*new HTMLSelectElement(tagName, document, form));
    select->addShadowRoot(ShadowRoot::create(document, makeUnique<SelectSlotAssignment>()));
    return select;
}

Ref<HTMLSelectElement> HTMLSelectElement::create(Document& document)
{
    return HTMLSelectElement::create(selectTag, document, nullptr);
}

HTMLSelectElement::~HTMLSelectElement() = default;

void HTMLSelectElement::didDetachRenderers()
{
#if !PLATFORM(IOS_FAMILY)
    if (RefPtr popup = m_popup)
        popup->hide();
    m_popup = nullptr;
    setPopupIsVisible(false);
#endif
    HTMLFormControlElement::didDetachRenderers();
}

void HTMLSelectElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    Ref document = this->document();

    ScriptDisallowedScope::EventAllowedScope rootScope { root };

    Ref buttonSlot = HTMLSlotElement::create(slotTag, document);
    ScriptDisallowedScope::EventAllowedScope buttonSlotScope { buttonSlot };
    buttonSlot->setAttributeWithoutSynchronization(inertAttr, emptyAtom());
    buttonSlot->setAttributeWithoutSynchronization(nameAttr, buttonSlotName());
    buttonSlot->appendChild(SelectFallbackButtonElement::create(document));
    root.appendChild(buttonSlot);
    m_buttonSlot = WTF::move(buttonSlot);

    if (!document->settings().htmlEnhancedSelectEnabled()) {
        root.appendChild(HTMLSlotElement::create(slotTag, document));
        return;
    }

    Ref popover = SelectPopoverElement::create(document);
    ScriptDisallowedScope::EventAllowedScope popoverScope { popover };
    popover->setAttributeWithoutSynchronization(popoverAttr, autoAtom());
    popover->setUserAgentPart(pickerSelectAtom());

    popover->appendChild(HTMLSlotElement::create(slotTag, document));

    root.appendChild(popover);
    m_popover = WTF::move(popover);
}

HTMLSelectElement* HTMLSelectElement::findOwnerSelect(ContainerNode* startNode, ExcludeOptGroup excludeOptGroup)
{
    if (!startNode)
        return nullptr;
    if (auto* select = dynamicDowncast<HTMLSelectElement>(*startNode))
        return select;
    if (is<HTMLOptGroupElement>(*startNode)) {
        if (excludeOptGroup == ExcludeOptGroup::Yes)
            return nullptr;
        return findOwnerSelect(startNode->parentNode(), ExcludeOptGroup::Yes);
    }
    if (is<HTMLDataListElement>(*startNode) || is<HTMLHRElement>(*startNode) || is<HTMLOptionElement>(*startNode))
        return nullptr;
    return findOwnerSelect(startNode->parentNode(), excludeOptGroup);
}

void HTMLSelectElement::didRecalcStyle(OptionSet<Style::Change> styleChange)
{
    // Even though the options didn't necessarily change, we will call setOptionsChangedOnRenderer for its side effect
    // of recomputing the width of the element. We need to do that if the style change included a change in zoom level.
    setOptionsChangedOnRenderer();

    // When the select's style changes, invalidate the fallback button's style since it depends on
    // the host's usedAppearance() to compute the padding.
    if (styleChange.contains(Style::Change::NonInherited)) {
        if (RefPtr buttonSlot = m_buttonSlot.get()) {
            if (RefPtr fallbackButton = dynamicDowncast<SelectFallbackButtonElement>(buttonSlot->firstChild()))
                fallbackButton->invalidateStyle();
        }
    }

    HTMLFormControlElement::didRecalcStyle(styleChange);
}

const AtomString& HTMLSelectElement::formControlType() const
{
    static MainThreadNeverDestroyed<const AtomString> selectMultiple("select-multiple"_s);
    static MainThreadNeverDestroyed<const AtomString> selectOne("select-one"_s);
    return m_multiple ? selectMultiple : selectOne;
}

void HTMLSelectElement::optionSelectedByUser(int optionIndex, bool fireOnChangeNow, bool allowMultipleSelection)
{
    // User interaction such as mousedown events can cause list box select elements to send change events.
    // This produces that same behavior for changes triggered by other code running on behalf of the user.
    if (!usesMenuListDeprecated()) {
        updateSelectedState(optionToListIndex(optionIndex), allowMultipleSelection, false);
        updateValidity();
        if (CheckedPtr renderer = this->renderer())
            renderer->updateFromElement();
        if (fireOnChangeNow)
            listBoxOnChange();
        return;
    }

    // Bail out if this index is already the selected one, to avoid running unnecessary JavaScript that can mess up
    // autofill when there is no actual change (see https://bugs.webkit.org/show_bug.cgi?id=35256 and <rdar://7467917>).
    // The selectOption function does not behave this way, possibly because other callers need a change event even
    // in cases where the selected option is not change.
    if (optionIndex == selectedIndex())
        return;

    OptionSet flags = { SelectOptionFlag::DeselectOtherOptions, SelectOptionFlag::UserDriven };
    if (fireOnChangeNow)
        flags.add(SelectOptionFlag::DispatchChangeEvent);
    selectOption(optionIndex, flags);
}

bool HTMLSelectElement::hasPlaceholderLabelOption() const
{
    // The select element has no placeholder label option if it has an attribute "multiple" specified or a display size of non-1.
    // 
    // The condition "size() > 1" is not compliant with the HTML5 spec as of Dec 3, 2010. "size() != 1" is correct.
    // Using "size() > 1" here because size() may be 0 in WebKit.
    // See the discussion at https://bugs.webkit.org/show_bug.cgi?id=43887
    //
    // "0 size()" happens when an attribute "size" is absent or an invalid size attribute is specified.
    // In this case, the display size should be assumed as the default.
    // The default display size is 1 for non-multiple select elements, and 4 for multiple select elements.
    //
    // Finally, if size() == 0 and non-multiple, the display size can be assumed as 1.
    if (multiple() || size() > 1)
        return false;

    int listIndex = optionToListIndex(0);
    ASSERT(listIndex >= 0);
    if (listIndex)
        return false;
    Ref option = downcast<HTMLOptionElement>(*listItems()[listIndex]);
    return option->value().isEmpty();
}

String HTMLSelectElement::validationMessage() const
{
    if (!willValidate())
        return String();

    if (customError())
        return customValidationMessage();

    return valueMissing() ? validationMessageValueMissingForSelectText() : String();
}

bool HTMLSelectElement::valueMissing() const
{
    if (!isRequired())
        return false;

    int firstSelectionIndex = selectedIndex();

    // If a non-placeholer label option is selected (firstSelectionIndex > 0), it's not value-missing.
    return firstSelectionIndex < 0 || (!firstSelectionIndex && hasPlaceholderLabelOption());
}

bool HTMLSelectElement::usesMenuList() const
{
#if !PLATFORM(IOS_FAMILY)
    return !m_multiple && m_size <= 1;
#else
    return true;
#endif
}

bool HTMLSelectElement::usesMenuListDeprecated() const
{
#if !PLATFORM(IOS_FAMILY)
    return !m_multiple && m_size <= 1;
#else
    return !m_multiple;
#endif
}

bool HTMLSelectElement::usesBaseAppearancePicker() const
{
    if (m_multiple || m_size > 1)
        return false;

    RefPtr popover = m_popover;
    if (!popover)
        return false;

    ASSERT(document().settings().htmlEnhancedSelectEnabled());

    if (CheckedPtr style = existingComputedStyle(); !style || style->usedAppearance() != StyleAppearance::Base)
        return false;

    CheckedPtr pickerStyle = popover->computedStyle();
    return pickerStyle && pickerStyle->usedAppearance() == StyleAppearance::Base;
}

SelectPopoverElement* HTMLSelectElement::pickerPopoverElement() const
{
    return m_popover;
}

void HTMLSelectElement::hidePickerPopoverElement()
{
    RefPtr popover = m_popover;
    if (!popover)
        return;

    setPopupIsVisible(false);
    popover->hidePopover();
}

static inline auto navigationKeyIdentifiersForWritingMode(const RenderElement* renderer) -> HTMLSelectElement::NavigationKeyIdentifiers
{
    bool isHorizontalWritingMode = renderer ? renderer->writingMode().isHorizontal() : true;
    bool isBlockFlipped = renderer ? renderer->writingMode().isBlockFlipped() : false;

    auto next = isHorizontalWritingMode ? "Down"_s : "Right"_s;
    auto previous = isHorizontalWritingMode ? "Up"_s : "Left"_s;
    if (isBlockFlipped)
        std::swap(next, previous);

    return { next, previous };
}

auto HTMLSelectElement::pickerNavigationKeyIdentifiers() const -> NavigationKeyIdentifiers
{
    RefPtr popover = m_popover;
    CheckedPtr renderer = popover ? popover->renderer() : nullptr;
    return navigationKeyIdentifiersForWritingMode(renderer);
}

int HTMLSelectElement::computeNavigationIndex(const String& keyIdentifier, int currentListIndex, NavigationKeyIdentifiers navKeys) const
{
    // Primary axis (writing-mode aware block direction).
    if (keyIdentifier == navKeys.next)
        return nextSelectableListIndex(currentListIndex);
    if (keyIdentifier == navKeys.previous)
        return previousSelectableListIndex(currentListIndex);

    // Secondary axis (the other pair of arrow keys, for convenience).
    bool primaryIsVertical = (navKeys.next == "Down"_s || navKeys.next == "Up"_s);
    if (primaryIsVertical) {
        // Primary is Down/Up, secondary is Right/Left.
        if (keyIdentifier == "Right"_s)
            return nextSelectableListIndex(currentListIndex);
        if (keyIdentifier == "Left"_s)
            return previousSelectableListIndex(currentListIndex);
    } else {
        // Primary is Right/Left, secondary is Down/Up.
        if (keyIdentifier == "Down"_s)
            return nextSelectableListIndex(currentListIndex);
        if (keyIdentifier == "Up"_s)
            return previousSelectableListIndex(currentListIndex);
    }

    if (keyIdentifier == "Home"_s)
        return firstSelectableListIndex();
    if (keyIdentifier == "End"_s)
        return lastSelectableListIndex();
    if (keyIdentifier == "PageDown"_s)
        return nextValidIndex(currentListIndex, SkipDirection::Forwards, 3);
    if (keyIdentifier == "PageUp"_s)
        return nextValidIndex(currentListIndex, SkipDirection::Backwards, 3);

    return -1;
}

int HTMLSelectElement::activeSelectionStartListIndex() const
{
    if (m_activeSelectionAnchorIndex >= 0)
        return m_activeSelectionAnchorIndex;
    return optionToListIndex(selectedIndex());
}

int HTMLSelectElement::activeSelectionEndListIndex() const
{
    if (m_activeSelectionEndIndex >= 0)
        return m_activeSelectionEndIndex;
    return lastSelectedListIndex();
}

ExceptionOr<void> HTMLSelectElement::add(const OptionOrOptGroupElement& element, const std::optional<HTMLElementOrInt>& before)
{
    RefPtr<HTMLElement> beforeElement;
    Ref<ContainerNode> parent = *this;
    if (before) {
        beforeElement = WTF::switchOn(before.value(),
            [](const Ref<HTMLElement>& element) -> RefPtr<HTMLElement> { return element.ptr(); },
            [this](int index) -> RefPtr<HTMLElement> { return item(index); }
        );
        if (std::holds_alternative<int>(before.value()) && beforeElement && beforeElement->parentNode())
            parent = *beforeElement->parentNode();
    }
    Ref toInsert = WTF::switchOn(element,
        [](const auto& htmlElement) -> HTMLElement& { return htmlElement; }
    );

    return parent->insertBefore(toInsert, WTF::move(beforeElement));
}

void HTMLSelectElement::remove(int optionIndex)
{
    int listIndex = optionToListIndex(optionIndex);
    if (listIndex < 0)
        return;

    Ref { *listItems()[listIndex] }->remove();
}

String HTMLSelectElement::value() const
{
    if (protect(document())->requiresScriptTrackingPrivacyProtection(ScriptTrackingPrivacyCategory::FormControls))
        return emptyString();
    for (auto& item : listItems()) {
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(item.get())) {
            if (option->selected())
                return option->value();
        }
    }
    return emptyString();
}

void HTMLSelectElement::setValue(const String& value)
{
    // Find the option with value() matching the given parameter and make it the current selection.
    unsigned optionIndex = 0;
    for (auto& item : listItems()) {
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(item.get())) {
            if (option->value() == value) {
                setSelectedIndex(optionIndex);
                return;
            }
            ++optionIndex;
        }
    }

    setSelectedIndex(-1);
}

bool HTMLSelectElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == alignAttr) {
        // Don't map 'align' attribute. This matches what Firefox, Opera and IE do.
        // See http://bugs.webkit.org/show_bug.cgi?id=12072
        return false;
    }

    return HTMLFormControlElement::hasPresentationalHintsForAttribute(name);
}

void HTMLSelectElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::sizeAttr: {
        unsigned oldSize = m_size;
        unsigned size = limitToOnlyHTMLNonNegative(newValue);

        // Ensure that we've determined selectedness of the items at least once prior to changing the size.
        if (oldSize != size)
            updateListItemSelectedStates();

        m_size = size;
        updateValidity();
        if (m_size != oldSize) {
            invalidateStyleAndRenderersForSubtree();
            setRecalcListItems();
            updateValidity();
        }
        break;
    }
    case AttributeNames::multipleAttr:
        parseMultipleAttribute(newValue);
        break;
    default:
        HTMLFormControlElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
        break;
    }
}

int HTMLSelectElement::defaultTabIndex() const
{
    return 0;
}

bool HTMLSelectElement::isKeyboardFocusable(const FocusEventData& focusEventData) const
{
    if (renderer())
        return isFocusable();
    return HTMLFormControlElement::isKeyboardFocusable(focusEventData);
}

bool HTMLSelectElement::isMouseFocusable() const
{
    if (renderer())
        return isFocusable();
    return HTMLFormControlElement::isMouseFocusable();
}

RenderPtr<RenderElement> HTMLSelectElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& position)
{
    if (usesMenuList()) {
        if (style.usedAppearance() == StyleAppearance::Base)
            return HTMLElement::createElementRenderer(WTF::move(style), position);
        return createRenderer<RenderMenuList>(*this, WTF::move(style));
    }
    return createRenderer<RenderListBox>(*this, WTF::move(style));
}

bool HTMLSelectElement::childShouldCreateRenderer(const Node& child) const
{
    if (!HTMLFormControlElement::childShouldCreateRenderer(child))
        return false;
    if (!usesMenuList())
        return is<HTMLOptionElement>(child) || is<HTMLOptGroupElement>(child) || validationMessageShadowTreeContains(child);
    if (child.isInShadowTree() && child.containingShadowRoot() == userAgentShadowRoot())
        return true;
    if (isFirstElementChildButton(child))
        return true;
    if (child.isBeforePseudoElement() || child.isAfterPseudoElement())
        return true;
    if (usesBaseAppearancePicker())
        return true;
    return validationMessageShadowTreeContains(child);
}

Ref<HTMLCollection> HTMLSelectElement::selectedOptions()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<HTMLSelectedOptionsCollection>(*this);
}

Ref<HTMLOptionsCollection> HTMLSelectElement::options()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<HTMLOptionsCollection>(*this);
}

void HTMLSelectElement::updateListItemSelectedStates(AllowStyleInvalidation allowStyleInvalidation)
{
    if (m_shouldRecalcListItems)
        recalcListItems(true, allowStyleInvalidation);
}

CompletionHandlerCallingScope HTMLSelectElement::optionToSelectFromChildChangeScope(const ContainerNode::ChildChange& change, HTMLOptGroupElement* parentOptGroup)
{
    if (multiple())
        return { };

    auto getLastSelectedOption = [](HTMLOptGroupElement& optGroup) -> HTMLOptionElement* {
        for (auto* option = Traversal<HTMLOptionElement>::lastChild(optGroup); option; option = Traversal<HTMLOptionElement>::previousSibling(*option)) {
            if (option->selectedWithoutUpdate())
                return option;
        }
        return nullptr;
    };

    RefPtr<HTMLOptionElement> optionToSelect;
    if (change.type == ChildChange::Type::ElementInserted) {
        if (auto* option = dynamicDowncast<HTMLOptionElement>(*change.siblingChanged)) {
            if (option->selectedWithoutUpdate())
                optionToSelect = option;
        } else if (RefPtr optGroup = dynamicDowncast<HTMLOptGroupElement>(change.siblingChanged); !parentOptGroup && optGroup)
            optionToSelect = getLastSelectedOption(*optGroup);
    } else if (parentOptGroup && change.type == ContainerNode::ChildChange::Type::AllChildrenReplaced)
        optionToSelect = getLastSelectedOption(*parentOptGroup);

    return CompletionHandlerCallingScope { [optionToSelect = WTF::move(optionToSelect), isInsertion = change.isInsertion(), select = Ref { *this }] {
        if (optionToSelect)
            select->optionSelectionStateChanged(*optionToSelect, true);
        else if (isInsertion)
            select->scrollToSelection();
    } };
}

// FIXME: we should really make this disappear when
// document().settings().htmlEnhancedSelectParsingEnabled() is true, but
// https://github.com/whatwg/html/issues/11825 needs to be resolved. It might not be possible
// without a risky behavioral change.
void HTMLSelectElement::childrenChanged(const ChildChange& change)
{
    ASSERT(change.affectsElements != ChildChange::AffectsElements::Unknown);

    if (change.affectsElements == ChildChange::AffectsElements::No) {
        HTMLFormControlElement::childrenChanged(change);
        return;
    }

    auto selectOptionIfNecessaryScope = optionToSelectFromChildChangeScope(change);

    setRecalcListItems();
    updateValidity();
    m_lastOnChangeSelection.clear();

    HTMLFormControlElement::childrenChanged(change);
}

void HTMLSelectElement::optionElementChildrenChanged()
{
    setOptionsChangedOnRenderer();
    invalidateStyleForSubtree();
    updateValidity();
    updateButtonText();
}

void HTMLSelectElement::updateButtonText(HTMLOptionElement* selectedOption, int optionIndex)
{
    protect(downcast<SelectFallbackButtonElement>(*protect(m_buttonSlot)->firstChild()))->updateText(selectedOption, optionIndex);
}

void HTMLSelectElement::setSize(unsigned size)
{
    setUnsignedIntegralAttribute(sizeAttr, limitToOnlyHTMLNonNegative(size));
}

HTMLOptionElement* HTMLSelectElement::namedItem(const AtomString& name)
{
    return options()->namedItem(name);
}

HTMLOptionElement* HTMLSelectElement::item(unsigned index)
{
    return options()->item(index);
}

bool HTMLSelectElement::isSupportedPropertyIndex(unsigned index)
{
    return options()->isSupportedPropertyIndex(index);
}

ExceptionOr<void> HTMLSelectElement::setItem(unsigned index, HTMLOptionElement* option)
{
    if (!option) {
        remove(index);
        return { };
    }

    // If we are adding options, we should check 'index > maxSelectItems' first to avoid integer overflow.
    if (index > length() && index >= maxSelectItems) {
        protect(document())->addConsoleMessage(MessageSource::Other, MessageLevel::Warning, makeString("Unable to expand the option list and set an option at index. The maximum list length is "_s, maxSelectItems, '.'));
        return { };
    }

    int diff = index - length();
    
    std::optional<HTMLElementOrInt> before;
    // Out of array bounds? First insert empty dummies.
    if (diff > 0) {
        auto result = setLength(index);
        if (result.hasException())
            return result;
        // Replace an existing entry?
    } else if (diff < 0) {
        if (RefPtr itemBefore = item(index + 1))
            before = itemBefore.releaseNonNull();
        remove(index);
    }

    // Finally add the new element.
    auto result = add(*option, before);
    if (result.hasException())
        return result;

    if (diff >= 0 && option->selected())
        optionSelectionStateChanged(*option, true);

    return { };
}

ExceptionOr<void> HTMLSelectElement::setLength(unsigned newLength)
{
    // If we are adding options, we should check 'index > maxSelectItems' first to avoid integer overflow.
    if (newLength > length() && newLength > maxSelectItems) {
        protect(document())->addConsoleMessage(MessageSource::Other, MessageLevel::Warning, makeString("Unable to expand the option list to length "_s, newLength, " items. The maximum number of items allowed is "_s, maxSelectItems, '.'));
        return { };
    }

    int diff = length() - newLength;

    if (diff < 0) { // Add dummy elements.
        do {
            auto result = add(HTMLOptionElement::create(protect(document())), std::nullopt);
            if (result.hasException())
                return result;
        } while (++diff);
    } else {
        auto& items = listItems();

        // Removing children fires mutation events, which might mutate the DOM further, so we first copy out a list
        // of elements that we intend to remove then attempt to remove them one at a time.
        Vector<Ref<HTMLOptionElement>> itemsToRemove;
        size_t optionIndex = 0;
        for (auto& item : items) {
            RefPtr option = dynamicDowncast<HTMLOptionElement>(*item);
            if (option && optionIndex++ >= newLength) {
                ASSERT(item->parentNode());
                itemsToRemove.append(option.releaseNonNull());
            }
        }

        // FIXME: Clients can detect what order we remove the options in; is it good to remove them in ascending order?
        // FIXME: This ignores exceptions. A previous version passed through the exception only for the last item removed.
        // What exception behavior do we want?
        for (auto& item : itemsToRemove)
            item->remove();
    }
    return { };
}

bool HTMLSelectElement::isRequiredFormControl() const
{
    return isRequired();
}

bool HTMLSelectElement::willRespondToMouseClickEventsWithEditability(Editability editability) const
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(editability);
    return !isDisabledFormControl();
#else
    return HTMLFormControlElement::willRespondToMouseClickEventsWithEditability(editability);
#endif
}

// Returns the 1st valid item |skip| items from |listIndex| in direction |direction| if there is one.
// Otherwise, it returns the valid item closest to that boundary which is past |listIndex| if there is one.
// Otherwise, it returns |listIndex|.
// Valid means that it is enabled and an option element.
int HTMLSelectElement::nextValidIndex(int listIndex, SkipDirection direction, int skip) const
{
    auto& listItems = this->listItems();
    int lastGoodIndex = listIndex;
    int size = listItems.size();
    int step = direction == SkipDirection::Forwards ? 1 : -1;
    bool isBaseSelectPicker = usesBaseAppearancePicker();
    for (listIndex += step; listIndex >= 0 && listIndex < size; listIndex += step) {
        --skip;
        RefPtr listItem = listItems[listIndex].get();
        if (!listItem->isDisabledFormControl() && is<HTMLOptionElement>(*listItem)) {
            if (isBaseSelectPicker && !listItem->isFocusable()) {
                // Skip hidden options.
                continue;
            }
            lastGoodIndex = listIndex;
            if (skip <= 0)
                break;
        }
    }
    return lastGoodIndex;
}

int HTMLSelectElement::nextSelectableListIndex(int startIndex) const
{
    return nextValidIndex(startIndex, SkipDirection::Forwards, 1);
}

int HTMLSelectElement::previousSelectableListIndex(int startIndex) const
{
    if (startIndex == -1)
        startIndex = listItems().size();
    return nextValidIndex(startIndex, SkipDirection::Backwards, 1);
}

int HTMLSelectElement::firstSelectableListIndex() const
{
    auto& items = listItems();
    int index = nextValidIndex(items.size(), SkipDirection::Backwards, INT_MAX);
    if (static_cast<size_t>(index) == items.size())
        return -1;
    return index;
}

int HTMLSelectElement::lastSelectableListIndex() const
{
    return nextValidIndex(-1, SkipDirection::Forwards, INT_MAX);
}

// Returns the index of the next valid item one page away from |startIndex| in direction |direction|.
int HTMLSelectElement::nextSelectableListIndexPageAway(int startIndex, SkipDirection direction) const
{
    auto& items = listItems();

    // Can't use m_size because renderer forces a minimum size.
    int pageSize = 0;
    if (CheckedPtr renderListBox = dynamicDowncast<RenderListBox>(*renderer()))
        pageSize = renderListBox->size() - 1; // -1 so we still show context.

    // One page away, but not outside valid bounds.
    // If there is a valid option item one page away, the index is chosen.
    // If there is no exact one page away valid option, returns startIndex or the most far index.
    int edgeIndex = direction == SkipDirection::Forwards ? 0 : items.size() - 1;
    int skipAmount = pageSize + (direction == SkipDirection::Forwards ? startIndex : edgeIndex - startIndex);
    return nextValidIndex(edgeIndex, direction, skipAmount);
}

void HTMLSelectElement::selectAll()
{
    ASSERT(m_multiple);
    if (!renderer() || !m_multiple)
        return;

    // Save the selection so it can be compared to the new selectAll selection
    // when dispatching change events.
    saveLastSelection();

    m_activeSelectionState = true;
    setActiveSelectionAnchorIndex(nextSelectableListIndex(-1));
    setActiveSelectionEndIndex(previousSelectableListIndex(-1));
    if (m_activeSelectionAnchorIndex < 0)
        return;

    updateListBoxSelection(false);
    listBoxOnChange();
    updateValidity();
}

void HTMLSelectElement::saveLastSelection()
{
    if (usesMenuListDeprecated()) {
        m_lastOnChangeIndex = selectedIndex();
        return;
    }

    m_lastOnChangeSelection = listItems().map([](auto& element) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(*element);
        return option && option->selected();
    });
}

void HTMLSelectElement::setActiveSelectionAnchorIndex(int index)
{
    m_activeSelectionAnchorIndex = index;

    // Cache the selection state so we can restore the old selection as the new
    // selection pivots around this anchor index.
    m_cachedStateForActiveSelection = listItems().map([](auto& element) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(*element);
        return option && option->selected();
    });
}

void HTMLSelectElement::setActiveSelectionEndIndex(int index)
{
    m_activeSelectionEndIndex = index;
}

void HTMLSelectElement::updateListBoxSelection(bool deselectOtherOptions)
{
    ASSERT(renderer());

#if !PLATFORM(IOS_FAMILY)
    ASSERT(renderer()->isRenderListBox() || m_multiple);
#else
    ASSERT(renderer()->isRenderMenuList() || m_multiple);
#endif

    ASSERT(!listItems().size() || m_activeSelectionAnchorIndex >= 0);

    unsigned start = std::min(m_activeSelectionAnchorIndex, m_activeSelectionEndIndex);
    unsigned end = std::max(m_activeSelectionAnchorIndex, m_activeSelectionEndIndex);

    auto& items = listItems();
    for (unsigned i = 0; i < items.size(); ++i) {
        RefPtr element = dynamicDowncast<HTMLOptionElement>(*items[i]);
        if (!element || element->isDisabledFormControl())
            continue;

        if (i >= start && i <= end)
            element->setSelectedState(m_activeSelectionState);
        else if (deselectOtherOptions || i >= m_cachedStateForActiveSelection.size())
            element->setSelectedState(false);
        else
            element->setSelectedState(m_cachedStateForActiveSelection[i]);
    }

    invalidateSelectedItems();
    scrollToSelection();
    updateValidity();
}

void HTMLSelectElement::listBoxOnChange()
{
    ASSERT(!usesMenuListDeprecated() || m_multiple);

    auto& items = listItems();

    // If the cached selection list is empty, or the size has changed, then fire
    // dispatchFormControlChangeEvent, and return early.
    if (m_lastOnChangeSelection.isEmpty() || m_lastOnChangeSelection.size() != items.size()) {
        dispatchFormControlChangeEvent();
        return;
    }

    // Update m_lastOnChangeSelection and fire dispatchFormControlChangeEvent.
    bool fireOnChange = false;
    for (unsigned i = 0; i < items.size(); ++i) {
        bool selected = [&] {
            RefPtr option = dynamicDowncast<HTMLOptionElement>(*items[i]);
            return option && option->selected();
        }();
        if (selected != m_lastOnChangeSelection[i])
            fireOnChange = true;
        m_lastOnChangeSelection[i] = selected;
    }

    if (fireOnChange) {
        dispatchInputEvent();
        dispatchFormControlChangeEvent();
    }
}

void HTMLSelectElement::dispatchChangeEventForMenuList()
{
    ASSERT(usesMenuListDeprecated());

    int selected = selectedIndex();
    if (m_lastOnChangeIndex != selected && m_isProcessingUserDrivenChange) {
        m_lastOnChangeIndex = selected;
        m_isProcessingUserDrivenChange = false;
        dispatchInputEvent();
        dispatchFormControlChangeEvent();
    }
}

void HTMLSelectElement::scrollToSelection()
{
#if !PLATFORM(IOS_FAMILY)
    if (usesMenuList())
        return;

    if (CheckedPtr renderer = dynamicDowncast<RenderListBox>(this->renderer()))
        renderer->selectionChanged();
#else
    if (CheckedPtr renderer = this->renderer())
        renderer->repaint();
#endif
}

void HTMLSelectElement::setOptionsChangedOnRenderer()
{
    if (CheckedPtr renderer = this->renderer()) {
        if (auto* renderMenuList = dynamicDowncast<RenderMenuList>(*renderer))
            renderMenuList->setOptionsChanged(true);
        else if (!usesMenuList())
            downcast<RenderListBox>(*renderer).setOptionsChanged(true);
    }

#if !PLATFORM(IOS_FAMILY)
    if (!m_popupIsVisible)
        return;

    if (RefPtr popup = m_popup)
        popup->updateFromElement();
#endif
}

const Vector<WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData>>& HTMLSelectElement::listItems() const
{
    if (m_shouldRecalcListItems)
        recalcListItems();
    else {
#if ASSERT_ENABLED
        Vector<WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData>> items = m_listItems;
        recalcListItems(false);
        ASSERT(items == m_listItems);
#endif
    }

    return m_listItems;
}

void HTMLSelectElement::invalidateSelectedItems()
{
    if (RefPtr collection = cachedHTMLCollection(CollectionType::SelectedOptions))
        collection->invalidateCache();
}

void HTMLSelectElement::setRecalcListItems()
{
    m_shouldRecalcListItems = true;
    // Manual selection anchor is reset when manipulating the select programmatically.
    m_activeSelectionAnchorIndex = -1;
    setOptionsChangedOnRenderer();
    invalidateStyleForSubtree();
    if (!isConnected()) {
        if (RefPtr collection = cachedHTMLCollection(CollectionType::SelectOptions))
            collection->invalidateCache();
        invalidateSelectedItems();
    }

    Ref document = this->document();
    if (this == document->focusedElement()) {
        if (RefPtr page = document->page())
            page->chrome().client().focusedSelectElementDidChangeOptions(*this);
    }
}

void HTMLSelectElement::recalcListItems(bool updateSelectedStates, AllowStyleInvalidation allowStyleInvalidation) const
{
    m_listItems.clear();

    m_shouldRecalcListItems = false;

    RefPtr<HTMLOptionElement> foundSelected;
    RefPtr<HTMLOptionElement> firstOption;
    auto handleOptionElement = [&](HTMLOptionElement& option) {
        m_listItems.append(&option);
        if (updateSelectedStates && !m_multiple) {
            if (!firstOption)
                firstOption = option;
            if (option.selected()) {
                if (foundSelected)
                    foundSelected->setSelectedState(false, allowStyleInvalidation);
                foundSelected = option;
            } else if (m_size <= 1 && !foundSelected && !option.isDisabledFormControl()) {
                foundSelected = option;
                foundSelected->setSelectedState(true, allowStyleInvalidation);
            }
        }
    };

    if (document().settings().htmlEnhancedSelectParsingEnabled()) {
        for (auto it = descendantsOfType<HTMLElement>(*const_cast<HTMLSelectElement*>(this)).begin(); it;) {
            Ref descendant = *it;
            if (RefPtr option = dynamicDowncast<HTMLOptionElement>(descendant)) {
                handleOptionElement(*option);
                it.traverseNextSkippingChildren();
                continue;
            }
            if (is<HTMLOptGroupElement>(descendant)) {
                m_listItems.append(descendant.ptr());
                for (auto optGroupIt = descendantsOfType<HTMLElement>(descendant).begin(); optGroupIt;) {
                    Ref optGroupDescendant = *optGroupIt;
                    if (RefPtr option = dynamicDowncast<HTMLOptionElement>(optGroupDescendant)) {
                        handleOptionElement(*option);
                        optGroupIt.traverseNextSkippingChildren();
                        continue;
                    }
                    if (is<HTMLOptGroupElement>(optGroupDescendant)
                        || is<HTMLDataListElement>(optGroupDescendant)
                        || is<HTMLSelectElement>(optGroupDescendant)
                        || is<HTMLHRElement>(optGroupDescendant)) {
                        optGroupIt.traverseNextSkippingChildren();
                        continue;
                    }
                    optGroupIt.traverseNext();
                }
                it.traverseNextSkippingChildren();
                continue;
            }
            if (is<HTMLHRElement>(descendant)) {
                m_listItems.append(descendant.ptr());
                it.traverseNextSkippingChildren();
                continue;
            }
            if (is<HTMLDataListElement>(descendant) || is<HTMLSelectElement>(descendant)) {
                it.traverseNextSkippingChildren();
                continue;
            }
            it.traverseNext();
        }
    } else {
        for (Ref child : childrenOfType<HTMLElement>(*const_cast<HTMLSelectElement*>(this))) {
            if (is<HTMLOptGroupElement>(child)) {
                m_listItems.append(child.ptr());
                for (Ref option : childrenOfType<HTMLOptionElement>(child))
                    handleOptionElement(option);
            } else if (RefPtr option = dynamicDowncast<HTMLOptionElement>(child))
                handleOptionElement(*option);
            else if (is<HTMLHRElement>(child))
                m_listItems.append(child.ptr());
        }
    }

    if (!foundSelected && m_size <= 1 && firstOption && !firstOption->selected())
        firstOption->setSelectedState(true, allowStyleInvalidation);
}

int HTMLSelectElement::selectedIndex() const
{
    unsigned index = 0;

    // Return the number of the first option selected.
    for (auto& element : listItems()) {
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*element)) {
            if (option->selected())
                return index;
            ++index;
        }
    }

    return -1;
}

void HTMLSelectElement::setSelectedIndex(int index)
{
    selectOption(index, SelectOptionFlag::DeselectOtherOptions);
}

void HTMLSelectElement::optionSelectionStateChanged(HTMLOptionElement& option, bool optionIsSelected)
{
    ASSERT(option.ownerSelectElement() == this);
    if (optionIsSelected)
        selectOption(option.index());
    else if (!usesMenuListDeprecated())
        selectOption(-1);
    else
        selectOption(nextSelectableListIndex(-1));
}

void HTMLSelectElement::selectOption(int optionIndex, OptionSet<SelectOptionFlag> flags)
{
    bool shouldDeselect = !m_multiple || flags.contains(SelectOptionFlag::DeselectOtherOptions);

    auto& items = listItems();
    int listIndex = optionToListIndex(optionIndex);

    RefPtr<HTMLElement> element;
    if (listIndex >= 0)
        element = items[listIndex].get();

    if (shouldDeselect)
        deselectItemsWithoutValidation(element.get());

    RefPtr<HTMLOptionElement> selectedOption;
    if (RefPtr option = dynamicDowncast<HTMLOptionElement>(element)) {
        if (m_activeSelectionAnchorIndex < 0 || shouldDeselect)
            setActiveSelectionAnchorIndex(listIndex);
        if (m_activeSelectionEndIndex < 0 || shouldDeselect)
            setActiveSelectionEndIndex(listIndex);
        option->setSelectedState(true);
        selectedOption = option;
    }

    invalidateSelectedItems();
    updateValidity();

    // Update the button text element to display the new selection and ensure it picks up the new
    // selection's direction and unicode-bidi.
    updateButtonText(selectedOption.get(), optionIndex);
    if (document().settings().htmlEnhancedSelectEnabled()
        && !document().settings().mutationEventsEnabled())
        updateSelectedContent(selectedOption.get());

    scrollToSelection();

    if (usesMenuListDeprecated()) {
        m_isProcessingUserDrivenChange = flags.contains(SelectOptionFlag::UserDriven);
        if (flags.contains(SelectOptionFlag::DispatchChangeEvent))
            dispatchChangeEventForMenuList();
        didUpdateActiveOption(optionIndex);
    }
}

int HTMLSelectElement::optionToListIndex(int optionIndex) const
{
    auto& items = listItems();
    int listSize = static_cast<int>(items.size());
    if (optionIndex < 0 || optionIndex >= listSize)
        return -1;

    int optionIndex2 = -1;
    for (int listIndex = 0; listIndex < listSize; ++listIndex) {
        if (is<HTMLOptionElement>(*items[listIndex])) {
            ++optionIndex2;
            if (optionIndex2 == optionIndex)
                return listIndex;
        }
    }

    return -1;
}

int HTMLSelectElement::listToOptionIndex(int listIndex) const
{
    auto& items = listItems();
    if (listIndex < 0 || listIndex >= static_cast<int>(items.size()) || !is<HTMLOptionElement>(*items[listIndex]))
        return -1;

    // Actual index of option not counting OPTGROUP entries that may be in list.
    int optionIndex = 0;
    for (int i = 0; i < listIndex; ++i) {
        if (is<HTMLOptionElement>(*items[i]))
            ++optionIndex;
    }

    return optionIndex;
}

void HTMLSelectElement::dispatchFocusEvent(RefPtr<Element>&& oldFocusedElement, const FocusOptions& options)
{
    // Save the selection so it can be compared to the new selection when
    // dispatching change events during blur event dispatch.
    if (usesMenuListDeprecated())
        saveLastSelection();
    HTMLFormControlElement::dispatchFocusEvent(WTF::move(oldFocusedElement), options);
}

void HTMLSelectElement::dispatchBlurEvent(RefPtr<Element>&& newFocusedElement)
{
    // We only need to fire change events here for menu lists, because we fire
    // change events for list boxes whenever the selection change is actually made.
    // This matches other browsers' behavior.
    if (usesMenuListDeprecated())
        dispatchChangeEventForMenuList();
    HTMLFormControlElement::dispatchBlurEvent(WTF::move(newFocusedElement));
}

void HTMLSelectElement::deselectItemsWithoutValidation(HTMLElement* excludeElement)
{
    for (auto& element : listItems()) {
        if (element == excludeElement)
            continue;
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*element))
            option->setSelectedState(false);
    }
    invalidateSelectedItems();
}

FormControlState HTMLSelectElement::saveFormControlState() const
{
    FormControlState state;
    auto& items = listItems();
    state.reserveInitialCapacity(items.size());
    for (auto& element : items) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(*element);
        if (!option || !option->selected())
            continue;
        state.append(option->value());
        if (!multiple())
            break;
    }
    return state;
}

size_t HTMLSelectElement::searchOptionsForValue(const String& value, size_t listIndexStart, size_t listIndexEnd) const
{
    auto& items = listItems();
    size_t loopEndIndex = std::min(items.size(), listIndexEnd);
    for (size_t i = listIndexStart; i < loopEndIndex; ++i) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(*items[i]);
        if (!option)
            continue;
        if (option->value() == value)
            return i;
    }
    return notFound;
}

void HTMLSelectElement::restoreFormControlState(const FormControlState& state)
{
    recalcListItems();

    auto& items = listItems();
    size_t itemsSize = items.size();
    if (!itemsSize)
        return;

    for (auto& element : items) {
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*element))
            option->setSelectedState(false);
    }

    if (!multiple()) {
        size_t foundIndex = searchOptionsForValue(state[0], 0, itemsSize);
        if (foundIndex != notFound)
            Ref { downcast<HTMLOptionElement>(*items[foundIndex]) }->setSelectedState(true);
    } else {
        size_t startIndex = 0;
        for (auto& value : state) {
            size_t foundIndex = searchOptionsForValue(value, startIndex, itemsSize);
            if (foundIndex == notFound)
                foundIndex = searchOptionsForValue(value, 0, startIndex);
            if (foundIndex == notFound)
                continue;
            Ref { downcast<HTMLOptionElement>(*items[foundIndex]) }->setSelectedState(true);
            startIndex = foundIndex + 1;
        }
    }

    invalidateSelectedItems();
    setOptionsChangedOnRenderer();
    updateValidity();
}

void HTMLSelectElement::parseMultipleAttribute(const AtomString& value)
{
    bool oldUsesMenuList = usesMenuList();
    bool oldMultiple = m_multiple;
    int oldSelectedIndex = selectedIndex();
    m_multiple = !value.isNull();
    updateValidity();
    if (oldUsesMenuList != usesMenuList())
        invalidateStyleAndRenderersForSubtree();
    if (oldMultiple != m_multiple) {
        if (oldSelectedIndex >= 0)
            setSelectedIndex(oldSelectedIndex);
        else
            reset();
    }
}

bool HTMLSelectElement::appendFormData(DOMFormData& formData)
{
    const AtomString& name = this->name();
    if (name.isEmpty())
        return false;

    bool successful = false;
    for (auto& element : listItems()) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(*element);
        if (option && option->selected() && !option->isDisabledFormControl()) {
            formData.append(name, option->value());
            successful = true;
        }
    }

    // It's possible that this is a menulist with multiple options and nothing
    // will be submitted (!successful). We won't send a unselected non-disabled
    // option as fallback. This behavior matches to other browsers.
    return successful;
} 

void HTMLSelectElement::reset()
{
    RefPtr<HTMLOptionElement> firstOption;
    RefPtr<HTMLOptionElement> selectedOption;

    for (auto& element : listItems()) {
        RefPtr option = dynamicDowncast<HTMLOptionElement>(*element);
        if (!option)
            continue;

        if (option->hasAttributeWithoutSynchronization(selectedAttr)) {
            if (selectedOption && !m_multiple)
                selectedOption->setSelectedState(false);
            option->setSelectedState(true);
            selectedOption = option;
        } else
            option->setSelectedState(false);

        if (!firstOption && !option->isDisabledFormControl())
            firstOption = WTF::move(option);
    }

    if (!selectedOption && firstOption && !m_multiple && m_size <= 1)
        firstOption->setSelectedState(true);

    setInteractedWithSinceLastFormSubmitEvent(false);
    invalidateSelectedItems();
    setOptionsChangedOnRenderer();
    invalidateStyleForSubtree();
    updateValidity();
    updateButtonText();
}

#if !PLATFORM(WIN)

bool HTMLSelectElement::platformHandleKeydownEvent(KeyboardEvent* event)
{
    if (!RenderTheme::singleton().popsMenuByArrowKeys())
        return false;

    if (!document().settings().spatialNavigationEnabled()) {
        if (event->keyIdentifier() == "Down"_s || event->keyIdentifier() == "Up"_s) {
            focus();
            protect(document())->updateStyleIfNeeded();
            // Calling focus() may cause us to lose our renderer. Return true so
            // that our caller doesn't process the event further, but don't set
            // the event as handled.
            if (!renderer() || !usesMenuList())
                return true;

            openPickerForUserInteraction();

            event->setDefaultHandled();
        }
        return true;
    }

    return false;
}

#endif

void HTMLSelectElement::menuListDefaultEventHandler(Event& event)
{
    ASSERT(renderer());
    ASSERT(usesMenuList());

    auto& eventNames = WebCore::eventNames();

    bool isBaseSelectPicker = usesBaseAppearancePicker();
    bool popoverOpen = isBaseSelectPicker && m_popover && protect(m_popover)->isPopoverShowing();

    if (event.type() == eventNames.keydownEvent) {
        RefPtr keyboardEvent = dynamicDowncast<KeyboardEvent>(event);
        if (!keyboardEvent)
            return;

        // When popover is open in base-select mode, let focused option handle navigation.
        if (popoverOpen)
            return;

        if (platformHandleKeydownEvent(keyboardEvent.get()))
            return;

        // When using spatial navigation, we want to be able to navigate away
        // from the select element when the user hits any of the arrow keys,
        // instead of changing the selection.
        if (document().settings().spatialNavigationEnabled()) {
            if (!m_activeSelectionState)
                return;
        }

        const String& keyIdentifier = keyboardEvent->keyIdentifier();
        auto& listItems = this->listItems();
        int listIndex = optionToListIndex(selectedIndex());

        // When using caret browsing, we want to be able to move the focus
        // out of the select element when user hits a left or right arrow key.
        if (document().settings().caretBrowsingEnabled()) {
            if (keyIdentifier == "Left"_s || keyIdentifier == "Right"_s)
                return;
        }

        // Menulist uses Down/Up for navigation; Right/Left are also accepted.
        listIndex = computeNavigationIndex(keyIdentifier, listIndex, { "Down"_s, "Up"_s });
        if (listIndex < 0)
            return;

        if (static_cast<size_t>(listIndex) < listItems.size())
            selectOption(listToOptionIndex(listIndex), { SelectOptionFlag::DeselectOtherOptions, SelectOptionFlag::DispatchChangeEvent, SelectOptionFlag::UserDriven });

        keyboardEvent->setDefaultHandled();
        return;
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (event.type() == eventNames.keypressEvent) {
        RefPtr keyboardEvent = dynamicDowncast<KeyboardEvent>(event);
        if (!keyboardEvent)
            return;

        // When popover is open in base-select mode, let focused option handle key presses.
        if (popoverOpen)
            return;

        int keyCode = keyboardEvent->keyCode();
        bool handled = false;

        if (keyCode == ' ' && document().settings().spatialNavigationEnabled()) {
            // Use space to toggle arrow key handling for selection change or spatial navigation.
            m_activeSelectionState = !m_activeSelectionState;
            keyboardEvent->setDefaultHandled();
            return;
        }

        if (RenderTheme::singleton().popsMenuBySpaceOrReturn()) {
            if (keyCode == ' ' || keyCode == '\r') {
                focus();
                protect(document())->updateStyleIfNeeded();

                // Calling focus() may remove the renderer or change the renderer type.
                if (!renderer() || !usesMenuList())
                    return;

                openPickerForUserInteraction();

                handled = true;
            }
        } else if (RenderTheme::singleton().popsMenuByArrowKeys()) {
            if (keyCode == ' ') {
                focus();
                protect(document())->updateStyleIfNeeded();

                // Calling focus() may remove the renderer or change the renderer type.
                if (!renderer() || !usesMenuList())
                    return;

                openPickerForUserInteraction();

                handled = true;
            } else if (keyCode == '\r') {
                if (RefPtr form = this->form())
                    form->submitImplicitly(*keyboardEvent, false);
                dispatchChangeEventForMenuList();
                handled = true;
            }
        }

        if (handled)
            keyboardEvent->setDefaultHandled();
        return;
    }

    if (RefPtr mouseEvent = dynamicDowncast<MouseEvent>(event); event.type() == eventNames.mousedownEvent && mouseEvent && mouseEvent->button() == MouseButton::Left) {
        focus();
        protect(document())->updateStyleIfNeeded();
#if !PLATFORM(IOS_FAMILY)
        if (!renderer() || !usesMenuList()) {
#else
        if (!usesBaseAppearancePicker()) {
#endif
            event.setDefaultHandled();
            return;
        }
        ASSERT(usesBaseAppearancePicker() || !m_popupIsVisible);
        if (m_popupIsVisible) {
            bool clickedInsidePopover = [&] {
                RefPtr popover = m_popover;
                if (!popover)
                    return false;
                RefPtr targetNode = dynamicDowncast<Node>(event.target());
                if (!targetNode)
                    return false;
                for (RefPtr element = dynamicDowncast<Element>(targetNode); element; element = element->parentElementInComposedTree()) {
                    if (element == popover)
                        return true;
                    if (element == this)
                        return false;
                }
                return false;
            }();
            if (!clickedInsidePopover)
                hidePickerPopoverElement();
        } else
            openPickerForUserInteraction();

        event.setDefaultHandled();
        return;
    }

#if !PLATFORM(IOS_FAMILY)
    if (event.type() == eventNames.blurEvent && !focused()) {
        if (m_popupIsVisible)
            hidePopup();
    }
#endif
}

void HTMLSelectElement::updateSelectedState(int listIndex, bool multi, bool shift)
{
    auto& items = listItems();
    int listSize = static_cast<int>(items.size());
    if (listIndex < 0 || listIndex >= listSize)
        return;

    Ref clickedElement = *items[listIndex];
    if (is<HTMLOptGroupElement>(clickedElement))
        return;

    // Save the selection so it can be compared to the new selection when
    // dispatching change events during mouseup, or after autoscroll finishes.
    saveLastSelection();

    m_activeSelectionState = true;

    bool shiftSelect = m_multiple && shift;
    bool multiSelect = m_multiple && multi && !shift;

    if (RefPtr option = dynamicDowncast<HTMLOptionElement>(clickedElement)) {
        // Keep track of whether an active selection (like during drag
        // selection), should select or deselect.
        if (option->selected() && multiSelect)
            m_activeSelectionState = false;
        if (!m_activeSelectionState)
            option->setSelectedState(false);
    }

    // If we're not in any special multiple selection mode, then deselect all
    // other items, excluding the clicked option. If no option was clicked, then
    // this will deselect all items in the list.
    if (!shiftSelect && !multiSelect)
        deselectItemsWithoutValidation(clickedElement.ptr());

    // If the anchor hasn't been set, and we're doing a single selection or a
    // shift selection, then initialize the anchor to the first selected index.
    if (m_activeSelectionAnchorIndex < 0 && !multiSelect)
        setActiveSelectionAnchorIndex(selectedIndex());

    // Set the selection state of the clicked option.
    if (RefPtr option = dynamicDowncast<HTMLOptionElement>(clickedElement); option && !option->isDisabledFormControl())
        option->setSelectedState(true);

    // If there was no selectedIndex() for the previous initialization, or If
    // we're doing a single selection, or a multiple selection (using cmd or
    // ctrl), then initialize the anchor index to the listIndex that just got
    // clicked.
    if (m_activeSelectionAnchorIndex < 0 || !shiftSelect)
        setActiveSelectionAnchorIndex(listIndex);

    invalidateSelectedItems();
    setActiveSelectionEndIndex(listIndex);
    updateListBoxSelection(!multiSelect);
}

void HTMLSelectElement::listBoxDefaultEventHandler(Event& event)
{
    auto& listItems = this->listItems();

    auto& eventNames = WebCore::eventNames();
    RefPtr mouseEvent = dynamicDowncast<MouseEvent>(event);
    RefPtr frame = document().frame();
    if (event.type() == eventNames.mousedownEvent && mouseEvent && mouseEvent->button() == MouseButton::Left) {
        focus();
        protect(document())->updateStyleIfNeeded();

        // Calling focus() may remove or change our renderer, in which case we don't want to handle the event further.
        CheckedPtr renderListBox = dynamicDowncast<RenderListBox>(this->renderer());
        if (!renderListBox)
            return;

        // Convert to coords relative to the list box if needed.
        IntPoint localOffset = roundedIntPoint(renderListBox->absoluteToLocal(mouseEvent->absoluteLocation(), UseTransforms));
        int listIndex = renderListBox->listIndexAtOffset(toIntSize(localOffset));
        if (listIndex >= 0) {
            if (!isDisabledFormControl()) {
#if PLATFORM(COCOA)
                updateSelectedState(listIndex, mouseEvent->metaKey(), mouseEvent->shiftKey());
#else
                updateSelectedState(listIndex, mouseEvent->ctrlKey(), mouseEvent->shiftKey());
#endif
            }
            if (frame)
                frame->eventHandler().setMouseDownMayStartAutoscroll();

            mouseEvent->setDefaultHandled();
        }
    } else if (event.type() == eventNames.mousemoveEvent && mouseEvent) {
        CheckedRef renderListBox = downcast<RenderListBox>(*renderer());
        if (renderListBox->canBeScrolledAndHasScrollableArea())
            return;

        if (mouseEvent->button() != MouseButton::Left || !mouseEvent->buttonDown())
            return;

        IntPoint localOffset = roundedIntPoint(renderListBox->absoluteToLocal(mouseEvent->absoluteLocation(), UseTransforms));
        int listIndex = renderListBox->listIndexAtOffset(toIntSize(localOffset));
        if (listIndex >= 0) {
            if (!isDisabledFormControl()) {
                if (m_multiple) {
                    // Only extend selection if there is something selected.
                    if (m_activeSelectionAnchorIndex < 0)
                        return;

                    setActiveSelectionEndIndex(listIndex);
                    updateListBoxSelection(false);
                } else {
                    setActiveSelectionAnchorIndex(listIndex);
                    setActiveSelectionEndIndex(listIndex);
                    updateListBoxSelection(true);
                }
            }
            mouseEvent->setDefaultHandled();
        }
    } else if (event.type() == eventNames.mouseupEvent && mouseEvent && mouseEvent->button() == MouseButton::Left && frame && frame->eventHandler().autoscrollRenderer() != renderer()) {
        // This click or drag event was not over any of the options.
        if (m_lastOnChangeSelection.isEmpty())
            return;
        // This makes sure we fire dispatchFormControlChangeEvent for a single
        // click. For drag selection, onChange will fire when the autoscroll
        // timer stops.
        listBoxOnChange();
    } else if (event.type() == eventNames.keydownEvent) {
        RefPtr keyboardEvent = dynamicDowncast<KeyboardEvent>(event);
        if (!keyboardEvent)
            return;

        CheckedPtr renderer = this->renderer();
        auto [nextKeyIdentifier, previousKeyIdentifier] = navigationKeyIdentifiersForWritingMode(renderer);

        const String& keyIdentifier = keyboardEvent->keyIdentifier();

        bool handled = false;
        int endIndex = 0;
        if (m_activeSelectionEndIndex < 0) {
            // Initialize the end index
            if (keyIdentifier == nextKeyIdentifier || keyIdentifier == "PageDown"_s) {
                int startIndex = lastSelectedListIndex();
                handled = true;
                if (keyIdentifier == nextKeyIdentifier)
                    endIndex = nextSelectableListIndex(startIndex);
                else
                    endIndex = nextSelectableListIndexPageAway(startIndex, SkipDirection::Forwards);
            } else if (keyIdentifier == previousKeyIdentifier || keyIdentifier == "PageUp"_s) {
                int startIndex = optionToListIndex(selectedIndex());
                handled = true;
                if (keyIdentifier == previousKeyIdentifier)
                    endIndex = previousSelectableListIndex(startIndex);
                else
                    endIndex = nextSelectableListIndexPageAway(startIndex, SkipDirection::Backwards);
            }
        } else {
            // Set the end index based on the current end index.
            if (keyIdentifier == nextKeyIdentifier) {
                endIndex = nextSelectableListIndex(m_activeSelectionEndIndex);
                handled = true;
            } else if (keyIdentifier == previousKeyIdentifier) {
                endIndex = previousSelectableListIndex(m_activeSelectionEndIndex);
                handled = true;
            } else if (keyIdentifier == "PageDown"_s) {
                endIndex = nextSelectableListIndexPageAway(m_activeSelectionEndIndex, SkipDirection::Forwards);
                handled = true;
            } else if (keyIdentifier == "PageUp"_s) {
                endIndex = nextSelectableListIndexPageAway(m_activeSelectionEndIndex, SkipDirection::Backwards);
                handled = true;
            }
        }
        if (keyIdentifier == "Home"_s) {
            endIndex = firstSelectableListIndex();
            handled = true;
        } else if (keyIdentifier == "End"_s) {
            endIndex = lastSelectableListIndex();
            handled = true;
        }

        if (document().settings().spatialNavigationEnabled()) {
            // Check if the selection moves to the boundary.
            if (keyIdentifier == "Left"_s || keyIdentifier == "Right"_s || ((keyIdentifier == "Down"_s || keyIdentifier == "Up"_s) && endIndex == m_activeSelectionEndIndex))
                return;
        }

        if (endIndex >= 0 && handled) {
            // Save the selection so it can be compared to the new selection
            // when dispatching change events immediately after making the new
            // selection.
            saveLastSelection();

            ASSERT_UNUSED(listItems, !listItems.size() || static_cast<size_t>(endIndex) < listItems.size());
            setActiveSelectionEndIndex(endIndex);

#if PLATFORM(COCOA)
            m_allowsNonContiguousSelection = m_multiple && document().settings().spatialNavigationEnabled();
#else
            m_allowsNonContiguousSelection = m_multiple && (document().settings().spatialNavigationEnabled() || keyboardEvent->ctrlKey());
#endif
            bool selectNewItem = keyboardEvent->shiftKey() || !m_allowsNonContiguousSelection;

            if (selectNewItem)
                m_activeSelectionState = true;
            // If the anchor is unitialized, or if we're going to deselect all
            // other options, then set the anchor index equal to the end index.
            bool deselectOthers = !m_multiple || (!keyboardEvent->shiftKey() && selectNewItem);
            if (m_activeSelectionAnchorIndex < 0 || deselectOthers) {
                if (deselectOthers)
                    deselectItemsWithoutValidation();
                setActiveSelectionAnchorIndex(m_activeSelectionEndIndex);
            }

            downcast<RenderListBox>(*renderer).scrollToRevealElementAtListIndex(endIndex);
            if (selectNewItem) {
                updateListBoxSelection(deselectOthers);
                listBoxOnChange();
            } else
                scrollToSelection();

            keyboardEvent->setDefaultHandled();
        }
    } else if (event.type() == eventNames.keypressEvent) {
        RefPtr keyboardEvent = dynamicDowncast<KeyboardEvent>(event);
        if (!keyboardEvent)
            return;

        int keyCode = keyboardEvent->keyCode();
        if (keyCode == '\r') {
            if (RefPtr form = this->form())
                form->submitImplicitly(*keyboardEvent, false);
            keyboardEvent->setDefaultHandled();
        } else if (m_multiple && keyCode == ' ' && m_allowsNonContiguousSelection) {
            // Use space to toggle selection change.
            m_activeSelectionState = !m_activeSelectionState;
            ASSERT(m_activeSelectionEndIndex >= 0);
            ASSERT(m_activeSelectionEndIndex < static_cast<int>(listItems.size()));
            ASSERT(is<HTMLOptionElement>(*listItems[m_activeSelectionEndIndex]));
            updateSelectedState(m_activeSelectionEndIndex, true /*multi*/, false /*shift*/);
            listBoxOnChange();
            keyboardEvent->setDefaultHandled();
        }
    }
}

void HTMLSelectElement::defaultEventHandler(Event& event)
{
    CheckedPtr renderer = this->renderer();
    if (!renderer)
        return;

    if (isDisabledFormControl()) {
        HTMLFormControlElement::defaultEventHandler(event);
        return;
    }

    if (usesMenuList())
        menuListDefaultEventHandler(event);
    else 
        listBoxDefaultEventHandler(event);

    if (event.defaultHandled())
        return;

    if (event.type() == eventNames().keypressEvent) {
        if (RefPtr keyboardEvent = dynamicDowncast<KeyboardEvent>(event)) {
            if (!keyboardEvent->ctrlKey() && !keyboardEvent->altKey() && !keyboardEvent->metaKey() && u_isprint(keyboardEvent->charCode())) {
                typeAheadFind(*keyboardEvent);
                event.setDefaultHandled();
                return;
            }
        }
    }
    HTMLFormControlElement::defaultEventHandler(event);
}

int HTMLSelectElement::lastSelectedListIndex() const
{
    auto& items = listItems();
    for (size_t i = items.size(); i;) {
        RefPtr element = dynamicDowncast<HTMLOptionElement>(*items[--i]);
        if (element && element->selected())
            return i;
    }
    return -1;
}

int HTMLSelectElement::indexOfSelectedOption() const
{
    return optionToListIndex(selectedIndex());
}

int HTMLSelectElement::optionCount() const
{
    return listItems().size();
}

String HTMLSelectElement::optionAtIndex(int index) const
{
    RefPtr option = dynamicDowncast<HTMLOptionElement>(*listItems()[index]);
    if (!option || option->isDisabledFormControl())
        return { };
    return option->textIndentedToRespectGroupLabel();
}

void HTMLSelectElement::typeAheadFind(KeyboardEvent& event)
{
    int index = typeAheadMatchIndex(event);
    if (index < 0)
        return;
    selectOption(listToOptionIndex(index), { SelectOptionFlag::DeselectOtherOptions, SelectOptionFlag::DispatchChangeEvent, SelectOptionFlag::UserDriven });
    if (!usesMenuListDeprecated())
        listBoxOnChange();
}

int HTMLSelectElement::typeAheadMatchIndex(KeyboardEvent& event)
{
    return m_typeAhead.handleEvent(&event, TypeAhead::MatchPrefix | TypeAhead::CycleFirstChar);
}

void HTMLSelectElement::accessKeySetSelectedIndex(int index)
{    
    // First bring into focus the list box.
    if (!focused())
        accessKeyAction(false);
    
    // If this index is already selected, unselect. otherwise update the selected index.
    auto& items = listItems();
    int listIndex = optionToListIndex(index);
    if (listIndex >= 0) {
        if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*items[listIndex])) {
            if (option->selected())
                option->setSelectedState(false);
            else
                selectOption(index, { SelectOptionFlag::DispatchChangeEvent, SelectOptionFlag::UserDriven });
        }
    }

    if (usesMenuListDeprecated())
        dispatchChangeEventForMenuList();
    else
        listBoxOnChange();

    scrollToSelection();
}

unsigned HTMLSelectElement::length() const
{
    unsigned options = 0;

    auto& items = listItems();
    for (unsigned i = 0; i < items.size(); ++i) {
        if (is<HTMLOptionElement>(*items[i]))
            ++options;
    }

    return options;
}

#if PLATFORM(IOS_FAMILY)
NO_RETURN_DUE_TO_ASSERT
void HTMLSelectElement::showPopup()
{
    ASSERT_NOT_REACHED();
}
#else
void HTMLSelectElement::showPopup()
{
    if (m_popupIsVisible)
        return;

    CheckedPtr renderer = this->renderer();
    if (!renderer || !usesMenuList())
        return;

    RefPtr frame = document().frame();
    if (!frame)
        return;

    RefPtr frameView = frame->view();
    if (!frameView)
        return;

    if (!m_popup)
        m_popup = document().page()->chrome().createPopupMenu(*this);
    setPopupIsVisible(true);

    // Compute the top left taking transforms into account, but use
    // the actual width of the element to size the popup.
    FloatPoint absTopLeft = renderer->localToAbsolute(FloatPoint(), UseTransforms);
    IntRect absBounds = renderer->absoluteBoundingBoxRectIgnoringTransforms();
    absBounds.setLocation(roundedIntPoint(absTopLeft));
    protect(m_popup)->show(absBounds, *frameView, optionToListIndex(selectedIndex())); // May run JS.
}

void HTMLSelectElement::hidePopup()
{
    if (RefPtr popup = m_popup)
        popup->hide();
}
#endif

void HTMLSelectElement::setPopupIsVisible(bool visible)
{
    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::Open, visible);
    m_popupIsVisible = visible;
}

bool HTMLSelectElement::isOpen() const
{
    return m_popupIsVisible;
}

void HTMLSelectElement::showPickerInternal()
{
    if (!usesBaseAppearancePicker()) {
#if !PLATFORM(IOS_FAMILY)
        showPopup();
#endif
        return;
    }
    if (RefPtr popover = m_popover) {
        setPopupIsVisible(true);
        popover->showPopoverInternal(this);
    }
}

void HTMLSelectElement::openPickerForUserInteraction()
{
    // Save the selection so it can be compared to the new selection when
    // dispatching change events during selectOption, which gets called from
    // RenderMenuList::valueChanged, which gets called after the user makes
    // a selection from the menu.
    saveLastSelection();
    showPickerInternal(); // May run JS and cause the renderer to get destroyed.

    if (!usesBaseAppearancePicker())
        return;

    protect(document())->updateStyleIfNeeded();
    int listIndex = optionToListIndex(selectedIndex());
    if (listIndex < 0)
        listIndex = firstSelectableListIndex();
    focusOptionAtIndex(listIndex);
}

void HTMLSelectElement::focusOptionAtIndex(int listIndex)
{
    if (!usesBaseAppearancePicker())
        return;

    auto& items = listItems();
    if (listIndex < 0 || static_cast<size_t>(listIndex) >= items.size())
        return;

    RefPtr option = dynamicDowncast<HTMLOptionElement>(items[listIndex].get());
    if (!option)
        return;

    FocusOptions focusOptions;
    focusOptions.preventScroll = false;
    option->focus(focusOptions);
}

ExceptionOr<void> HTMLSelectElement::showPicker()
{
    RefPtr frame = document().frame();
    if (!frame)
        return { };

    if (!isMutable())
        return Exception { ExceptionCode::InvalidStateError, "Select showPicker() cannot be used on immutable controls."_s };

    // In cross-origin iframes it should throw a "SecurityError" DOMException. In same-origin iframes it should work fine.
    RefPtr localTopFrame = dynamicDowncast<LocalFrame>(frame->tree().top());
    if (!localTopFrame || !protect(protect(frame->document())->securityOrigin())->isSameOriginAs(protect(protect(localTopFrame->document())->securityOrigin())))
        return Exception { ExceptionCode::SecurityError, "Select showPicker() called from cross-origin iframe."_s };

    RefPtr window = frame->window();
    if (!window || !window->consumeTransientActivation())
        return Exception { ExceptionCode::NotAllowedError, "Select showPicker() requires a user gesture."_s };

    showPickerInternal(); // showPickerInternal() may run JS and cause the renderer to get destroyed.

    return { };
}

void HTMLSelectElement::updateSelectedContent(HTMLOptionElement* selectedOption) const
{
    ASSERT(document().settings().htmlEnhancedSelectParsingEnabled());
    ASSERT(document().settings().htmlEnhancedSelectEnabled());
    ASSERT(!document().settings().mutationEventsEnabled());

    if (m_multiple || !m_selectedContentDescendantCount)
        return;

    RefPtr selectedOptionRef = selectedOption;
    if (!selectedOptionRef) {
        for (auto& element : listItems()) {
            if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*element)) {
                if (option->selected()) {
                    selectedOptionRef = option;
                    break;
                }
            }
        }
    }

    Vector<Ref<HTMLSelectedContentElement>> selectedContentElements;
    for (Ref selectedContent : descendantsOfType<HTMLSelectedContentElement>(*const_cast<HTMLSelectElement*>(this))) {
        if (!selectedContent->isDisabled())
            selectedContentElements.append(selectedContent);
    }

    for (Ref selectedContent : selectedContentElements) {
        if (!selectedOptionRef)
            selectedContent->removeChildren();
        else
            selectedOptionRef->cloneIntoSelectedContent(selectedContent);
    }
}

void HTMLSelectElement::registerSelectedContentElement()
{
    ++m_selectedContentDescendantCount;
}

void HTMLSelectElement::unregisterSelectedContentElement()
{
    ASSERT(m_selectedContentDescendantCount > 0);
    --m_selectedContentDescendantCount;
}

// PopupMenuClient methods
void HTMLSelectElement::valueChanged(unsigned listIndex, bool fireOnChange)
{
    // Check to ensure a page navigation has not occurred while
    // the popup was up.
    RefPtr frame = document().frame();
    if (!frame || &document() != frame->document())
        return;

    optionSelectedByUser(listToOptionIndex(listIndex), fireOnChange);
}

String HTMLSelectElement::itemText(unsigned listIndex) const
{
    auto& listItems = this->listItems();
    if (listIndex >= listItems.size())
        return String();

    String itemString;
    if (RefPtr optGroupElement = dynamicDowncast<HTMLOptGroupElement>(listItems[listIndex].get()))
        itemString = optGroupElement->groupLabelText();
    if (RefPtr optionElement = dynamicDowncast<HTMLOptionElement>(listItems[listIndex].get()))
        itemString = optionElement->textIndentedToRespectGroupLabel();

    if (CheckedPtr renderer = this->renderer())
        return applyTextTransform(protect(renderer->style()).get(), itemString);
    return itemString;
}

String HTMLSelectElement::itemToolTip(unsigned listIndex) const
{
    const auto& listItems = this->listItems();
    if (listIndex >= listItems.size())
        return String();

    RefPtr element = listItems[listIndex].get();
    return element ? element->title() : String();
}

String HTMLSelectElement::itemAccessibilityText(unsigned listIndex) const
{
    // Allow the accessible name be changed if necessary.
    const auto& listItems = this->listItems();
    if (listIndex >= listItems.size())
        return String();
    RefPtr element = listItems[listIndex].get();
    return element->attributeWithoutSynchronization(aria_labelAttr);
}

bool HTMLSelectElement::itemIsEnabled(unsigned listIndex) const
{
    const auto& listItems = this->listItems();
    if (listIndex >= listItems.size())
        return false;

    RefPtr element = listItems[listIndex].get();
    if (!is<HTMLOptionElement>(*element))
        return false;

    if (RefPtr parentElement = element->parentElement()) {
        if (is<HTMLOptGroupElement>(*parentElement) && parentElement->isDisabledFormControl())
            return false;
    }

    return !element->isDisabledFormControl();
}

PopupMenuStyle HTMLSelectElement::itemStyle(unsigned listIndex) const
{
    const auto& listItems = this->listItems();
    if (!listItems.size())
        return menuStyle();
    if (listIndex >= listItems.size())
        listIndex = 0;
    RefPtr element = listItems[listIndex].get();

    Color itemBackgroundColor;
    bool itemHasCustomBackgroundColor = false;
    if (CheckedPtr menuList = dynamicDowncast<RenderMenuList>(renderer()))
        menuList->getItemBackgroundColor(listIndex, itemBackgroundColor, itemHasCustomBackgroundColor);

    CheckedPtr style = element->computedStyleForEditability();
    if (!style)
        return menuStyle();

    return PopupMenuStyle(
        style->visitedDependentColorApplyingColorFilter(),
        itemBackgroundColor,
        style->fontCascade(),
        element->getAttribute(langAttr),
        style->visibility() == Visibility::Visible,
        style->display() == Style::DisplayType::None,
        true,
        style->writingMode().bidiDirection(),
        isOverride(style->unicodeBidi()),
        itemHasCustomBackgroundColor ? PopupMenuStyle::CustomBackgroundColor : PopupMenuStyle::DefaultBackgroundColor
    );
}

PopupMenuStyle HTMLSelectElement::menuStyle() const
{
    CheckedPtr renderer = this->renderer();
    ASSERT(renderer);
    if (!renderer) {
        // Fallback with minimal valid style - this shouldn't normally happen
        // since showPopup() requires a renderer
        auto defaultStyle = RenderStyle::createPtr();
        return PopupMenuStyle(
            Color::black,
            Color::white,
            defaultStyle->fontCascade(),
            nullString(),
            true,
            false,
            false,
            TextDirection::LTR,
            false
        );
    }

    CheckedRef outerStyle = renderer->style();
    auto bounds = renderer->absoluteBoundingBoxRectIgnoringTransforms();
    auto popupSize = RenderTheme::singleton().popupMenuSize(outerStyle, bounds);
    return PopupMenuStyle(
        outerStyle->visitedDependentColorApplyingColorFilter(),
        outerStyle->visitedDependentBackgroundColorApplyingColorFilter(),
        outerStyle->fontCascade(),
        nullString(),
        outerStyle->usedVisibility() == Visibility::Visible,
        outerStyle->display() == Style::DisplayType::None,
        outerStyle->hasUsedAppearance() && outerStyle->usedAppearance() == StyleAppearance::Menulist,
        outerStyle->writingMode().bidiDirection(),
        isOverride(outerStyle->unicodeBidi()),
        PopupMenuStyle::DefaultBackgroundColor,
        PopupMenuStyle::SelectPopup,
        popupSize
    );
}

int HTMLSelectElement::listSize() const
{
    return listItems().size();
}

void HTMLSelectElement::popupDidHide()
{
#if !PLATFORM(IOS_FAMILY)
    setPopupIsVisible(false);
#endif
}

bool HTMLSelectElement::itemIsSeparator(unsigned listIndex) const
{
    const auto& listItems = this->listItems();
    return listIndex < listItems.size() && listItems[listIndex]->hasTagName(hrTag);
}

bool HTMLSelectElement::itemIsLabel(unsigned listIndex) const
{
    const auto& listItems = this->listItems();
    return listIndex < listItems.size() && is<HTMLOptGroupElement>(*listItems[listIndex]);
}

bool HTMLSelectElement::itemIsSelected(unsigned listIndex) const
{
    const auto& listItems = this->listItems();
    if (listIndex >= listItems.size())
        return false;
    RefPtr option = dynamicDowncast<HTMLOptionElement>(listItems[listIndex].get());
    return option && option->selected();
}

#if !PLATFORM(COCOA)
void HTMLSelectElement::setTextFromItem(unsigned listIndex)
{
    downcast<SelectFallbackButtonElement>(*protect(m_buttonSlot)->firstChild()).updateText(nullptr, listToOptionIndex(listIndex));
}
#endif

#if PLATFORM(WIN)
int HTMLSelectElement::clientInsetLeft() const
{
    return 0;
}

int HTMLSelectElement::clientInsetRight() const
{
    return 0;
}

LayoutUnit HTMLSelectElement::clientPaddingLeft() const
{
    CheckedPtr renderer = dynamicDowncast<RenderMenuList>(this->renderer());
    return renderer ? renderer->clientPaddingLeft() : 0_lu;
}

LayoutUnit HTMLSelectElement::clientPaddingRight() const
{
    CheckedPtr renderer = dynamicDowncast<RenderMenuList>(this->renderer());
    return renderer ? renderer->clientPaddingRight() : 0_lu;
}

FontSelector* HTMLSelectElement::fontSelector() const
{
    return &protect(document())->fontSelector();
}

HostWindow* HTMLSelectElement::hostWindow() const
{
    if (renderer() && usesMenuList())
        return renderer()->hostWindow();
    return nullptr;
}
#endif

void HTMLSelectElement::didUpdateActiveOption(int optionIndex)
{
    if (!AXObjectCache::accessibilityEnabled())
        return;

    CheckedPtr axCache = protect(document())->existingAXObjectCache();
    if (!axCache)
        return;

    if (m_lastActiveIndex == optionIndex)
        return;
    m_lastActiveIndex = optionIndex;

    int listIndex = optionToListIndex(optionIndex);
    if (listIndex < 0 || listIndex >= static_cast<int>(listItems().size()))
        return;

    if (renderer())
        axCache->onSelectedOptionChanged(*this, optionIndex);
}

} // namespace WebCore
