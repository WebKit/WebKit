/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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
#include "DOMFormData.h"
#include "DeprecatedGlobalSettings.h"
#include "Editor.h"
#include "ElementInlines.h"
#include "ElementRareData.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderTextControlSingleLine.h"
#include "RenderTheme.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "ShadowPseudoIds.h"
#include "ShadowRoot.h"
#include "TextControlInnerElements.h"
#include "TextEvent.h"
#include "TextIterator.h"
#include "TextNodeTraversal.h"
#include "UserTypingGestureIndicator.h"
#include "WheelEvent.h"

#if ENABLE(DATALIST_ELEMENT)
#include "HTMLDataListElement.h"
#include "HTMLOptionElement.h"
#endif

namespace WebCore {

using namespace HTMLNames;

TextFieldInputType::TextFieldInputType(Type type, HTMLInputElement& element)
    : InputType(type, element)
{
    ASSERT(needsShadowSubtree());
}

TextFieldInputType::~TextFieldInputType()
{
    if (m_innerSpinButton)
        m_innerSpinButton->removeSpinButtonOwner();
#if ENABLE(DATALIST_ELEMENT)
    closeSuggestions();
#endif
}

bool TextFieldInputType::isKeyboardFocusable(KeyboardEvent*) const
{
    ASSERT(element());
#if PLATFORM(IOS_FAMILY)
    if (element()->isReadOnly())
        return false;
#endif
    return element()->isTextFormControlFocusable();
}

bool TextFieldInputType::isMouseFocusable() const
{
    ASSERT(element());
    return element()->isTextFormControlFocusable();
}

bool TextFieldInputType::isEmptyValue() const
{
    auto innerText = innerTextElement();
    if (!innerText) {
        // Since we always create the shadow subtree if a value is set, we know
        // that the value is empty.
        return true;
    }

    for (Text* text = TextNodeTraversal::firstWithin(*innerText); text; text = TextNodeTraversal::next(*text, innerText.get())) {
        if (text->length())
            return false;
    }
    return true;
}

bool TextFieldInputType::valueMissing(const String& value) const
{
    ASSERT(element());
    return element()->isMutable() && element()->isRequired() && value.isEmpty();
}

void TextFieldInputType::setValue(const String& sanitizedValue, bool valueChanged, TextFieldEventBehavior eventBehavior, TextControlSetValueSelection selection)
{
    ASSERT(element());

    // Grab this input element to keep reference even if JS event handler
    // changes input type.
    Ref<HTMLInputElement> input(*element());

    // We don't ask InputType::setValue to dispatch events because
    // TextFieldInputType dispatches events different way from InputType.
    InputType::setValue(sanitizedValue, valueChanged, DispatchNoEvent, selection);

    // Visible value needs update if it differs from sanitized value, if it was set with setValue().
    // event_behavior == DispatchNoEvent usually means this call is not a user edit.
    bool needsTextUpdate = valueChanged || (eventBehavior == TextFieldEventBehavior::DispatchNoEvent && sanitizedValue != element()->innerTextValue());
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

    // FIXME: Why do we do this when eventBehavior == DispatchNoEvent
    if (!input->focused() || eventBehavior == DispatchNoEvent)
        input->setTextAsOfLastFormControlChangeEvent(sanitizedValue);

    if (UserTypingGestureIndicator::processingUserTypingGesture())
        didSetValueByUserEdit();
}

#if ENABLE(DATALIST_ELEMENT)
void TextFieldInputType::handleClickEvent(MouseEvent&)
{
    if (element()->focused() && element()->list())
        displaySuggestions(DataListSuggestionActivationType::ControlClicked);
}
#endif

auto TextFieldInputType::handleKeydownEvent(KeyboardEvent& event) -> ShouldCallBaseEventHandler
{
    ASSERT(element());
    if (!element()->focused())
        return ShouldCallBaseEventHandler::Yes;
#if ENABLE(DATALIST_ELEMENT)
    const String& key = event.keyIdentifier();
    if (m_suggestionPicker && (key == "Enter"_s || key == "Up"_s || key == "Down"_s)) {
        m_suggestionPicker->handleKeydownWithIdentifier(key);
        event.setDefaultHandled();
    }
#endif
    RefPtr frame = element()->document().frame();
    if (frame && frame->editor().doTextFieldCommandFromEvent(*element(), &event))
        event.setDefaultHandled();
    return ShouldCallBaseEventHandler::Yes;
}

void TextFieldInputType::handleKeydownEventForSpinButton(KeyboardEvent& event)
{
    ASSERT(element());
    if (!element()->isMutable())
        return;
#if ENABLE(DATALIST_ELEMENT)
    if (m_suggestionPicker)
        return;
#endif
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

    if (m_innerSpinButton) {
        m_innerSpinButton->forwardEvent(event);
        if (event.defaultHandled())
            return;
    }

    auto& eventNames = WebCore::eventNames();
    bool isFocusEvent = event.type() == eventNames.focusEvent;
    bool isBlurEvent = event.type() == eventNames.blurEvent;
    if (isFocusEvent || isBlurEvent)
        capsLockStateMayHaveChanged();
    if (event.isMouseEvent() || isFocusEvent || isBlurEvent)
        element()->forwardEvent(event);
}

void TextFieldInputType::elementDidBlur()
{
    ASSERT(element());
    auto* renderer = element()->renderer();
    if (!renderer)
        return;

    auto* innerTextRenderer = innerTextElement()->renderer();
    if (!innerTextRenderer)
        return;

    auto* innerLayer = innerTextRenderer->layer();
    if (!innerLayer)
        return;

    auto* innerLayerScrollable = innerLayer->ensureLayerScrollableArea();

    bool isLeftToRightDirection = downcast<RenderTextControlSingleLine>(*renderer).style().isLeftToRightDirection();
    ScrollOffset scrollOffset(isLeftToRightDirection ? 0 : innerLayerScrollable->scrollWidth(), 0);
    innerLayerScrollable->scrollToOffset(scrollOffset);

#if ENABLE(DATALIST_ELEMENT)
    closeSuggestions();
#endif
}

void TextFieldInputType::handleFocusEvent(Node* oldFocusedNode, FocusDirection)
{
    ASSERT(element());
    ASSERT_UNUSED(oldFocusedNode, oldFocusedNode != element());
    if (RefPtr frame = element()->document().frame()) {
        frame->editor().textFieldDidBeginEditing(*element());
#if ENABLE(DATALIST_ELEMENT)
        if (shouldOnlyShowDataListDropdownButtonWhenFocusedOrEdited() && element()->list() && m_dataListDropdownIndicator)
            m_dataListDropdownIndicator->setInlineStyleProperty(CSSPropertyDisplay, suggestions().size() ? CSSValueBlock : CSSValueNone, true);
#endif
    }
}

void TextFieldInputType::handleBlurEvent()
{
    InputType::handleBlurEvent();
    ASSERT(element());
    element()->endEditing();
#if ENABLE(DATALIST_ELEMENT)
    if (shouldOnlyShowDataListDropdownButtonWhenFocusedOrEdited() && element()->list() && m_dataListDropdownIndicator)
        m_dataListDropdownIndicator->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone, true);
#endif
}

