/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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

static const int defaultRows = 2;
static const int defaultCols = 20;

// On submission, LF characters are converted into CRLF.
// This function returns number of characters considering this.
static inline unsigned computeLengthForSubmission(StringView text, unsigned numberOfLineBreaks)
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

static inline unsigned computeLengthForSubmission(StringView text)
{
    return numGraphemeClusters(text) + numberOfLineBreaks(text);
}

static inline unsigned upperBoundForLengthForSubmission(StringView text, unsigned numberOfLineBreaks)
{
    return text.length() + numberOfLineBreaks;
}

HTMLTextAreaElement::HTMLTextAreaElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : HTMLTextFormControlElement(tagName, document, form)
    , m_rows(defaultRows)
    , m_cols(defaultCols)
{
    ASSERT(hasTagName(textareaTag));
    setFormControlValueMatchesRenderer(true);
}

Ref<HTMLTextAreaElement> HTMLTextAreaElement::create(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
{
    auto textArea = adoptRef(*new HTMLTextAreaElement(tagName, document, form));
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
    static MainThreadNeverDestroyed<const AtomString> textarea("textarea", AtomString::ConstructFromLiteral);
    return textarea;
}

FormControlState HTMLTextAreaElement::saveFormControlState() const
{
    return m_isDirty ? FormControlState { { value() } } : FormControlState { };
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
        setNonDirtyValue(defaultValue());
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
        if (shouldWrapText()) {
            addPropertyToPresentationalHintStyle(style, CSSPropertyWhiteSpace, CSSValuePreWrap);
            addPropertyToPresentationalHintStyle(style, CSSPropertyWordWrap, CSSValueBreakWord);
        } else {
            addPropertyToPresentationalHintStyle(style, CSSPropertyWhiteSpace, CSSValuePre);
            addPropertyToPresentationalHintStyle(style, CSSPropertyWordWrap, CSSValueNormal);
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
        if (equalLettersIgnoringASCIICase(value, "physical") || equalLettersIgnoringASCIICase(value, "hard") || equalLettersIgnoringASCIICase(value, "on"))
            wrap = HardWrap;
        else if (equalLettersIgnoringASCIICase(value, "off"))
            wrap = NoWrap;
        else
            wrap = SoftWrap;
        if (wrap != m_wrap) {
            m_wrap = wrap;
            if (renderer())
                renderer()->setNeedsLayoutAndPrefWidthsRecalc();
        }
    } else if (name == maxlengthAttr)
        maxLengthAttributeChanged(value);
    else if (name == minlengthAttr)
        minLengthAttributeChanged(value);
    else
        HTMLTextFormControlElement::parseAttribute(name, value);
}

void HTMLTextAreaElement::maxLengthAttributeChanged(const AtomString& newValue)
{
    internalSetMaxLength(parseHTMLNonNegativeInteger(newValue).value_or(-1));
    updateValidity();
}

void HTMLTextAreaElement::minLengthAttributeChanged(const AtomString& newValue)
{
    internalSetMinLength(parseHTMLNonNegativeInteger(newValue).value_or(-1));
    updateValidity();
}

RenderPtr<RenderElement> HTMLTextAreaElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderTextControlMultiLine>(*this, WTFMove(style));
}

bool HTMLTextAreaElement::appendFormData(DOMFormData& formData, bool)
{
    if (name().isEmpty())
        return false;

    Ref<HTMLTextAreaElement> protectedThis(*this);
    document().updateLayout();

    formData.append(name(), m_wrap == HardWrap ? valueWithHardLineBreaks() : value());

    auto& dirnameAttrValue = attributeWithoutSynchronization(dirnameAttr);
    if (!dirnameAttrValue.isNull())
        formData.append(dirnameAttrValue, directionForFormData());

    return true;    
}

void HTMLTextAreaElement::reset()
{
    setNonDirtyValue(defaultValue());
}

bool HTMLTextAreaElement::hasCustomFocusLogic() const
{
    return true;
}

int HTMLTextAreaElement::defaultTabIndex() const
{
    return 0;
}

bool HTMLTextAreaElement::isKeyboardFocusable(KeyboardEvent*) const
{
    // If a given text area can be focused at all, then it will always be keyboard focusable.
    return isFocusable();
}

bool HTMLTextAreaElement::isMouseFocusable() const
{
    return isFocusable();
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

    if (!focused())
        return;

    setChangedSinceLastFormControlChangeEvent(true);

    if (RefPtr<Frame> frame = document().frame())
        frame->editor().textDidChangeInTextArea(this);
    // When typing in a textarea, childrenChanged is not called, so we need to force the directionality check.
    calculateAndAdjustDirectionality();
}

void HTMLTextAreaElement::handleBeforeTextInsertedEvent(BeforeTextInsertedEvent& event) const
{
    ASSERT(renderer());
    int signedMaxLength = effectiveMaxLength();
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
    event.setText(sanitizeUserInputValue(event.text(), appendableLength));
}

