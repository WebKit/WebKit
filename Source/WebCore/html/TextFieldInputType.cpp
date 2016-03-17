/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2014 Apple Inc. All rights reserved.
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
#include "Editor.h"
#include "FormDataList.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "RenderLayer.h"
#include "RenderTextControlSingleLine.h"
#include "RenderTheme.h"
#include "ShadowRoot.h"
#include "TextBreakIterator.h"
#include "TextControlInnerElements.h"
#include "TextEvent.h"
#include "TextIterator.h"
#include "TextNodeTraversal.h"
#include "WheelEvent.h"

namespace WebCore {

using namespace HTMLNames;

TextFieldInputType::TextFieldInputType(HTMLInputElement& element)
    : InputType(element)
{
}

TextFieldInputType::~TextFieldInputType()
{
    if (m_innerSpinButton)
        m_innerSpinButton->removeSpinButtonOwner();
}

bool TextFieldInputType::isKeyboardFocusable(KeyboardEvent*) const
{
#if PLATFORM(IOS)
    if (element().isReadOnly())
        return false;
#endif
    return element().isTextFormControlFocusable();
}

bool TextFieldInputType::isMouseFocusable() const
{
    return element().isTextFormControlFocusable();
}

bool TextFieldInputType::isTextField() const
{
    return true;
}

bool TextFieldInputType::isEmptyValue() const
{
    TextControlInnerTextElement* innerText = innerTextElement();
    ASSERT(innerText);

    for (Text* text = TextNodeTraversal::firstWithin(*innerText); text; text = TextNodeTraversal::next(*text, innerText)) {
        if (text->length())
            return false;
    }
    return true;
}

bool TextFieldInputType::valueMissing(const String& value) const
{
    return element().isRequired() && value.isEmpty();
}

void TextFieldInputType::setValue(const String& sanitizedValue, bool valueChanged, TextFieldEventBehavior eventBehavior)
{
    // Grab this input element to keep reference even if JS event handler
    // changes input type.
    Ref<HTMLInputElement> input(element());

    // We don't ask InputType::setValue to dispatch events because
    // TextFieldInputType dispatches events different way from InputType.
    InputType::setValue(sanitizedValue, valueChanged, DispatchNoEvent);

    if (valueChanged)
        updateInnerTextValue();

    unsigned max = visibleValue().length();
    if (input->focused())
        input->setSelectionRange(max, max);
    else
        input->cacheSelectionInResponseToSetValue(max);

    if (!valueChanged)
        return;

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
}

void TextFieldInputType::handleKeydownEvent(KeyboardEvent* event)
{
    if (!element().focused())
        return;
    Frame* frame = element().document().frame();
    if (!frame || !frame->editor().doTextFieldCommandFromEvent(&element(), event))
        return;
    event->setDefaultHandled();
}

void TextFieldInputType::handleKeydownEventForSpinButton(KeyboardEvent* event)
{
    if (element().isDisabledOrReadOnly())
        return;
    const String& key = event->keyIdentifier();
    if (key == "Up")
        spinButtonStepUp();
    else if (key == "Down")
        spinButtonStepDown();
    else
        return;
    event->setDefaultHandled();
}

void TextFieldInputType::forwardEvent(Event* event)
{
    if (m_innerSpinButton) {
        m_innerSpinButton->forwardEvent(event);
        if (event->defaultHandled())
            return;
    }

    if (event->isMouseEvent()
        || event->type() == eventNames().blurEvent
        || event->type() == eventNames().focusEvent)
    {
        element().document().updateStyleIfNeeded();

        if (element().renderer()) {
            RenderTextControlSingleLine& renderTextControl = downcast<RenderTextControlSingleLine>(*element().renderer());
            if (event->type() == eventNames().blurEvent) {
                if (RenderTextControlInnerBlock* innerTextRenderer = innerTextElement()->renderer()) {
                    if (RenderLayer* innerLayer = innerTextRenderer->layer()) {
                        ScrollOffset scrollOffset(!renderTextControl.style().isLeftToRightDirection() ? innerLayer->scrollWidth() : 0, 0);
                        innerLayer->scrollToOffset(scrollOffset, RenderLayer::ScrollOffsetClamped);
                    }
                }

                capsLockStateMayHaveChanged();
            } else if (event->type() == eventNames().focusEvent)
                capsLockStateMayHaveChanged();

            element().forwardEvent(event);
        }
    }
}

void TextFieldInputType::handleFocusEvent(Node* oldFocusedNode, FocusDirection)
{
    ASSERT_UNUSED(oldFocusedNode, oldFocusedNode != &element());
    if (Frame* frame = element().document().frame())
        frame->editor().textFieldDidBeginEditing(&element());
}

void TextFieldInputType::handleBlurEvent()
{
    InputType::handleBlurEvent();
    element().endEditing();
}

bool TextFieldInputType::shouldSubmitImplicitly(Event* event)
{
    return (event->type() == eventNames().textInputEvent && is<TextEvent>(*event) && downcast<TextEvent>(*event).data() == "\n")
        || InputType::shouldSubmitImplicitly(event);
}

RenderPtr<RenderElement> TextFieldInputType::createInputRenderer(Ref<RenderStyle>&& style)
{
    return createRenderer<RenderTextControlSingleLine>(element(), WTFMove(style));
}

bool TextFieldInputType::needsContainer() const
{
    return false;
}

bool TextFieldInputType::shouldHaveSpinButton() const
{
    Document& document = element().document();
    RefPtr<RenderTheme> theme = document.page() ? &document.page()->theme() : RenderTheme::defaultTheme();
    return theme->shouldHaveSpinButton(element());
}

bool TextFieldInputType::shouldHaveCapsLockIndicator() const
{
    Document& document = element().document();
    RefPtr<RenderTheme> theme = document.page() ? &document.page()->theme() : RenderTheme::defaultTheme();
    return theme->shouldHaveCapsLockIndicator(element());
}

void TextFieldInputType::createShadowSubtree()
{
    ASSERT(element().shadowRoot());

    ASSERT(!m_innerText);
    ASSERT(!m_innerBlock);
    ASSERT(!m_innerSpinButton);
    ASSERT(!m_capsLockIndicator);
    ASSERT(!m_autoFillButton);

    Document& document = element().document();
    bool shouldHaveSpinButton = this->shouldHaveSpinButton();
    bool shouldHaveCapsLockIndicator = this->shouldHaveCapsLockIndicator();
    bool createsContainer = shouldHaveSpinButton || shouldHaveCapsLockIndicator || needsContainer();

    m_innerText = TextControlInnerTextElement::create(document);

    if (!createsContainer) {
        element().userAgentShadowRoot()->appendChild(*m_innerText, IGNORE_EXCEPTION);
        updatePlaceholderText();
        return;
    }

    createContainer();
    updatePlaceholderText();

    if (shouldHaveSpinButton) {
        m_innerSpinButton = SpinButtonElement::create(document, *this);
        m_container->appendChild(*m_innerSpinButton, IGNORE_EXCEPTION);
    }

    if (shouldHaveCapsLockIndicator) {
        m_capsLockIndicator = HTMLDivElement::create(document);
        m_capsLockIndicator->setPseudo(AtomicString("-webkit-caps-lock-indicator", AtomicString::ConstructFromLiteral));

        bool shouldDrawCapsLockIndicator = this->shouldDrawCapsLockIndicator();
        m_capsLockIndicator->setInlineStyleProperty(CSSPropertyDisplay, shouldDrawCapsLockIndicator ? CSSValueBlock : CSSValueNone, true);

        m_container->appendChild(*m_capsLockIndicator, IGNORE_EXCEPTION);
    }

    updateAutoFillButton();
}

HTMLElement* TextFieldInputType::containerElement() const
{
    return m_container.get();
}

HTMLElement* TextFieldInputType::innerBlockElement() const
{
    return m_innerBlock.get();
}

TextControlInnerTextElement* TextFieldInputType::innerTextElement() const
{
    ASSERT(m_innerText);
    return m_innerText.get();
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
    m_container = nullptr;
}

void TextFieldInputType::attributeChanged()
{
    // FIXME: Updating the inner text on any attribute update should
    // be unnecessary. We should figure out what attributes affect.
    updateInnerTextValue();
}

void TextFieldInputType::disabledAttributeChanged()
{
    if (m_innerSpinButton)
        m_innerSpinButton->releaseCapture();
    capsLockStateMayHaveChanged();
    updateAutoFillButton();
}

void TextFieldInputType::readonlyAttributeChanged()
{
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

static bool isASCIILineBreak(UChar c)
{
    return c == '\r' || c == '\n';
}

static String limitLength(const String& string, int maxLength)
{
    unsigned newLength = numCharactersInGraphemeClusters(string, maxLength);
    for (unsigned i = 0; i < newLength; ++i) {
        const UChar current = string[i];
        if (current < ' ' && current != '\t') {
            newLength = i;
            break;
        }
    }
    return string.left(newLength);
}

static String autoFillButtonTypeToAccessibilityLabel(AutoFillButtonType autoFillButtonType)
{
    switch (autoFillButtonType) {
    case AutoFillButtonType::Contacts:
        return AXAutoFillContactsLabel();
    case AutoFillButtonType::Credentials:
        return AXAutoFillCredentialsLabel();
    default:
    case AutoFillButtonType::None:
        ASSERT_NOT_REACHED();
        return AtomicString();
    }
}
    
static AtomicString autoFillButtonTypeToAutoFillButtonPseudoClassName(AutoFillButtonType autoFillButtonType)
{
    AtomicString pseudoClassName;
    switch (autoFillButtonType) {
    case AutoFillButtonType::Contacts:
        pseudoClassName = AtomicString("-webkit-contacts-auto-fill-button", AtomicString::ConstructFromLiteral);
        break;
    case AutoFillButtonType::Credentials:
        pseudoClassName = AtomicString("-webkit-credentials-auto-fill-button", AtomicString::ConstructFromLiteral);
        break;
    case AutoFillButtonType::None:
        ASSERT_NOT_REACHED();
        break;
    }

    return pseudoClassName;
}

static bool isAutoFillButtonTypeChanged(const AtomicString& attribute, AutoFillButtonType autoFillButtonType)
{
    if (attribute == "-webkit-contacts-auto-fill-button" && autoFillButtonType != AutoFillButtonType::Contacts)
        return true;

    if (attribute == "-webkit-credentials-auto-fill-button" && autoFillButtonType != AutoFillButtonType::Credentials)
        return true;

    return false;
}

String TextFieldInputType::sanitizeValue(const String& proposedValue) const
{
    return limitLength(proposedValue.removeCharacters(isASCIILineBreak), HTMLInputElement::maxEffectiveLength);
}

void TextFieldInputType::handleBeforeTextInsertedEvent(BeforeTextInsertedEvent* event)
{
    // Make sure that the text to be inserted will not violate the maxLength.

    // We use RenderTextControlSingleLine::text() instead of InputElement::value()
    // because they can be mismatched by sanitizeValue() in
    // HTMLInputElement::subtreeHasChanged() in some cases.
    String innerText = element().innerTextValue();
    unsigned oldLength = numGraphemeClusters(innerText);

    // selectionLength represents the selection length of this text field to be
    // removed by this insertion.
    // If the text field has no focus, we don't need to take account of the
    // selection length. The selection is the source of text drag-and-drop in
    // that case, and nothing in the text field will be removed.
    unsigned selectionLength = 0;
    if (element().focused()) {
        ASSERT(enclosingTextFormControl(element().document().frame()->selection().selection().start()) == &element());
        int selectionStart = element().selectionStart();
        ASSERT(selectionStart <= element().selectionEnd());
        int selectionCodeUnitCount = element().selectionEnd() - selectionStart;
        selectionLength = selectionCodeUnitCount ? numGraphemeClusters(innerText.substring(selectionStart, selectionCodeUnitCount)) : 0;
    }
    ASSERT(oldLength >= selectionLength);

    // Selected characters will be removed by the next text event.
    unsigned baseLength = oldLength - selectionLength;
    unsigned maxLength = isTextType() ? element().effectiveMaxLength() : HTMLInputElement::maxEffectiveLength;
    unsigned appendableLength = maxLength > baseLength ? maxLength - baseLength : 0;

    // Truncate the inserted text to avoid violating the maxLength and other constraints.
    String eventText = event->text();
    unsigned textLength = eventText.length();
    while (textLength > 0 && isASCIILineBreak(eventText[textLength - 1]))
        textLength--;
    eventText.truncate(textLength);
    eventText.replace("\r\n", " ");
    eventText.replace('\r', ' ');
    eventText.replace('\n', ' ');

    event->setText(limitLength(eventText, appendableLength));
}

bool TextFieldInputType::shouldRespectListAttribute()
{
    return InputType::themeSupportsDataListUI(this);
}

void TextFieldInputType::updatePlaceholderText()
{
    if (!supportsPlaceholder())
        return;
    String placeholderText = element().strippedPlaceholder();
    if (placeholderText.isEmpty()) {
        if (m_placeholder) {
            m_placeholder->parentNode()->removeChild(*m_placeholder, ASSERT_NO_EXCEPTION);
            m_placeholder = nullptr;
        }
        return;
    }
    if (!m_placeholder) {
        m_placeholder = TextControlPlaceholderElement::create(element().document());
        element().userAgentShadowRoot()->insertBefore(*m_placeholder, m_container ? m_container.get() : innerTextElement(), ASSERT_NO_EXCEPTION);        
    }
    m_placeholder->setInnerText(placeholderText, ASSERT_NO_EXCEPTION);
}

bool TextFieldInputType::appendFormData(FormDataList& list, bool multipart) const
{
    InputType::appendFormData(list, multipart);
    const AtomicString& dirnameAttrValue = element().fastGetAttribute(dirnameAttr);
    if (!dirnameAttrValue.isNull())
        list.appendData(dirnameAttrValue, element().directionForFormData());
    return true;
}

String TextFieldInputType::convertFromVisibleValue(const String& visibleValue) const
{
    return visibleValue;
}

void TextFieldInputType::subtreeHasChanged()
{
    element().setChangedSinceLastFormControlChangeEvent(true);

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
    String innerText = convertFromVisibleValue(element().innerTextValue());
    if (!supportsSelectionAPI())
        innerText = sanitizeValue(innerText);
    element().setValueFromRenderer(innerText);
    element().updatePlaceholderVisibility();
    // Recalc for :invalid change.
    element().setNeedsStyleRecalc();

    didSetValueByUserEdit();
}

void TextFieldInputType::didSetValueByUserEdit()
{
    if (!element().focused())
        return;
    if (Frame* frame = element().document().frame())
        frame->editor().textDidChangeInTextField(&element());
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
    if (!element().formControlValueMatchesRenderer()) {
        // Update the renderer value if the formControlValueMatchesRenderer() flag is false.
        // It protects an unacceptable renderer value from being overwritten with the DOM value.
        element().setInnerTextValue(visibleValue());
        element().updatePlaceholderVisibility();
    }
}

void TextFieldInputType::focusAndSelectSpinButtonOwner()
{
    Ref<HTMLInputElement> input(element());
    input->focus();
    input->select();
}

bool TextFieldInputType::shouldSpinButtonRespondToMouseEvents()
{
    return !element().isDisabledOrReadOnly();
}

bool TextFieldInputType::shouldSpinButtonRespondToWheelEvents()
{
    return shouldSpinButtonRespondToMouseEvents() && element().focused();
}

bool TextFieldInputType::shouldDrawCapsLockIndicator() const
{
    if (element().document().focusedElement() != &element())
        return false;

    if (element().isDisabledOrReadOnly())
        return false;

    Frame* frame = element().document().frame();
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
    return !element().isDisabledOrReadOnly() && element().autoFillButtonType() != AutoFillButtonType::None;
}

void TextFieldInputType::autoFillButtonElementWasClicked()
{
    Page* page = element().document().page();
    if (!page)
        return;

    page->chrome().client().handleAutoFillButtonClick(element());
}

void TextFieldInputType::createContainer()
{
    ASSERT(!m_container);

    m_container = TextControlInnerContainer::create(element().document());
    m_container->setPseudo(AtomicString("-webkit-textfield-decoration-container", AtomicString::ConstructFromLiteral));

    m_innerBlock = TextControlInnerElement::create(element().document());
    m_innerBlock->appendChild(*m_innerText, IGNORE_EXCEPTION);
    m_container->appendChild(*m_innerBlock, IGNORE_EXCEPTION);

    element().userAgentShadowRoot()->appendChild(*m_container, IGNORE_EXCEPTION);
}

void TextFieldInputType::createAutoFillButton(AutoFillButtonType autoFillButtonType)
{
    ASSERT(!m_autoFillButton);

    if (autoFillButtonType == AutoFillButtonType::None)
        return;

    m_autoFillButton = AutoFillButtonElement::create(element().document(), *this);
    m_autoFillButton->setPseudo(autoFillButtonTypeToAutoFillButtonPseudoClassName(autoFillButtonType));
    m_autoFillButton->setAttribute(roleAttr, "button");
    m_autoFillButton->setAttribute(aria_labelAttr, autoFillButtonTypeToAccessibilityLabel(autoFillButtonType));
    m_container->appendChild(*m_autoFillButton, IGNORE_EXCEPTION);
}

void TextFieldInputType::updateAutoFillButton()
{
    if (shouldDrawAutoFillButton()) {
        if (!m_container)
            createContainer();

        if (!m_autoFillButton)
            createAutoFillButton(element().autoFillButtonType());

        const AtomicString& attribute = m_autoFillButton->fastGetAttribute(pseudoAttr);
        bool shouldUpdateAutoFillButtonType = isAutoFillButtonTypeChanged(attribute, element().autoFillButtonType());
        if (shouldUpdateAutoFillButtonType) {
            m_autoFillButton->setPseudo(autoFillButtonTypeToAutoFillButtonPseudoClassName(element().autoFillButtonType()));
            m_autoFillButton->setAttribute(aria_labelAttr, autoFillButtonTypeToAccessibilityLabel(element().autoFillButtonType()));
        }
        m_autoFillButton->setInlineStyleProperty(CSSPropertyDisplay, CSSValueBlock, true);
        return;
    }
    
    if (m_autoFillButton)
        m_autoFillButton->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone, true);        
}

} // namespace WebCore