bool TextFieldInputType::shouldSubmitImplicitly(Event& event)
{
    return (event.type() == eventNames().textInputEvent && is<TextEvent>(event) && downcast<TextEvent>(event).data() == "\n"_s)
        || InputType::shouldSubmitImplicitly(event);
}

RenderPtr<RenderElement> TextFieldInputType::createInputRenderer(RenderStyle&& style)
{
    ASSERT(element());
    return createRenderer<RenderTextControlSingleLine>(*element(), WTFMove(style));
}

bool TextFieldInputType::needsContainer() const
{
    return false;
}

bool TextFieldInputType::shouldHaveSpinButton() const
{
    ASSERT(element());
    return RenderTheme::singleton().shouldHaveSpinButton(*element());
}

bool TextFieldInputType::shouldHaveCapsLockIndicator() const
{
    ASSERT(element());
    return RenderTheme::singleton().shouldHaveCapsLockIndicator(*element());
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

    Document& document = element()->document();
    bool shouldHaveSpinButton = this->shouldHaveSpinButton();
    bool shouldHaveCapsLockIndicator = this->shouldHaveCapsLockIndicator();
    bool shouldDrawAutoFillButton = this->shouldDrawAutoFillButton();
#if ENABLE(DATALIST_ELEMENT)
    bool hasDataList = element()->list();
#endif
    bool createsContainer = shouldHaveSpinButton || shouldHaveCapsLockIndicator || shouldDrawAutoFillButton
#if ENABLE(DATALIST_ELEMENT)
        || hasDataList
#endif
        || needsContainer();

    m_innerText = TextControlInnerTextElement::create(document, element()->isInnerTextElementEditable());

    ScriptDisallowedScope::EventAllowedScope eventAllowedScope { *element()->userAgentShadowRoot() };
    if (!createsContainer) {
        element()->userAgentShadowRoot()->appendChild(ContainerNode::ChildChange::Source::Parser, *m_innerText);
        updatePlaceholderText();
        return;
    }

    createContainer();
    updatePlaceholderText();

    if (shouldHaveSpinButton) {
        m_innerSpinButton = SpinButtonElement::create(document, *this);
        m_container->appendChild(ContainerNode::ChildChange::Source::Parser, *m_innerSpinButton);
    }

    if (shouldHaveCapsLockIndicator) {
        m_capsLockIndicator = HTMLDivElement::create(document);
        m_container->appendChild(ContainerNode::ChildChange::Source::Parser, *m_capsLockIndicator);

        m_capsLockIndicator->setPseudo(ShadowPseudoIds::webkitCapsLockIndicator());

        bool shouldDrawCapsLockIndicator = this->shouldDrawCapsLockIndicator();
        m_capsLockIndicator->setInlineStyleProperty(CSSPropertyDisplay, shouldDrawCapsLockIndicator ? CSSValueBlock : CSSValueNone, true);
    }

    updateAutoFillButton();

#if ENABLE(DATALIST_ELEMENT)
    dataListMayHaveChanged();
#endif
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

HTMLElement* TextFieldInputType::capsLockIndicatorElement() const
{
    return m_capsLockIndicator.get();
}

HTMLElement* TextFieldInputType::autoFillButtonElement() const
{
    return m_autoFillButton.get();
}

HTMLElement* TextFieldInputType::placeholderElement() const
{
    return m_placeholder.get();
}

void TextFieldInputType::destroyShadowSubtree()
{
    InputType::destroyShadowSubtree();
    m_innerText = nullptr;
    m_placeholder = nullptr;
    m_innerBlock = nullptr;
    if (m_innerSpinButton)
        m_innerSpinButton->removeSpinButtonOwner();
    m_innerSpinButton = nullptr;
    m_capsLockIndicator = nullptr;
    m_autoFillButton = nullptr;
#if ENABLE(DATALIST_ELEMENT)
    m_dataListDropdownIndicator = nullptr;
#endif
    m_container = nullptr;
}

void TextFieldInputType::attributeChanged(const QualifiedName& name)
{
    if (name == valueAttr || name == placeholderAttr) {
        if (element())
            updateInnerTextValue();
    }
    InputType::attributeChanged(name);
}

void TextFieldInputType::disabledStateChanged()
{
    if (!hasCreatedShadowSubtree())
        return;

    if (m_innerSpinButton)
        m_innerSpinButton->releaseCapture();
    capsLockStateMayHaveChanged();
    updateAutoFillButton();
}

void TextFieldInputType::readOnlyStateChanged()
{
    if (!hasCreatedShadowSubtree())
        return;

    if (m_innerSpinButton)
        m_innerSpinButton->releaseCapture();
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

#if ENABLE(DATALIST_ELEMENT)

void TextFieldInputType::createDataListDropdownIndicator()
{
    ASSERT(!m_dataListDropdownIndicator);
    if (!m_container)
        createContainer();

    ScriptDisallowedScope::EventAllowedScope allowedScope(*m_container);
    m_dataListDropdownIndicator = DataListButtonElement::create(element()->document(), *this);
    m_container->appendChild(*m_dataListDropdownIndicator);
    m_dataListDropdownIndicator->setPseudo(ShadowPseudoIds::webkitListButton());
    m_dataListDropdownIndicator->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone, true);
}

bool TextFieldInputType::shouldOnlyShowDataListDropdownButtonWhenFocusedOrEdited() const
{
#if PLATFORM(IOS_FAMILY)
#if ENABLE(IOS_FORM_CONTROL_REFRESH)
    return !element()->document().settings().iOSFormControlRefreshEnabled();
#else
    return true;
#endif
#else
    return false;
#endif
}

#endif // ENABLE(DATALIST_ELEMENT)

static String limitLength(const String& string, unsigned maxNumGraphemeClusters)
{
    StringView stringView { string };

    if (!stringView.is8Bit())
        maxNumGraphemeClusters = numCodeUnitsInGraphemeClusters(stringView, maxNumGraphemeClusters);

    return string.left(maxNumGraphemeClusters);
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
        return ShadowPseudoIds::webkitContactsAutoFillButton();
    case AutoFillButtonType::Credentials:
        return ShadowPseudoIds::webkitCredentialsAutoFillButton();
    case AutoFillButtonType::StrongPassword:
        return ShadowPseudoIds::webkitStrongPasswordAutoFillButton();
    case AutoFillButtonType::CreditCard:
        return ShadowPseudoIds::webkitCreditCardAutoFillButton();
    case AutoFillButtonType::Loading:
        return ShadowPseudoIds::webkitLoadingAutoFillButton();
    case AutoFillButtonType::None:
        ASSERT_NOT_REACHED();
        return emptyAtom();
    }
    ASSERT_NOT_REACHED();
    return { };
}

