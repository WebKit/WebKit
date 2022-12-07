/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
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
#include "HTMLTextAreaElement.h"

#include "BeforeTextInsertedEvent.h"
#include "CSSValueKeywords.h"
#include "DOMFormData.h"
#include "Document.h"
#include "Editor.h"
#include "ElementChildIterator.h"
#include "Event.h"
#include "EventNames.h"
#include "FormController.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "LocalizedStrings.h"
#include "RenderTextControlMultiLine.h"
#include "ShadowRoot.h"
#include "Text.h"
#include "TextControlInnerElements.h"
#include "TextIterator.h"
#include "TextNodeTraversal.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLTextAreaElement);

using namespace HTMLNames;

// On submission, LF characters are converted into CRLF.
// This function returns number of characters considering this.
static unsigned computeLengthForSubmission(StringView text, unsigned numberOfLineBreaks)
{
    return numGraphemeClusters(text) + numberOfLineBreaks;
}

static unsigned numberOfLineBreaks(StringView text)
{
    unsigned length = text.length();
    unsigned count = 0;
    for (unsigned i = 0; i < length; i++) {
        if (text[i] == '\n')
            count++;
    }
    return count;
}

static unsigned computeLengthForSubmission(StringView text)
{
    return numGraphemeClusters(text) + numberOfLineBreaks(text);
}

static unsigned upperBoundForLengthForSubmission(StringView text, unsigned numberOfLineBreaks)
{
    return text.length() + numberOfLineBreaks;
}

HTMLTextAreaElement::HTMLTextAreaElement(Document& document, HTMLFormElement* form)
    : HTMLTextFormControlElement(textareaTag, document, form)
{
    setFormControlValueMatchesRenderer(true);
}

Ref<HTMLTextAreaElement> HTMLTextAreaElement::create(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
{
    ASSERT_UNUSED(tagName, tagName == textareaTag);
    auto textArea = adoptRef(*new HTMLTextAreaElement(document, form));
    textArea->ensureUserAgentShadowRoot();
    return textArea;
}

Ref<HTMLTextAreaElement> HTMLTextAreaElement::create(Document& document)
{
    return create(textareaTag, document, nullptr);
}

void HTMLTextAreaElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    root.appendChild(TextControlInnerTextElement::create(document(), isInnerTextElementEditable()));
}

const AtomString& HTMLTextAreaElement::formControlType() const
{
    return textareaTag->localName();
}

FormControlState HTMLTextAreaElement::saveFormControlState() const
{
    return m_isDirty ? FormControlState { { AtomString { value() } } } : FormControlState { };
}

void HTMLTextAreaElement::restoreFormControlState(const FormControlState& state)
{
    setValue(state[0]);
}

void HTMLTextAreaElement::childrenChanged(const ChildChange& change)
{
    HTMLElement::childrenChanged(change);
    setLastChangeWasNotUserEdit();
    if (m_isDirty)
        setInnerTextValue(value());
    else
        setNonDirtyValue(defaultValue(), TextControlSetValueSelection::Clamp);
}

bool HTMLTextAreaElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == alignAttr) {
        // Don't map 'align' attribute.  This matches what Firefox, Opera and IE do.
        // See http://bugs.webkit.org/show_bug.cgi?id=7075
        return false;
    }
    if (name == wrapAttr)
        return true;
    return HTMLTextFormControlElement::hasPresentationalHintsForAttribute(name);
}

void HTMLTextAreaElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == wrapAttr) {
        if (m_wrap != NoWrap) {
            addPropertyToPresentationalHintStyle(style, CSSPropertyWhiteSpace, CSSValuePreWrap);
            addPropertyToPresentationalHintStyle(style, CSSPropertyOverflowWrap, CSSValueBreakWord);
        } else {
            addPropertyToPresentationalHintStyle(style, CSSPropertyWhiteSpace, CSSValuePre);
            addPropertyToPresentationalHintStyle(style, CSSPropertyOverflowWrap, CSSValueNormal);
        }
    } else
        HTMLTextFormControlElement::collectPresentationalHintsForAttribute(name, value, style);
}

void HTMLTextAreaElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == rowsAttr) {
        unsigned rows = limitToOnlyHTMLNonNegativeNumbersGreaterThanZero(value, defaultRows);
        if (m_rows != rows) {
            m_rows = rows;
            if (renderer())
                renderer()->setNeedsLayoutAndPrefWidthsRecalc();
        }
    } else if (name == colsAttr) {
        unsigned cols = limitToOnlyHTMLNonNegativeNumbersGreaterThanZero(value, defaultCols);
        if (m_cols != cols) {
            m_cols = cols;
            if (renderer())
                renderer()->setNeedsLayoutAndPrefWidthsRecalc();
        }
    } else if (name == wrapAttr) {
        // The virtual/physical values were a Netscape extension of HTML 3.0, now deprecated.
        // The soft/hard /off values are a recommendation for HTML 4 extension by IE and NS 4.
        WrapMethod wrap;
        if (equalLettersIgnoringASCIICase(value, "physical"_s) || equalLettersIgnoringASCIICase(value, "hard"_s) || equalLettersIgnoringASCIICase(value, "on"_s))
            wrap = HardWrap;
        else if (equalLettersIgnoringASCIICase(value, "off"_s))
            wrap = NoWrap;
        else
            wrap = SoftWrap;
        if (wrap != m_wrap) {
            m_wrap = wrap;
            if (renderer())
                renderer()->setNeedsLayoutAndPrefWidthsRecalc();
        }
    } else if (name == maxlengthAttr) {
        internalSetMaxLength(parseHTMLNonNegativeInteger(value).value_or(-1));
        updateValidity();
    } else if (name == minlengthAttr) {
        internalSetMinLength(parseHTMLNonNegativeInteger(value).value_or(-1));
        updateValidity();
    } else
        HTMLTextFormControlElement::parseAttribute(name, value);
}

RenderPtr<RenderElement> HTMLTextAreaElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderTextControlMultiLine>(*this, WTFMove(style));
}

bool HTMLTextAreaElement::appendFormData(DOMFormData& formData)
{
    if (name().isEmpty())
        return false;

    Ref protectedThis(*this);
    document().updateLayout();

    formData.append(name(), m_wrap == HardWrap ? valueWithHardLineBreaks() : value());
    if (auto& dirname = attributeWithoutSynchronization(dirnameAttr); !dirname.isNull())
        formData.append(dirname, directionForFormData());
    return true;    
}

void HTMLTextAreaElement::reset()
{
    setNonDirtyValue(defaultValue(), TextControlSetValueSelection::SetSelectionToEnd);
}

void HTMLTextAreaElement::updateFocusAppearance(SelectionRestorationMode restorationMode, SelectionRevealMode revealMode)
{
    if (restorationMode == SelectionRestorationMode::RestoreOrSelectAll && hasCachedSelection())
        restoreCachedSelection(revealMode, Element::defaultFocusTextStateChangeIntent());
    else {
        // If this is the first focus, set a caret at the beginning of the text.  
        // This matches some browsers' behavior; see bug 11746 Comment #15.
        // http://bugs.webkit.org/show_bug.cgi?id=11746#c15
        setSelectionRange(0, 0, SelectionHasNoDirection, revealMode, Element::defaultFocusTextStateChangeIntent());
    }
}

void HTMLTextAreaElement::defaultEventHandler(Event& event)
{
    if (renderer() && (event.isMouseEvent() || event.type() == eventNames().blurEvent))
        forwardEvent(event);
    else if (renderer() && is<BeforeTextInsertedEvent>(event))
        handleBeforeTextInsertedEvent(downcast<BeforeTextInsertedEvent>(event));

    HTMLTextFormControlElement::defaultEventHandler(event);
}

void HTMLTextAreaElement::subtreeHasChanged()
{
    setFormControlValueMatchesRenderer(false);
    updateValidity();
    updatePlaceholderVisibility();

    if (!focused())
        return;

    setChangedSinceLastFormControlChangeEvent(true);

    if (RefPtr frame = document().frame())
        frame->editor().textDidChangeInTextArea(*this);
    // When typing in a textarea, childrenChanged is not called, so we need to force the directionality check.
    if (selfOrPrecedingNodesAffectDirAuto())
        updateEffectiveDirectionalityOfDirAuto();
}

void HTMLTextAreaElement::handleBeforeTextInsertedEvent(BeforeTextInsertedEvent& event) const
{
    ASSERT(renderer());
    int signedMaxLength = maxLength();
    if (signedMaxLength < 0)
        return;
    unsigned unsignedMaxLength = static_cast<unsigned>(signedMaxLength);

    const String& currentValue = innerTextValue();
    unsigned numberOfLineBreaksInCurrentValue = numberOfLineBreaks(currentValue);
    if (upperBoundForLengthForSubmission(currentValue, numberOfLineBreaksInCurrentValue)
        + upperBoundForLengthForSubmission(event.text(), numberOfLineBreaks(event.text())) < unsignedMaxLength)
        return;

    unsigned currentLength = computeLengthForSubmission(currentValue, numberOfLineBreaksInCurrentValue);
    // selectionLength represents the selection length of this text field to be
    // removed by this insertion.
    // If the text field has no focus, we don't need to take account of the
    // selection length. The selection is the source of text drag-and-drop in
    // that case, and nothing in the text field will be removed.
    auto selectionRange = focused() ? document().frame()->selection().selection().toNormalizedRange() : std::nullopt;
    unsigned selectionLength = selectionRange ? computeLengthForSubmission(plainText(*selectionRange)) : 0;
    ASSERT(currentLength >= selectionLength);
    unsigned baseLength = currentLength - selectionLength;
    unsigned appendableLength = unsignedMaxLength > baseLength ? unsignedMaxLength - baseLength : 0;
    auto text = event.text();
    event.setText(text.left(numCodeUnitsInGraphemeClusters(text, appendableLength)));
}