String HTMLTextAreaElement::sanitizeUserInputValue(const String& proposedValue, unsigned maxLength)
{
    return proposedValue.left(numCodeUnitsInGraphemeClusters(proposedValue, maxLength));
}

RefPtr<TextControlInnerTextElement> HTMLTextAreaElement::innerTextElement() const
{
    RefPtr<ShadowRoot> root = userAgentShadowRoot();
    if (!root)
        return nullptr;
    
    return childrenOfType<TextControlInnerTextElement>(*root).first();
}

void HTMLTextAreaElement::rendererWillBeDestroyed()
{
    updateValue();
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

void HTMLTextAreaElement::setValue(const String& value)
{
    setValueCommon(value);
    m_isDirty = true;
    updateValidity();
}

void HTMLTextAreaElement::setNonDirtyValue(const String& value)
{
    setValueCommon(value);
    m_isDirty = false;
    updateValidity();
}

void HTMLTextAreaElement::setValueCommon(const String& newValue)
{
    m_wasModifiedByUser = false;
    // Code elsewhere normalizes line endings added by the user via the keyboard or pasting.
    // We normalize line endings coming from JavaScript here.
    String normalizedValue = newValue.isNull() ? emptyString() : newValue;
    normalizedValue.replace("\r\n", "\n");
    normalizedValue.replace('\r', '\n');

    // Return early because we don't want to move the caret or trigger other side effects
    // when the value isn't changing. This matches Firefox behavior, at least.
    if (normalizedValue == value())
        return;

    m_value = normalizedValue;
    setInnerTextValue(m_value);
    setLastChangeWasNotUserEdit();
    updatePlaceholderVisibility();
    invalidateStyleForSubtree();
    setFormControlValueMatchesRenderer(true);

    // Set the caret to the end of the text value.
    if (document().focusedElement() == this) {
        unsigned endOfString = m_value.length();
        setSelectionRange(endOfString, endOfString);
    }

    setTextAsOfLastFormControlChangeEvent(normalizedValue);
}

String HTMLTextAreaElement::defaultValue() const
{
    return TextNodeTraversal::childTextContent(*this);
}

void HTMLTextAreaElement::setDefaultValue(const String& defaultValue)
{
    setTextContent(defaultValue);
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
        return validationMessageTooLongText(computeLengthForSubmission(value()), effectiveMaxLength());

    return String();
}

bool HTMLTextAreaElement::valueMissing() const
{
    return willValidate() && valueMissing(value());
}

bool HTMLTextAreaElement::tooShort() const
{
    return willValidate() && tooShort(value(), CheckDirtyFlag);
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

bool HTMLTextAreaElement::tooLong() const
{
    return willValidate() && tooLong(value(), CheckDirtyFlag);
}

bool HTMLTextAreaElement::tooLong(StringView value, NeedsToCheckDirtyFlag check) const
{
    // Return false for the default value or value set by script even if it is
    // longer than maxLength.
    if (check == CheckDirtyFlag && !m_wasModifiedByUser)
        return false;

    int max = effectiveMaxLength();
    if (max < 0)
        return false;

    // FIXME: The HTML specification says that the "number of characters" is measured using code-unit length and,
    // in the case of textarea elements, with all line breaks normalized to a single character (as opposed to CRLF pairs).
    unsigned unsignedMax = static_cast<unsigned>(max);
    unsigned numberOfLineBreaksInValue = numberOfLineBreaks(value);
    return upperBoundForLengthForSubmission(value, numberOfLineBreaksInValue) > unsignedMax
        && computeLengthForSubmission(value, numberOfLineBreaksInValue) > unsignedMax;
}

bool HTMLTextAreaElement::isValidValue(const String& candidate) const
{
    return !valueMissing(candidate) && !tooShort(candidate, IgnoreDirtyFlag) && !tooLong(candidate, IgnoreDirtyFlag);
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

bool HTMLTextAreaElement::shouldUseInputMethod()
{
    return true;
}

HTMLElement* HTMLTextAreaElement::placeholderElement() const
{
    return m_placeholder.get();
}

bool HTMLTextAreaElement::matchesReadWritePseudoClass() const
{
    return !isDisabledOrReadOnly();
}

void HTMLTextAreaElement::updatePlaceholderText()
{
    String placeholderText = strippedPlaceholder();
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
    m_placeholder->setInnerText(placeholderText);
}

bool HTMLTextAreaElement::willRespondToMouseClickEvents()
{
    return !isDisabledFormControl();
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

    setValueCommon(sourceElement.value());
    m_isDirty = sourceElement.m_isDirty;
    HTMLTextFormControlElement::copyNonAttributePropertiesFromElement(source);

    updateValidity();
}

} // namespace WebCore