static bool isAutoFillButtonTypeChanged(const AtomString& attribute, AutoFillButtonType autoFillButtonType)
{
    if (attribute == ShadowPseudoIds::webkitContactsAutoFillButton() && autoFillButtonType != AutoFillButtonType::Contacts)
        return true;
    if (attribute == ShadowPseudoIds::webkitCredentialsAutoFillButton() && autoFillButtonType != AutoFillButtonType::Credentials)
        return true;
    if (attribute == ShadowPseudoIds::webkitStrongPasswordAutoFillButton() && autoFillButtonType != AutoFillButtonType::StrongPassword)
        return true;
    if (attribute == ShadowPseudoIds::webkitCreditCardAutoFillButton() && autoFillButtonType != AutoFillButtonType::CreditCard)
        return true;
    if (attribute == ShadowPseudoIds::webkitLoadingAutoFillButton() && autoFillButtonType != AutoFillButtonType::Loading)
        return true;
    return false;
}

String TextFieldInputType::sanitizeValue(const String& proposedValue) const
{
    return limitLength(proposedValue.removeCharacters(isHTMLLineBreak), HTMLInputElement::maxEffectiveLength);
}

void TextFieldInputType::handleBeforeTextInsertedEvent(BeforeTextInsertedEvent& event)
{
    ASSERT(element());
    // Make sure that the text to be inserted will not violate the maxLength.

    // We use RenderTextControlSingleLine::text() instead of InputElement::value()
    // because they can be mismatched by sanitizeValue() in
    // HTMLInputElement::subtreeHasChanged() in some cases.
    String innerText = element()->innerTextValue();
    unsigned oldLength = numGraphemeClusters(innerText);

    // selectionLength represents the selection length of this text field to be
    // removed by this insertion.
    // If the text field has no focus, we don't need to take account of the
    // selection length. The selection is the source of text drag-and-drop in
    // that case, and nothing in the text field will be removed.
    unsigned selectionLength = 0;
    if (element()->focused()) {
        ASSERT(enclosingTextFormControl(element()->document().frame()->selection().selection().start()) == element());
        unsigned selectionStart = element()->selectionStart();
        ASSERT(selectionStart <= element()->selectionEnd());
        int selectionCodeUnitCount = element()->selectionEnd() - selectionStart;
        selectionLength = selectionCodeUnitCount ? numGraphemeClusters(StringView(innerText).substring(selectionStart, selectionCodeUnitCount)) : 0;
    }
    ASSERT(oldLength >= selectionLength);

    // Selected characters will be removed by the next text event.
    unsigned baseLength = oldLength - selectionLength;
    unsigned maxLength = isTextType() ? element()->effectiveMaxLength() : HTMLInputElement::maxEffectiveLength;
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
#if ENABLE(DATALIST_ELEMENT)
    return DeprecatedGlobalSettings::dataListElementEnabled();
#else
    return InputType::themeSupportsDataListUI(this);
#endif
}