RefPtr<TextControlInnerTextElement> HTMLTextAreaElement::innerTextElement() const
{
    RefPtr root = userAgentShadowRoot();
    if (!root)
        return nullptr;
    return childrenOfType<TextControlInnerTextElement>(*root).first();
}

RefPtr<TextControlInnerTextElement> HTMLTextAreaElement::innerTextElementCreatingShadowSubtreeIfNeeded()
{
    return innerTextElement();
}

void HTMLTextAreaElement::updateValue() const
{
    if (formControlValueMatchesRenderer())
        return;

    ASSERT(renderer());
    m_value = innerTextValue();
    const_cast<HTMLTextAreaElement*>(this)->setFormControlValueMatchesRenderer(true);
    m_isDirty = true;
    m_wasModifiedByUser = true;
    const_cast<HTMLTextAreaElement*>(this)->updatePlaceholderVisibility();
}

String HTMLTextAreaElement::value() const
{
    updateValue();
    return m_value;
}

ExceptionOr<void> HTMLTextAreaElement::setValue(const String& value, TextFieldEventBehavior eventBehavior, TextControlSetValueSelection selection)
{
    setValueCommon(value, eventBehavior, selection);
    m_isDirty = true;
    updateValidity();
    return { };
}

void HTMLTextAreaElement::setNonDirtyValue(const String& value, TextControlSetValueSelection selection)
{
    setValueCommon(value, TextFieldEventBehavior::DispatchNoEvent, selection);
    m_isDirty = false;
    updateValidity();
}

void HTMLTextAreaElement::setValueCommon(const String& newValue, TextFieldEventBehavior, TextControlSetValueSelection selection)
{
    m_wasModifiedByUser = false;
    // Code elsewhere normalizes line endings added by the user via the keyboard or pasting.
    // We normalize line endings coming from JavaScript here.
    auto normalizedValue = newValue.isNull() ? emptyString() : makeStringBySimplifyingNewLines(newValue);

    // Return early because we don't want to move the caret or trigger other side effects
    // when the value isn't changing. This matches Firefox behavior, at least.
    if (normalizedValue == value())
        return;

    bool shouldClamp = selection == TextControlSetValueSelection::Clamp;
    auto selectionStartValue = shouldClamp ? computeSelectionStart() : 0;
    auto selectionEndValue = shouldClamp ? computeSelectionEnd() : 0;

    m_value = normalizedValue;
    setInnerTextValue(String { m_value });
    setLastChangeWasNotUserEdit();
    updatePlaceholderVisibility();
    invalidateStyleForSubtree();
    if (selfOrPrecedingNodesAffectDirAuto())
        updateEffectiveDirectionalityOfDirAuto();
    setFormControlValueMatchesRenderer(true);

    auto endOfString = m_value.length();
    if (document().focusedElement() == this)
        setSelectionRange(endOfString, endOfString);
    else if (selection == TextControlSetValueSelection::SetSelectionToEnd) {
        // We don't change text selection here but need to update caret to
        // the end of the text value except for initialize.
        cacheSelection(endOfString, endOfString, SelectionHasNoDirection);
    } else if (shouldClamp)
        cacheSelection(std::min(endOfString, selectionStartValue), std::min(endOfString, selectionEndValue), SelectionHasNoDirection);

    setTextAsOfLastFormControlChangeEvent(normalizedValue);
}

String HTMLTextAreaElement::defaultValue() const
{
    return TextNodeTraversal::childTextContent(*this);
}

void HTMLTextAreaElement::setDefaultValue(String&& defaultValue)
{
    setTextContent(WTFMove(defaultValue));
}

String HTMLTextAreaElement::validationMessage() const
{
    if (!willValidate())
        return String();

    if (customError())
        return customValidationMessage();

    if (valueMissing())
        return validationMessageValueMissingText();

    if (tooShort())
        return validationMessageTooShortText(computeLengthForSubmission(value()), minLength());

    if (tooLong())
        return validationMessageTooLongText(computeLengthForSubmission(value()), maxLength());

    return String();
}

