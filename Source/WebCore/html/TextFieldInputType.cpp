/*
 * Copyright (C) 2010-2013 Google Inc. All rights reserved.
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextFieldInputType.h"

#include "BeforeTextInsertedEvent.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ContainerNodeInlines.h"
#include "DOMFormData.h"
#include "DocumentEventLoop.h"
#include "DocumentPage.h"
#include "DocumentView.h"
#include "Editor.h"
#include "ElementRareData.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameSelection.h"
#include "HTMLDataListElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
#include "KeyboardEvent.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "LocalizedStrings.h"
#include "NodeRenderStyle.h"
#include "PlatformKeyboardEvent.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderTextControlSingleLine.h"
#include "RenderTheme.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "TextControlInnerElements.h"
#include "TextEvent.h"
#include "TextIterator.h"
#include "TextNodeTraversal.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "UserAgentParts.h"
#include "UserGestureIndicator.h"
#include "UserTypingGestureIndicator.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextFieldInputType);

using namespace HTMLNames;

TextFieldInputType::TextFieldInputType(Type type, HTMLInputElement& element)
    : InputType(type, element)
{
    ASSERT(needsShadowSubtree());
}

TextFieldInputType::~TextFieldInputType()
{
    closeSuggestions();
    if (RefPtr suggestionPicker = m_suggestionPicker)
        suggestionPicker->detach();
}

bool TextFieldInputType::isKeyboardFocusable(const FocusEventData&) const
{
    ASSERT(element());
#if PLATFORM(IOS_FAMILY)
    if (element()->isReadOnly())
        return false;
#endif
    return protectedElement()->isTextFormControlFocusable();
}

bool TextFieldInputType::isMouseFocusable() const
{
    ASSERT(element());
    return protectedElement()->isTextFormControlFocusable();
}

bool TextFieldInputType::isEmptyValue() const
{
    auto innerText = innerTextElement();
    if (!innerText) {
        if (element()->shadowRoot())
            return true; // Shadow tree is empty
        return visibleValue().isEmpty();
    }

    for (RefPtr text = TextNodeTraversal::firstWithin(*innerText); text; text = TextNodeTraversal::next(*text, innerText.get())) {
        if (text->length())
            return false;
    }
    return true;
}

bool TextFieldInputType::valueMissing(const String& value) const
{
    ASSERT(element());
    Ref element = *this->element();
    return element->isMutable() && element->isRequired() && value.isEmpty();
}

void TextFieldInputType::setValue(const String& sanitizedValue, bool valueChanged, TextFieldEventBehavior eventBehavior, TextControlSetValueSelection selection)
{
    ASSERT(element());

    // Grab this input element to keep reference even if JS event handler
    // changes input type.
    Ref input = *element();

    // We don't ask InputType::setValue to dispatch events because
    // TextFieldInputType dispatches events different way from InputType.
    InputType::setValue(sanitizedValue, valueChanged, DispatchNoEvent, selection);

    // Visible value needs update if it differs from sanitized value, if it was set with setValue().
    // event_behavior == DispatchNoEvent usually means this call is not a user edit.
    bool needsTextUpdate = valueChanged || (eventBehavior == TextFieldEventBehavior::DispatchNoEvent && sanitizedValue != input->innerTextValue());
    if (needsTextUpdate)
        updateInnerTextValue();
    if (!valueChanged)
        return;

    if (selection == TextControlSetValueSelection::SetSelectionToEnd) {
        auto max = visibleValue().length();
        if (input->focused())
            input->setSelectionRange(max, max);
        else
            input->cacheSelectionInResponseToSetValue(max);
    }

    switch (eventBehavior) {
    case DispatchChangeEvent:
        // If the user is still editing this field, dispatch an input event rather than a change event.
        // The change event will be dispatched when editing finishes.
        if (input->focused())
            input->dispatchFormControlInputEvent();
        else
            input->dispatchFormControlChangeEvent();
        break;

    case DispatchInputAndChangeEvent: {
        input->dispatchFormControlInputEvent();
        input->dispatchFormControlChangeEvent();
        break;
    }

    case DispatchNoEvent:
        break;
    }

    if (!input->focused())
        input->setTextAsOfLastFormControlChangeEvent(String(sanitizedValue));

    if (UserTypingGestureIndicator::processingUserTypingGesture())
        didSetValueByUserEdit();
}

void TextFieldInputType::handleClickEvent(MouseEvent&)
{
    Ref element = *this->element();
    if (element->focused() && element->hasDataList())
        displaySuggestions(DataListSuggestionActivationType::ControlClicked);
}

void TextFieldInputType::showPicker()
{
#if !PLATFORM(IOS_FAMILY)
    if (protectedElement()->hasDataList())
        displaySuggestions(DataListSuggestionActivationType::ControlClicked);
#endif
}

auto TextFieldInputType::handleKeydownEvent(KeyboardEvent& event) -> ShouldCallBaseEventHandler
{
    ASSERT(element());
    Ref element = *this->element();
    if (!element->focused())
        return ShouldCallBaseEventHandler::Yes;
    const String& key = event.keyIdentifier();
    if (RefPtr suggestionPicker = m_suggestionPicker; suggestionPicker && (key == "Enter"_s || key == "Up"_s || key == "Down"_s || key == "U+001B"_s)) {
        suggestionPicker->handleKeydownWithIdentifier(key);
        event.setDefaultHandled();
    }
    RefPtr frame = element->document().frame();
    if (frame && frame->protectedEditor()->doTextFieldCommandFromEvent(element.get(), &event))
        event.setDefaultHandled();
    return ShouldCallBaseEventHandler::Yes;
}

void TextFieldInputType::handleKeydownEventForSpinButton(KeyboardEvent& event)
{
    ASSERT(element());
    if (!protectedElement()->isMutable())
        return;
    if (m_suggestionPicker)
        return;
    const String& key = event.keyIdentifier();
    if (key == "Up"_s)
        spinButtonStepUp();
    else if (key == "Down"_s)
        spinButtonStepDown();
    else
        return;
    event.setDefaultHandled();
}

void TextFieldInputType::forwardEvent(Event& event)
{
    ASSERT(element());

    auto& eventNames = WebCore::eventNames();
    bool isFocusEvent = event.type() == eventNames.focusEvent;
    bool isBlurEvent = event.type() == eventNames.blurEvent;
    if (isFocusEvent || isBlurEvent)
        capsLockStateMayHaveChanged();
    if (event.isMouseEvent() || isFocusEvent || isBlurEvent)
        protectedElement()->forwardEvent(event);
}

void TextFieldInputType::elementDidBlur()
{
    ASSERT(element());
    CheckedPtr renderer = element()->renderer();
    if (!renderer)
        return;

    CheckedPtr innerTextRenderer = innerTextElement()->renderer();
    if (!innerTextRenderer)
        return;

    CheckedPtr innerLayer = innerTextRenderer->layer();
    if (!innerLayer)
        return;

    CheckedPtr innerLayerScrollable = innerLayer->ensureLayerScrollableArea();

    bool isLeftToRightDirection = downcast<RenderTextControlSingleLine>(*renderer).style().isLeftToRightDirection();
    ScrollOffset scrollOffset(isLeftToRightDirection ? 0 : innerLayerScrollable->scrollWidth(), 0);
    innerLayerScrollable->scrollToOffset(scrollOffset);

    closeSuggestions();
}

void TextFieldInputType::handleFocusEvent(Node* oldFocusedNode, FocusDirection)
{
    ASSERT(element());
    ASSERT_UNUSED(oldFocusedNode, oldFocusedNode != element());
    Ref element = *this->element();
    if (RefPtr frame = element->document().frame())
        frame->protectedEditor()->textFieldDidBeginEditing(element.get());
}

void TextFieldInputType::handleBlurEvent()
{
    InputType::handleBlurEvent();
    ASSERT(element());
    protectedElement()->endEditing();
}

bool TextFieldInputType::shouldSubmitImplicitly(Event& event)
{
    auto* textEvent = dynamicDowncast<TextEvent>(event);
    return (event.type() == eventNames().textInputEvent && textEvent && textEvent->data() == "\n"_s)
        || InputType::shouldSubmitImplicitly(event);
}

RenderPtr<RenderElement> TextFieldInputType::createInputRenderer(RenderStyle&& style)
{
    ASSERT(element());
    // FIXME: https://github.com/llvm/llvm-project/pull/142471 Moving style is not unsafe.
    SUPPRESS_UNCOUNTED_ARG return createRenderer<RenderTextControlSingleLine>(RenderObject::Type::TextControlSingleLine, *protectedElement(), WTFMove(style));
}

bool TextFieldInputType::needsContainer() const
{
    return false;
}

bool TextFieldInputType::shouldHaveSpinButton() const
{
    ASSERT(element());
    return RenderTheme::singleton().shouldHaveSpinButton(*protectedElement());
}

bool TextFieldInputType::shouldHaveCapsLockIndicator() const
{
    ASSERT(element());
    return RenderTheme::singleton().shouldHaveCapsLockIndicator(*protectedElement());
}

void TextFieldInputType::createShadowSubtree()
{
    ASSERT(needsShadowSubtree());
    ASSERT(element());
    ASSERT(element()->shadowRoot());
    ASSERT(!element()->shadowRoot()->hasChildNodes());

    ASSERT(!m_innerText);
    ASSERT(!m_innerBlock);
    ASSERT(!m_innerSpinButton);
    ASSERT(!m_capsLockIndicator);
    ASSERT(!m_autoFillButton);

    Ref element = *this->element();
    Ref document = element->document();
    bool shouldHaveSpinButton = this->shouldHaveSpinButton();
    bool shouldHaveCapsLockIndicator = this->shouldHaveCapsLockIndicator();
    bool shouldDrawAutoFillButton = this->shouldDrawAutoFillButton();
    bool hasDataList = element->hasDataList();
    bool createsContainer = shouldHaveSpinButton || shouldHaveCapsLockIndicator || shouldDrawAutoFillButton || hasDataList || needsContainer();

    Ref innerText = TextControlInnerTextElement::create(document, element->isInnerTextElementEditable());
    m_innerText = innerText.copyRef();

    Ref shadowRoot = *element->userAgentShadowRoot();
    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { shadowRoot };
    if (!createsContainer) {
        shadowRoot->appendChild(ContainerNode::ChildChange::Source::Parser, innerText);
        updatePlaceholderText();
        updateInnerTextValue();
        return;
    }

    createContainer(PreserveSelectionRange::No);
    updatePlaceholderText();
    updateInnerTextValue();

    if (shouldHaveSpinButton) {
        Ref innerSpinButton = SpinButtonElement::create(document, *this);
        m_innerSpinButton = innerSpinButton.copyRef();
        RefPtr { m_container }->appendChild(ContainerNode::ChildChange::Source::Parser, innerSpinButton);
    }

    if (shouldHaveCapsLockIndicator) {
        Ref capsLockIndicator = HTMLDivElement::create(document);
        m_capsLockIndicator = capsLockIndicator.copyRef();
        RefPtr { m_container }->appendChild(ContainerNode::ChildChange::Source::Parser, capsLockIndicator);

        capsLockIndicator->setUserAgentPart(UserAgentParts::webkitCapsLockIndicator());

        bool shouldDrawCapsLockIndicator = this->shouldDrawCapsLockIndicator();
        capsLockIndicator->setInlineStyleProperty(CSSPropertyDisplay, shouldDrawCapsLockIndicator ? CSSValueBlock : CSSValueNone, IsImportant::Yes);
    }

    updateAutoFillButton();

    dataListMayHaveChanged();
}

HTMLElement* TextFieldInputType::containerElement() const
{
    return m_container.get();
}

HTMLElement* TextFieldInputType::innerBlockElement() const
{
    return m_innerBlock.get();
}

RefPtr<TextControlInnerTextElement> TextFieldInputType::innerTextElement() const
{
    return m_innerText;
}

HTMLElement* TextFieldInputType::innerSpinButtonElement() const
{
    return m_innerSpinButton.get();
}

HTMLElement* TextFieldInputType::autoFillButtonElement() const
{
    return m_autoFillButton.get();
}

HTMLElement* TextFieldInputType::placeholderElement() const
{
    return m_placeholder.get();
}

void TextFieldInputType::removeShadowSubtree()
{
    InputType::removeShadowSubtree();
    m_innerText = nullptr;
    m_placeholder = nullptr;
    m_innerBlock = nullptr;
    if (RefPtr innerSpinButton = m_innerSpinButton)
        innerSpinButton->removeSpinButtonOwner();
    m_innerSpinButton = nullptr;
    m_capsLockIndicator = nullptr;
    m_autoFillButton = nullptr;
    m_dataListDropdownIndicator = nullptr;
    m_container = nullptr;
}

void TextFieldInputType::attributeChanged(const QualifiedName& name)
{
    if (name == valueAttr || name == placeholderAttr) {
        if (element() && element()->shadowRoot())
            updateInnerTextValue();
    }
    InputType::attributeChanged(name);
}

void TextFieldInputType::disabledStateChanged()
{
    if (!hasCreatedShadowSubtree())
        return;

    if (RefPtr innerSpinButton = m_innerSpinButton)
        innerSpinButton->releaseCapture();
    capsLockStateMayHaveChanged();
    updateAutoFillButton();
}

void TextFieldInputType::readOnlyStateChanged()
{
    if (!hasCreatedShadowSubtree())
        return;

    if (RefPtr innerSpinButton = m_innerSpinButton)
        innerSpinButton->releaseCapture();
    capsLockStateMayHaveChanged();
    updateAutoFillButton();
}

bool TextFieldInputType::supportsReadOnly() const
{
    return true;
}

bool TextFieldInputType::shouldUseInputMethod() const
{
    return true;
}

void TextFieldInputType::createDataListDropdownIndicator()
{
    ASSERT(!m_dataListDropdownIndicator);
    if (!m_container)
        createContainer();
    if (!element())
        return;
    
    ScriptDisallowedScope::EventAllowedScope allowedScope(*m_container);
    Ref dataListDropdownIndicator = DataListButtonElement::create(element()->protectedDocument().get(), *this);
    m_dataListDropdownIndicator = dataListDropdownIndicator.copyRef();
    RefPtr { m_container }->appendChild(dataListDropdownIndicator);
    dataListDropdownIndicator->setUserAgentPart(UserAgentParts::webkitListButton());
    dataListDropdownIndicator->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone, IsImportant::Yes);
}

static ValueOrReference<String> limitLength(const String& string LIFETIME_BOUND, unsigned maxLength)
{
    if (string.length() <= maxLength) [[likely]]
        return string;

    unsigned newLength = maxLength;
    if (newLength > 0 && U16_IS_LEAD(string[newLength - 1]))
        --newLength;
    return string.left(newLength);
}

static String autoFillButtonTypeToAccessibilityLabel(AutoFillButtonType autoFillButtonType)
{
    switch (autoFillButtonType) {
    case AutoFillButtonType::Contacts:
        return AXAutoFillContactsLabel();
    case AutoFillButtonType::Credentials:
        return AXAutoFillCredentialsLabel();
    case AutoFillButtonType::StrongPassword:
        return AXAutoFillStrongPasswordLabel();
    case AutoFillButtonType::CreditCard:
        return AXAutoFillCreditCardLabel();
    case AutoFillButtonType::Loading:
        return AXAutoFillLoadingLabel();
    case AutoFillButtonType::None:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

static String autoFillButtonTypeToAutoFillButtonText(AutoFillButtonType autoFillButtonType)
{
    switch (autoFillButtonType) {
    case AutoFillButtonType::Contacts:
    case AutoFillButtonType::Credentials:
    case AutoFillButtonType::CreditCard:
    case AutoFillButtonType::Loading:
        return emptyString();
    case AutoFillButtonType::StrongPassword:
        return autoFillStrongPasswordLabel();
    case AutoFillButtonType::None:
        ASSERT_NOT_REACHED();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

static AtomString autoFillButtonTypeToAutoFillButtonPseudoClassName(AutoFillButtonType autoFillButtonType)
{
    switch (autoFillButtonType) {
    case AutoFillButtonType::Contacts:
        return UserAgentParts::webkitContactsAutoFillButton();
    case AutoFillButtonType::Credentials:
        return UserAgentParts::webkitCredentialsAutoFillButton();
    case AutoFillButtonType::StrongPassword:
        return UserAgentParts::webkitStrongPasswordAutoFillButton();
    case AutoFillButtonType::CreditCard:
        return UserAgentParts::webkitCreditCardAutoFillButton();
    case AutoFillButtonType::Loading:
        return UserAgentParts::internalLoadingAutoFillButton();
    case AutoFillButtonType::None:
        ASSERT_NOT_REACHED();
        return emptyAtom();
    }
    ASSERT_NOT_REACHED();
    return { };
}

static bool isAutoFillButtonTypeChanged(const AtomString& attribute, AutoFillButtonType autoFillButtonType)
{
    if (attribute == UserAgentParts::webkitContactsAutoFillButton() && autoFillButtonType != AutoFillButtonType::Contacts)
        return true;
    if (attribute == UserAgentParts::webkitCredentialsAutoFillButton() && autoFillButtonType != AutoFillButtonType::Credentials)
        return true;
    if (attribute == UserAgentParts::webkitStrongPasswordAutoFillButton() && autoFillButtonType != AutoFillButtonType::StrongPassword)
        return true;
    if (attribute == UserAgentParts::webkitCreditCardAutoFillButton() && autoFillButtonType != AutoFillButtonType::CreditCard)
        return true;
    if (attribute == UserAgentParts::internalLoadingAutoFillButton() && autoFillButtonType != AutoFillButtonType::Loading)
        return true;
    return false;
}

ValueOrReference<String> TextFieldInputType::sanitizeValue(const String& proposedValue LIFETIME_BOUND) const
{
    if (!containsHTMLLineBreak(proposedValue)) [[likely]]
        return limitLength(proposedValue, HTMLInputElement::maxEffectiveLength);

    // Passing a lambda instead of a function name helps the compiler inline isHTMLLineBreak.
    auto proposedValueWithoutLineBreaks = proposedValue.removeCharacters([](auto character) {
        return isHTMLLineBreak(character);
    });
    return String(limitLength(proposedValueWithoutLineBreaks, HTMLInputElement::maxEffectiveLength));
}

void TextFieldInputType::handleBeforeTextInsertedEvent(BeforeTextInsertedEvent& event)
{
    ASSERT(element());
    Ref element = *this->element();
    // Make sure that the text to be inserted will not violate the maxLength.

    // We use RenderTextControlSingleLine::text() instead of InputElement::value()
    // because they can be mismatched by sanitizeValue() in
    // HTMLInputElement::subtreeHasChanged() in some cases.
    auto innerText = element->innerTextValue();
    unsigned oldLength = innerText.length();

    // selectionLength represents the selection length of this text field to be
    // removed by this insertion.
    // If the text field has no focus, we don't need to take account of the
    // selection length. The selection is the source of text drag-and-drop in
    // that case, and nothing in the text field will be removed.
    unsigned selectionLength = 0;
    if (element->focused()) {
        ASSERT(enclosingTextFormControl(element->document().frame()->selection().selection().start()) == element.ptr());
        unsigned selectionStart = element->selectionStart();
        ASSERT(selectionStart <= element->selectionEnd());
        selectionLength = element->selectionEnd() - selectionStart;
    }
    ASSERT(oldLength >= selectionLength);

    // Selected characters will be removed by the next text event.
    unsigned baseLength = oldLength - selectionLength;
    unsigned maxLength = isTextType() ? element->effectiveMaxLength() : HTMLInputElement::maxEffectiveLength;
    unsigned appendableLength = maxLength > baseLength ? maxLength - baseLength : 0;

    // Truncate the inserted text to avoid violating the maxLength and other constraints.
    // FIXME: This may cause a lot of String allocations in the worst case scenario.
    String eventText = event.text();
    unsigned textLength = eventText.length();
    while (textLength > 0 && isHTMLLineBreak(eventText[textLength - 1]))
        textLength--;
    eventText = makeStringByReplacingAll(eventText.left(textLength), "\r\n"_s, " "_s);
    eventText = makeStringByReplacingAll(eventText, '\r', ' ');
    eventText = makeStringByReplacingAll(eventText, '\n', ' ');
    event.setText(limitLength(eventText, appendableLength));
}

bool TextFieldInputType::shouldRespectListAttribute()
{
    return element() && element()->document().settings().dataListElementEnabled();
}

void TextFieldInputType::updatePlaceholderText()
{
    ASSERT(element());

    if (!hasCreatedShadowSubtree())
        return;

    if (!supportsPlaceholder())
        return;

    Ref element = *this->element();
    auto placeholderText = element->placeholder();
    if (placeholderText.isEmpty()) {
        if (RefPtr placeholder = std::exchange(m_placeholder, nullptr))
            placeholder->remove();
        return;
    }
    if (!m_placeholder) {
        Ref placeholder = TextControlPlaceholderElement::create(element->protectedDocument());
        m_placeholder = placeholder.copyRef();
        if (RefPtr container = m_container)
            element->protectedUserAgentShadowRoot()->insertBefore(placeholder, container);
        else
            element->protectedUserAgentShadowRoot()->insertBefore(placeholder, innerTextElement());
    }
    RefPtr { m_placeholder }->setInnerText(WTFMove(placeholderText));
}

bool TextFieldInputType::appendFormData(DOMFormData& formData) const
{
    InputType::appendFormData(formData);
    ASSERT(element());
    Ref element = *this->element();
    // FIXME: should type=number be TextFieldInputType to begin with?
    if (element->isNumberField())
        return true;
    if (auto& dirname = element->attributeWithoutSynchronization(dirnameAttr); !dirname.isNull())
        formData.append(dirname, element->directionForFormData());
    return true;
}

String TextFieldInputType::convertFromVisibleValue(const String& visibleValue) const
{
    return visibleValue;
}

void TextFieldInputType::subtreeHasChanged()
{
    ASSERT(element());
    Ref element = *this->element();
    element->setChangedSinceLastFormControlChangeEvent(true);

    // We don't need to call sanitizeUserInputValue() function here because
    // HTMLInputElement::handleBeforeTextInsertedEvent() has already called
    // sanitizeUserInputValue().
    // ---
    // sanitizeValue() is needed because IME input doesn't dispatch BeforeTextInsertedEvent.
    // ---
    // Input types that support the selection API do *not* sanitize their
    // user input in order to retain parity between what's in the model and
    // what's on the screen. Otherwise, we retain the sanitization process for
    // backward compatibility. https://bugs.webkit.org/show_bug.cgi?id=150346
    auto innerText = convertFromVisibleValue(element->innerTextValue());
    if (!supportsSelectionAPI())
        innerText = sanitizeValue(innerText);
    element->setValueFromRenderer(innerText);
    element->updatePlaceholderVisibility();
    // Recalc for :invalid change.
    element->invalidateStyleForSubtree();

    didSetValueByUserEdit();
}

void TextFieldInputType::didSetValueByUserEdit()
{
    ASSERT(element());
    Ref element = *this->element();
    if (!element->focused())
        return;
    if (RefPtr frame = element->document().frame())
        frame->protectedEditor()->textDidChangeInTextField(element.get());
    if (element->hasDataList())
        displaySuggestions(DataListSuggestionActivationType::TextChanged);
}

void TextFieldInputType::spinButtonStepDown()
{
    stepUpFromRenderer(-1);
}

void TextFieldInputType::spinButtonStepUp()
{
    stepUpFromRenderer(1);
}

void TextFieldInputType::updateInnerTextValue()
{
    ASSERT(element());
    Ref element = *this->element();
    if (!element->formControlValueMatchesRenderer()) {
        // Update the renderer value if the formControlValueMatchesRenderer() flag is false.
        // It protects an unacceptable renderer value from being overwritten with the DOM value.
        element->setInnerTextValue(visibleValue());
        element->updatePlaceholderVisibility();
    }
}

void TextFieldInputType::focusAndSelectSpinButtonOwner()
{
    ASSERT(element());
    Ref<HTMLInputElement> input(*element());
    input->focus();
    input->select();
}

bool TextFieldInputType::shouldSpinButtonRespondToMouseEvents() const
{
    ASSERT(element());
    return protectedElement()->isMutable();
}

bool TextFieldInputType::shouldDrawCapsLockIndicator() const
{
    ASSERT(element());
    Ref element = *this->element();
    if (element->document().focusedElement() != element.ptr())
        return false;

    if (!element->isMutable())
        return false;

    if (element->hasAutofillStrongPasswordButton())
        return false;

    RefPtr frame = element->document().frame();
    if (!frame)
        return false;

    if (!frame->checkedSelection()->isFocusedAndActive())
        return false;

    return PlatformKeyboardEvent::currentCapsLockState();
}

void TextFieldInputType::capsLockStateMayHaveChanged()
{
    RefPtr capsLockIndicator = m_capsLockIndicator;
    if (!capsLockIndicator)
        return;

    bool shouldDrawCapsLockIndicator = this->shouldDrawCapsLockIndicator();
    capsLockIndicator->setInlineStyleProperty(CSSPropertyDisplay, shouldDrawCapsLockIndicator ? CSSValueBlock : CSSValueNone, IsImportant::Yes);
}

bool TextFieldInputType::shouldDrawAutoFillButton() const
{
    ASSERT(element());
    Ref element = *this->element();
    return element->isMutable() && element->autofillButtonType() != AutoFillButtonType::None;
}

void TextFieldInputType::autoFillButtonElementWasClicked()
{
    RefPtr element = this->element();
    ASSERT(element);
    RefPtr page = element->document().page();
    if (!page)
        return;

    auto event = Event::create(eventNames().webkitautofillrequestEvent, Event::CanBubble::No, Event::IsCancelable::No, Event::IsComposed::Yes);
    event->setIsAutofillEvent();
    element->dispatchEvent(WTFMove(event));

    page->chrome().client().handleAutoFillButtonClick(*element);
}

void TextFieldInputType::createContainer(PreserveSelectionRange preserveSelection)
{
    ASSERT(!m_container);
    ASSERT(element());

    Ref element = *this->element();
    Ref shadowRoot = *element->userAgentShadowRoot();
    ScriptDisallowedScope::EventAllowedScope allowedScope(shadowRoot);

    Ref document = element->document();
    // FIXME: <https://webkit.org/b/245977> Suppress selectionchange events during subtree modification.
    std::optional<std::tuple<unsigned, unsigned, TextFieldSelectionDirection>> selectionState;
    if (preserveSelection == PreserveSelectionRange::Yes && enclosingTextFormControl(document->selection().selection().start()) == element.ptr())
        selectionState = { element->selectionStart(), element->selectionEnd(), element->computeSelectionDirection() };

    Ref container = TextControlInnerContainer::create(document);
    m_container = container.copyRef();
    shadowRoot->appendChild(container);
    container->setUserAgentPart(UserAgentParts::webkitTextfieldDecorationContainer());

    Ref innerBlock = TextControlInnerElement::create(document);
    m_innerBlock = innerBlock.copyRef();
    RefPtr { m_container }->appendChild(innerBlock);
    innerBlock->appendChild(Ref { *m_innerText });

    if (selectionState) {
        document->checkedEventLoop()->queueTask(TaskSource::DOMManipulation, [selectionState = *selectionState, element = WeakPtr { element }] {
            if (!element || !element->focused())
                return;

            auto selection = element->document().selection().selection();
            if (selection.start().deprecatedNode() != element->userAgentShadowRoot())
                return;

            auto& [selectionStart, selectionEnd, selectionDirection] = selectionState;
            element->setSelectionRange(selectionStart, selectionEnd, selectionDirection);
        });
    }
}

void TextFieldInputType::createAutoFillButton(AutoFillButtonType autoFillButtonType)
{
    ASSERT(!m_autoFillButton);

    if (autoFillButtonType == AutoFillButtonType::None)
        return;

    ASSERT(element());
    Ref autoFillButton = AutoFillButtonElement::create(element()->protectedDocument().get(), *this);
    m_autoFillButton = autoFillButton.copyRef();
    RefPtr { m_container }->appendChild(autoFillButton);

    autoFillButton->setUserAgentPart(autoFillButtonTypeToAutoFillButtonPseudoClassName(autoFillButtonType));
    autoFillButton->setAttributeWithoutSynchronization(roleAttr, HTMLNames::buttonTag->localName());
    autoFillButton->setAttributeWithoutSynchronization(aria_labelAttr, AtomString { autoFillButtonTypeToAccessibilityLabel(autoFillButtonType) });
    autoFillButton->setTextContent(autoFillButtonTypeToAutoFillButtonText(autoFillButtonType));
}

void TextFieldInputType::updateAutoFillButton()
{
    ASSERT(element());

    if (!hasCreatedShadowSubtree())
        return;

    capsLockStateMayHaveChanged();

    if (shouldDrawAutoFillButton()) {
        if (!m_container)
            createContainer();

        auto autoFillButtonType = element()->autofillButtonType();
        if (!m_autoFillButton)
            createAutoFillButton(autoFillButtonType);
        Ref autoFillButton = *m_autoFillButton;

        auto part = autoFillButton->userAgentPart();
        bool shouldUpdateAutoFillButtonType = isAutoFillButtonTypeChanged(part, autoFillButtonType);
        if (shouldUpdateAutoFillButtonType) {
            autoFillButton->setUserAgentPart(autoFillButtonTypeToAutoFillButtonPseudoClassName(autoFillButtonType));
            autoFillButton->setAttributeWithoutSynchronization(aria_labelAttr, AtomString { autoFillButtonTypeToAccessibilityLabel(autoFillButtonType) });
            autoFillButton->setTextContent(autoFillButtonTypeToAutoFillButtonText(autoFillButtonType));
        }
        autoFillButton->setInlineStyleProperty(CSSPropertyDisplay, CSSValueBlock, IsImportant::Yes);
        return;
    }
    
    if (RefPtr autoFillButton = m_autoFillButton)
        autoFillButton->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone, IsImportant::Yes);
}

void TextFieldInputType::dataListMayHaveChanged()
{
    if (!hasCreatedShadowSubtree())
        return;

    m_cachedSuggestions = { };

    if (!m_dataListDropdownIndicator)
        createDataListDropdownIndicator();
    RefPtr element = this->element();
    if (!element)
        return;
    RefPtr { m_dataListDropdownIndicator }->setInlineStyleProperty(CSSPropertyDisplay, element->list() ? CSSValueBlock : CSSValueNone, IsImportant::Yes);
    if (element->hasDataList() && element->focused())
        displaySuggestions(DataListSuggestionActivationType::DataListMayHaveChanged);
}

HTMLElement* TextFieldInputType::dataListButtonElement() const
{
    return m_dataListDropdownIndicator.get();
}

void TextFieldInputType::dataListButtonElementWasClicked()
{
    Ref<HTMLInputElement> input(*element());
    if (input->hasDataList()) {
        m_isFocusingWithDataListDropdown = true;
        unsigned max = visibleValue().length();
        input->setSelectionRange(max, max);
        m_isFocusingWithDataListDropdown = false;

        displaySuggestions(DataListSuggestionActivationType::IndicatorClicked);
    }
}

IntRect TextFieldInputType::elementRectInRootViewCoordinates() const
{
    if (!element()->renderer())
        return IntRect();
    Ref element = *this->element();
    return element->protectedDocument()->protectedView()->contentsToRootView(element->checkedRenderer()->absoluteBoundingBoxRect());
}

Vector<DataListSuggestion> TextFieldInputType::suggestions()
{
    // FIXME: Suggestions are "typing completions" and so should probably use the findPlainText algorithm rather than the simplistic "ignoring ASCII case" rules.

    Vector<DataListSuggestion> suggestions;
    Vector<DataListSuggestion> matchesContainingValue;

    Ref element = *this->element();
    String elementValue = element->value();

    if (!m_cachedSuggestions.first.isNull() && equalIgnoringASCIICase(m_cachedSuggestions.first, elementValue))
        return m_cachedSuggestions.second;

    RefPtr page = element->document().page();
    bool canShowLabels = page && page->chrome().client().canShowDataListSuggestionLabels();
    if (RefPtr dataList = element->dataList()) {
        for (Ref option : dataList->suggestions()) {
            DataListSuggestion suggestion;
            suggestion.value = option->value();
            if (!element->isValidValue(suggestion.value))
                continue;
            suggestion.value = sanitizeValue(suggestion.value);
            suggestion.label = option->label();
            if (suggestion.value == suggestion.label)
                suggestion.label = { };

            if (elementValue.isEmpty() || suggestion.value.startsWithIgnoringASCIICase(elementValue))
                suggestions.append(WTFMove(suggestion));
            else if (suggestion.value.containsIgnoringASCIICase(elementValue) || (canShowLabels && suggestion.label.containsIgnoringASCIICase(elementValue)))
                matchesContainingValue.append(WTFMove(suggestion));
        }
    }

    suggestions.appendVector(WTFMove(matchesContainingValue));
    m_cachedSuggestions = std::make_pair(elementValue, suggestions);

    return suggestions;
}

void TextFieldInputType::didSelectDataListOption(const String& selectedOption)
{
    protectedElement()->setValue(selectedOption, DispatchInputAndChangeEvent);
}

void TextFieldInputType::didCloseSuggestions()
{
    m_cachedSuggestions = { };
    if (RefPtr suggestionPicker = std::exchange(m_suggestionPicker, nullptr))
        suggestionPicker->detach();
    if (CheckedPtr renderer = element()->renderer())
        renderer->repaint();
}

void TextFieldInputType::displaySuggestions(DataListSuggestionActivationType type)
{
    if (element()->isDisabledFormControl() || !element()->renderer())
        return;

    if (!UserGestureIndicator::processingUserGesture() && !(type == DataListSuggestionActivationType::TextChanged || type == DataListSuggestionActivationType::DataListMayHaveChanged))
        return;

    if (!m_suggestionPicker && suggestions().size() > 0)
        m_suggestionPicker = chrome()->createDataListSuggestionPicker(*this);

    if (RefPtr suggestionPicker = m_suggestionPicker)
        suggestionPicker->displayWithActivationType(type);
}

void TextFieldInputType::closeSuggestions()
{
    if (RefPtr suggestionPicker = m_suggestionPicker)
        suggestionPicker->close();
}

bool TextFieldInputType::isPresentingAttachedView() const
{
    return !!m_suggestionPicker;
}

bool TextFieldInputType::isFocusingWithDataListDropdown() const
{
    return m_isFocusingWithDataListDropdown;
}

} // namespace WebCore