void TextFieldInputType::updatePlaceholderText()
{
    ASSERT(element());

    if (!hasCreatedShadowSubtree())
        return;

    if (!supportsPlaceholder())
        return;

    String placeholderText = element()->placeholder();
    if (placeholderText.isEmpty()) {
        if (m_placeholder) {
            m_placeholder->parentNode()->removeChild(*m_placeholder);
            m_placeholder = nullptr;
        }
        return;
    }
    if (!m_placeholder) {
        m_placeholder = TextControlPlaceholderElement::create(element()->document());
        element()->userAgentShadowRoot()->insertBefore(*m_placeholder, m_container ? m_container.get() : innerTextElement().get());
    }
    m_placeholder->setInnerText(WTFMove(placeholderText));
}

bool TextFieldInputType::appendFormData(DOMFormData& formData) const
{
    InputType::appendFormData(formData);
    ASSERT(element());
    auto& dirnameAttrValue = element()->attributeWithoutSynchronization(dirnameAttr);
    if (!dirnameAttrValue.isNull())
        formData.append(dirnameAttrValue, element()->directionForFormData());
    return true;
}

String TextFieldInputType::convertFromVisibleValue(const String& visibleValue) const
{
    return visibleValue;
}

void TextFieldInputType::subtreeHasChanged()
{
    ASSERT(element());
    element()->setChangedSinceLastFormControlChangeEvent(true);

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
    String innerText = convertFromVisibleValue(element()->innerTextValue());
    if (!supportsSelectionAPI())
        innerText = sanitizeValue(innerText);
    element()->setValueFromRenderer(innerText);
    element()->updatePlaceholderVisibility();
    // Recalc for :invalid change.
    element()->invalidateStyleForSubtree();

    didSetValueByUserEdit();
}