bool HTMLTextAreaElement::valueMissing() const
{
    return willValidate() && valueMissing({ });
}

bool HTMLTextAreaElement::tooShort() const
{
    return willValidate() && tooShort({ }, CheckDirtyFlag);
}

bool HTMLTextAreaElement::tooLong() const
{
    return willValidate() && tooLong({ }, CheckDirtyFlag);
}

bool HTMLTextAreaElement::valueMissing(StringView value) const
{
    if (!(isRequired() && isMutable()))
        return false;
    if (value.isNull())
        value = this->value();
    return value.isEmpty();
}

bool HTMLTextAreaElement::isValidValue(StringView candidate) const
{
    return !valueMissing(candidate) && !tooShort(candidate, IgnoreDirtyFlag) && !tooLong(candidate, IgnoreDirtyFlag);
}

bool HTMLTextAreaElement::tooShort(StringView value, NeedsToCheckDirtyFlag check) const
{
    // Return false for the default value or value set by script even if it is
    // shorter than minLength.
    if (check == CheckDirtyFlag && !m_wasModifiedByUser)
        return false;

    int min = minLength();
    if (min <= 0)
        return false;

    if (value.isNull())
        value = this->value();

    // The empty string is excluded from tooShort validation.
    if (value.isEmpty())
        return false;

    // FIXME: The HTML specification says that the "number of characters" is measured using code-unit length and,
    // in the case of textarea elements, with all line breaks normalized to a single character (as opposed to CRLF pairs).
    unsigned unsignedMin = static_cast<unsigned>(min);
    unsigned numberOfLineBreaksInValue = numberOfLineBreaks(value);
    return upperBoundForLengthForSubmission(value, numberOfLineBreaksInValue) < unsignedMin
        && computeLengthForSubmission(value, numberOfLineBreaksInValue) < unsignedMin;
}

bool HTMLTextAreaElement::tooLong(StringView value, NeedsToCheckDirtyFlag check) const
{
    // Return false for the default value or value set by script even if it is
    // longer than maxLength.
    if (check == CheckDirtyFlag && !m_wasModifiedByUser)
        return false;

    int max = maxLength();
    if (max < 0)
        return false;

    if (value.isNull())
        value = this->value();

    // FIXME: The HTML specification says that the "number of characters" is measured using code-unit length and,
    // in the case of textarea elements, with all line breaks normalized to a single character (as opposed to CRLF pairs).
    unsigned unsignedMax = max;
    unsigned numberOfLineBreaksInValue = numberOfLineBreaks(value);
    return upperBoundForLengthForSubmission(value, numberOfLineBreaksInValue) > unsignedMax
        && computeLengthForSubmission(value, numberOfLineBreaksInValue) > unsignedMax;
}

bool HTMLTextAreaElement::accessKeyAction(bool)
{
    focus();
    return false;
}

void HTMLTextAreaElement::setCols(unsigned cols)
{
    setUnsignedIntegralAttribute(colsAttr, limitToOnlyHTMLNonNegativeNumbersGreaterThanZero(cols, defaultCols));
}

void HTMLTextAreaElement::setRows(unsigned rows)
{
    setUnsignedIntegralAttribute(rowsAttr, limitToOnlyHTMLNonNegativeNumbersGreaterThanZero(rows, defaultRows));
}

void HTMLTextAreaElement::updatePlaceholderText()
{
    auto& placeholderText = attributeWithoutSynchronization(placeholderAttr);
    if (placeholderText.isEmpty()) {
        if (m_placeholder) {
            userAgentShadowRoot()->removeChild(*m_placeholder);
            m_placeholder = nullptr;
        }
        return;
    }
    if (!m_placeholder) {
        m_placeholder = TextControlPlaceholderElement::create(document());
        userAgentShadowRoot()->insertBefore(*m_placeholder, innerTextElement()->nextSibling());
    }
    m_placeholder->setInnerText(String { placeholderText });
}

RenderStyle HTMLTextAreaElement::createInnerTextStyle(const RenderStyle& style)
{
    auto textBlockStyle = RenderStyle::create();
    textBlockStyle.inheritFrom(style);
    adjustInnerTextStyle(style, textBlockStyle);
    textBlockStyle.setDisplay(DisplayType::Block);
    return textBlockStyle;
}

void HTMLTextAreaElement::copyNonAttributePropertiesFromElement(const Element& source)
{
    auto& sourceElement = downcast<HTMLTextAreaElement>(source);

    setValueCommon(sourceElement.value(), DispatchNoEvent, TextControlSetValueSelection::DoNotSet);
    m_isDirty = sourceElement.m_isDirty;
    HTMLTextFormControlElement::copyNonAttributePropertiesFromElement(source);

    updateValidity();
}

} // namespace WebCore