void TextFieldInputType::didSetValueByUserEdit()
{
    ASSERT(element());
    if (!element()->focused())
        return;
    if (RefPtr frame = element()->document().frame())
        frame->editor().textDidChangeInTextField(*element());
#if ENABLE(DATALIST_ELEMENT)
    if (shouldOnlyShowDataListDropdownButtonWhenFocusedOrEdited() && element()->list() && m_dataListDropdownIndicator)
        m_dataListDropdownIndicator->setInlineStyleProperty(CSSPropertyDisplay, suggestions().size() ? CSSValueBlock : CSSValueNone, true);

    if (element()->list())
        displaySuggestions(DataListSuggestionActivationType::TextChanged);
#endif
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
    if (!element()->formControlValueMatchesRenderer()) {
        // Update the renderer value if the formControlValueMatchesRenderer() flag is false.
        // It protects an unacceptable renderer value from being overwritten with the DOM value.
        element()->setInnerTextValue(visibleValue());
        element()->updatePlaceholderVisibility();
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
    return element()->isMutable();
}

bool TextFieldInputType::shouldSpinButtonRespondToWheelEvents() const
{
    ASSERT(element());
    return shouldSpinButtonRespondToMouseEvents() && element()->focused();
}

bool TextFieldInputType::shouldDrawCapsLockIndicator() const
{
    ASSERT(element());
    if (element()->document().focusedElement() != element())
        return false;

    if (!element()->isMutable())
        return false;

    if (element()->hasAutoFillStrongPasswordButton())
        return false;

    RefPtr<Frame> frame = element()->document().frame();
    if (!frame)
        return false;

    if (!frame->selection().isFocusedAndActive())
        return false;

    return PlatformKeyboardEvent::currentCapsLockState();
}

void TextFieldInputType::capsLockStateMayHaveChanged()
{
    if (!m_capsLockIndicator)
        return;

    bool shouldDrawCapsLockIndicator = this->shouldDrawCapsLockIndicator();
    m_capsLockIndicator->setInlineStyleProperty(CSSPropertyDisplay, shouldDrawCapsLockIndicator ? CSSValueBlock : CSSValueNone, true);
}

bool TextFieldInputType::shouldDrawAutoFillButton() const
{
    ASSERT(element());
    return element()->isMutable() && element()->autoFillButtonType() != AutoFillButtonType::None;
}

void TextFieldInputType::autoFillButtonElementWasClicked()
{
    ASSERT(element());
    Page* page = element()->document().page();
    if (!page)
        return;

    page->chrome().client().handleAutoFillButtonClick(*element());
}

void TextFieldInputType::createContainer()
{
    ASSERT(!m_container);
    ASSERT(element());

    static MainThreadNeverDestroyed<const AtomString> webkitTextfieldDecorationContainerName("-webkit-textfield-decoration-container"_s);

    ScriptDisallowedScope::EventAllowedScope allowedScope(*element()->userAgentShadowRoot());

    // FIXME: <https://webkit.org/b/245977> Suppress selectionchange events during subtree modification.
    std::optional<std::tuple<unsigned, unsigned, TextFieldSelectionDirection>> selectionState;
    if (enclosingTextFormControl(element()->document().selection().selection().start()) == element())
        selectionState = { element()->selectionStart(), element()->selectionEnd(), element()->computeSelectionDirection() };

    m_container = TextControlInnerContainer::create(element()->document());
    element()->userAgentShadowRoot()->appendChild(*m_container);
    m_container->setPseudo(ShadowPseudoIds::webkitTextfieldDecorationContainer());

    m_innerBlock = TextControlInnerElement::create(element()->document());
    m_container->appendChild(*m_innerBlock);
    m_innerBlock->appendChild(*m_innerText);

    if (selectionState) {
        auto [selectionStart, selectionEnd, selectionDirection] = *selectionState;
        element()->setSelectionRange(selectionStart, selectionEnd, selectionDirection);
    }
}

void TextFieldInputType::createAutoFillButton(AutoFillButtonType autoFillButtonType)
{
    ASSERT(!m_autoFillButton);

    if (autoFillButtonType == AutoFillButtonType::None)
        return;

    ASSERT(element());
    m_autoFillButton = AutoFillButtonElement::create(element()->document(), *this);
    m_container->appendChild(*m_autoFillButton);

    m_autoFillButton->setPseudo(autoFillButtonTypeToAutoFillButtonPseudoClassName(autoFillButtonType));
    m_autoFillButton->setAttributeWithoutSynchronization(roleAttr, HTMLNames::buttonTag->localName());
    m_autoFillButton->setAttributeWithoutSynchronization(aria_labelAttr, AtomString { autoFillButtonTypeToAccessibilityLabel(autoFillButtonType) });
    m_autoFillButton->setTextContent(autoFillButtonTypeToAutoFillButtonText(autoFillButtonType));
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

        AutoFillButtonType autoFillButtonType = element()->autoFillButtonType();
        if (!m_autoFillButton)
            createAutoFillButton(autoFillButtonType);

        const AtomString& attribute = m_autoFillButton->attributeWithoutSynchronization(pseudoAttr);
        bool shouldUpdateAutoFillButtonType = isAutoFillButtonTypeChanged(attribute, autoFillButtonType);
        if (shouldUpdateAutoFillButtonType) {
            m_autoFillButton->setPseudo(autoFillButtonTypeToAutoFillButtonPseudoClassName(autoFillButtonType));
            m_autoFillButton->setAttributeWithoutSynchronization(aria_labelAttr, AtomString { autoFillButtonTypeToAccessibilityLabel(autoFillButtonType) });
            m_autoFillButton->setTextContent(autoFillButtonTypeToAutoFillButtonText(autoFillButtonType));
        }
        m_autoFillButton->setInlineStyleProperty(CSSPropertyDisplay, CSSValueBlock, true);
        return;
    }
    
    if (m_autoFillButton)
        m_autoFillButton->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone, true);        
}

#if ENABLE(DATALIST_ELEMENT)

void TextFieldInputType::dataListMayHaveChanged()
{
    if (!hasCreatedShadowSubtree())
        return;

    m_cachedSuggestions = { };

    if (!m_dataListDropdownIndicator)
        createDataListDropdownIndicator();

    if (!shouldOnlyShowDataListDropdownButtonWhenFocusedOrEdited())
        m_dataListDropdownIndicator->setInlineStyleProperty(CSSPropertyDisplay, element()->list() ? CSSValueBlock : CSSValueNone, true);
}

HTMLElement* TextFieldInputType::dataListButtonElement() const
{
    return m_dataListDropdownIndicator.get();
}

void TextFieldInputType::dataListButtonElementWasClicked()
{
    Ref<HTMLInputElement> input(*element());
    if (input->list()) {
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
    return element()->document().view()->contentsToRootView(element()->renderer()->absoluteBoundingBoxRect());
}

Vector<DataListSuggestion> TextFieldInputType::suggestions()
{
    // FIXME: Suggestions are "typing completions" and so should probably use the findPlainText algorithm rather than the simplistic "ignoring ASCII case" rules.

    Vector<DataListSuggestion> suggestions;
    Vector<DataListSuggestion> matchesContainingValue;

    String elementValue = element()->value();

    if (!m_cachedSuggestions.first.isNull() && equalIgnoringASCIICase(m_cachedSuggestions.first, elementValue))
        return m_cachedSuggestions.second;

    auto* page = element()->document().page();
    bool canShowLabels = page && page->chrome().client().canShowDataListSuggestionLabels();
    if (auto dataList = element()->dataList()) {
        for (auto& option : dataList->suggestions()) {
            DataListSuggestion suggestion;
            suggestion.value = option.value();
            if (!element()->isValidValue(suggestion.value))
                continue;
            suggestion.value = sanitizeValue(suggestion.value);
            suggestion.label = option.label();
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
    element()->setValue(selectedOption, DispatchInputAndChangeEvent);
}

void TextFieldInputType::didCloseSuggestions()
{
    m_cachedSuggestions = { };
    m_suggestionPicker = nullptr;
    if (element()->renderer())
        element()->renderer()->repaint();
}

void TextFieldInputType::displaySuggestions(DataListSuggestionActivationType type)
{
    if (element()->isDisabledFormControl() || !element()->renderer())
        return;

    if (!UserGestureIndicator::processingUserGesture() && type != DataListSuggestionActivationType::TextChanged)
        return;

    if (!m_suggestionPicker && suggestions().size() > 0)
        m_suggestionPicker = chrome()->createDataListSuggestionPicker(*this);

    if (!m_suggestionPicker)
        return;

    m_suggestionPicker->displayWithActivationType(type);
}

void TextFieldInputType::closeSuggestions()
{
    if (m_suggestionPicker)
        m_suggestionPicker->close();
}

bool TextFieldInputType::isPresentingAttachedView() const
{
    return !!m_suggestionPicker;
}

bool TextFieldInputType::isFocusingWithDataListDropdown() const
{
    return m_isFocusingWithDataListDropdown;
}

#endif

} // namespace WebCore
