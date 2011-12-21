/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "HTMLInputElement.h"

#include "AXObjectCache.h"
#include "BeforeTextInsertedEvent.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FileList.h"
#include "Frame.h"
#include "HTMLCollection.h"
#include "HTMLDataListElement.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
#include "InputType.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "NumberInputType.h"
#include "RenderTextControlSingleLine.h"
#include "RenderTheme.h"
#include "SearchInputType.h"
#include "ScriptEventListener.h"
#include "WheelEvent.h"
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(INPUT_COLOR)
#include "ColorInputType.h"
#endif

#if ENABLE(INPUT_SPEECH)
#include "RuntimeEnabledFeatures.h"
#endif

using namespace std;

namespace WebCore {

using namespace HTMLNames;

// FIXME: According to HTML4, the length attribute's value can be arbitrarily
// large. However, due to https://bugs.webkit.org/show_bug.cgi?id=14536 things
// get rather sluggish when a text field has a larger number of characters than
// this, even when just clicking in the text field.
const int HTMLInputElement::maximumLength = 524288;
const int defaultSize = 20;
const int maxSavedResults = 256;

HTMLInputElement::HTMLInputElement(const QualifiedName& tagName, Document* document, HTMLFormElement* form, bool createdByParser)
    : HTMLTextFormControlElement(tagName, document, form)
    , m_size(defaultSize)
    , m_maxLength(maximumLength)
    , m_maxResults(-1)
    , m_isChecked(false)
    , m_reflectsCheckedAttribute(true)
    , m_isIndeterminate(false)
    , m_hasType(false)
    , m_isActivatedSubmit(false)
    , m_autocomplete(Uninitialized)
    , m_isAutofilled(false)
    , m_stateRestored(false)
    , m_parsingInProgress(createdByParser)
    , m_wasModifiedByUser(false)
    , m_canReceiveDroppedFiles(false)
    , m_inputType(InputType::createText(this))
{
    ASSERT(hasTagName(inputTag) || hasTagName(isindexTag));
}

PassRefPtr<HTMLInputElement> HTMLInputElement::create(const QualifiedName& tagName, Document* document, HTMLFormElement* form, bool createdByParser)
{
    RefPtr<HTMLInputElement> inputElement = adoptRef(new HTMLInputElement(tagName, document, form, createdByParser));
    inputElement->createShadowSubtree();
    return inputElement.release();
}

void HTMLInputElement::createShadowSubtree()
{
    m_inputType->createShadowSubtree();
}

HTMLInputElement::~HTMLInputElement()
{
    if (needsSuspensionCallback())
        document()->unregisterForPageCacheSuspensionCallbacks(this);

    document()->checkedRadioButtons().removeButton(this);

    // Need to remove this from the form while it is still an HTMLInputElement,
    // so can't wait for the base class's destructor to do it.
    removeFromForm();
}

const AtomicString& HTMLInputElement::formControlName() const
{
    return m_name.isNull() ? emptyAtom : m_name;
}

HTMLElement* HTMLInputElement::containerElement() const
{
    return m_inputType->containerElement();
}

HTMLElement* HTMLInputElement::innerTextElement() const
{
    return m_inputType->innerTextElement();
}

HTMLElement* HTMLInputElement::innerBlockElement() const
{
    return m_inputType->innerBlockElement();
}

HTMLElement* HTMLInputElement::innerSpinButtonElement() const
{
    return m_inputType->innerSpinButtonElement();
}

HTMLElement* HTMLInputElement::resultsButtonElement() const
{
    return m_inputType->resultsButtonElement();
}

HTMLElement* HTMLInputElement::cancelButtonElement() const
{
    return m_inputType->cancelButtonElement();
}

#if ENABLE(INPUT_SPEECH)
HTMLElement* HTMLInputElement::speechButtonElement() const
{
    return m_inputType->speechButtonElement();
}
#endif

HTMLElement* HTMLInputElement::placeholderElement() const
{
    return m_inputType->placeholderElement();
}

bool HTMLInputElement::shouldAutocomplete() const
{
    if (m_autocomplete != Uninitialized)
        return m_autocomplete == On;
    return HTMLTextFormControlElement::shouldAutocomplete();
}

void HTMLInputElement::updateCheckedRadioButtons()
{
    if (attached() && checked())
        checkedRadioButtons().addButton(this);

    if (form()) {
        const Vector<FormAssociatedElement*>& controls = form()->associatedElements();
        for (unsigned i = 0; i < controls.size(); ++i) {
            if (!controls[i]->isFormControlElement())
                continue;
            HTMLFormControlElement* control = static_cast<HTMLFormControlElement*>(controls[i]);
            if (control->name() != name())
                continue;
            if (control->type() != type())
                continue;
            control->setNeedsValidityCheck();
        }
    } else {
        typedef Document::FormElementListHashSet::const_iterator Iterator;
        Iterator end = document()->getFormElements()->end();
        for (Iterator it = document()->getFormElements()->begin(); it != end; ++it) {
            Element* element = *it;
            if (element->formControlName() != name())
                continue;
            if (element->formControlType() != type())
                continue;
            HTMLFormControlElement* control = static_cast<HTMLFormControlElement*>(element);
            if (control->form())
                continue;
            control->setNeedsValidityCheck();
        }
    }
}

bool HTMLInputElement::isValidValue(const String& value) const
{
    if (!m_inputType->canSetStringValue()) {
        ASSERT_NOT_REACHED();
        return false;
    }
    return !m_inputType->typeMismatchFor(value)
        && !stepMismatch(value)
        && !rangeUnderflow(value)
        && !rangeOverflow(value)
        && !tooLong(value, IgnoreDirtyFlag)
        && !patternMismatch(value)
        && !valueMissing(value);
}

bool HTMLInputElement::typeMismatch() const
{
    return m_inputType->typeMismatch();
}

bool HTMLInputElement::valueMissing(const String& value) const
{
    if (!isRequiredFormControl() || readOnly() || disabled())
        return false;
    return m_inputType->valueMissing(value);
}

bool HTMLInputElement::patternMismatch(const String& value) const
{
    return m_inputType->patternMismatch(value);
}

bool HTMLInputElement::tooLong(const String& value, NeedsToCheckDirtyFlag check) const
{
    // We use isTextType() instead of supportsMaxLength() because of the
    // 'virtual' overhead.
    if (!isTextType())
        return false;
    int max = maxLength();
    if (max < 0)
        return false;
    if (check == CheckDirtyFlag) {
        // Return false for the default value or a value set by a script even if
        // it is longer than maxLength.
        if (!hasDirtyValue() || !m_wasModifiedByUser)
            return false;
    }
    return numGraphemeClusters(value) > static_cast<unsigned>(max);
}

bool HTMLInputElement::rangeUnderflow(const String& value) const
{
    return m_inputType->rangeUnderflow(value);
}

bool HTMLInputElement::rangeOverflow(const String& value) const
{
    return m_inputType->rangeOverflow(value);
}

double HTMLInputElement::minimum() const
{
    return m_inputType->minimum();
}

double HTMLInputElement::maximum() const
{
    return m_inputType->maximum();
}

bool HTMLInputElement::stepMismatch(const String& value) const
{
    double step;
    if (!getAllowedValueStep(&step))
        return false;
    return m_inputType->stepMismatch(value, step);
}

String HTMLInputElement::minimumString() const
{
    return m_inputType->serialize(minimum());
}

String HTMLInputElement::maximumString() const
{
    return m_inputType->serialize(maximum());
}

String HTMLInputElement::stepBaseString() const
{
    return m_inputType->serialize(m_inputType->stepBase());
}

String HTMLInputElement::stepString() const
{
    double step;
    if (!getAllowedValueStep(&step)) {
        // stepString() should be called only if stepMismatch() can be true.
        ASSERT_NOT_REACHED();
        return String();
    }
    return serializeForNumberType(step / m_inputType->stepScaleFactor());
}

String HTMLInputElement::typeMismatchText() const
{
    return m_inputType->typeMismatchText();
}

String HTMLInputElement::valueMissingText() const
{
    return m_inputType->valueMissingText();
}

bool HTMLInputElement::getAllowedValueStep(double* step) const
{
    return getAllowedValueStepWithDecimalPlaces(RejectAny, step, 0);
}

bool HTMLInputElement::getAllowedValueStepWithDecimalPlaces(AnyStepHandling anyStepHandling, double* step, unsigned* decimalPlaces) const
{
    ASSERT(step);
    double defaultStep = m_inputType->defaultStep();
    double stepScaleFactor = m_inputType->stepScaleFactor();
    if (!isfinite(defaultStep) || !isfinite(stepScaleFactor))
        return false;
    const AtomicString& stepString = fastGetAttribute(stepAttr);
    if (stepString.isEmpty()) {
        *step = defaultStep * stepScaleFactor;
        if (decimalPlaces)
            *decimalPlaces = 0;
        return true;
    }

    if (equalIgnoringCase(stepString, "any")) {
        switch (anyStepHandling) {
        case RejectAny:
            return false;
        case AnyIsDefaultStep:
            *step = defaultStep * stepScaleFactor;
            if (decimalPlaces)
                *decimalPlaces = 0;
            return true;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    double parsed;
    if (!decimalPlaces) {
        if (!parseToDoubleForNumberType(stepString, &parsed) || parsed <= 0.0) {
            *step = defaultStep * stepScaleFactor;
            return true;
        }
    } else {
        if (!parseToDoubleForNumberTypeWithDecimalPlaces(stepString, &parsed, decimalPlaces) || parsed <= 0.0) {
            *step = defaultStep * stepScaleFactor;
            *decimalPlaces = 0;
            return true;
        }
    }
    // For date, month, week, the parsed value should be an integer for some types.
    if (m_inputType->parsedStepValueShouldBeInteger())
        parsed = max(round(parsed), 1.0);
    double result = parsed * stepScaleFactor;
    // For datetime, datetime-local, time, the result should be an integer.
    if (m_inputType->scaledStepValueShouldBeInteger())
        result = max(round(result), 1.0);
    ASSERT(result > 0);
    *step = result;
    return true;
}

void HTMLInputElement::applyStep(double count, AnyStepHandling anyStepHandling, bool sendChangeEvent, ExceptionCode& ec)
{
    double step;
    unsigned stepDecimalPlaces, currentDecimalPlaces;
    if (!getAllowedValueStepWithDecimalPlaces(anyStepHandling, &step, &stepDecimalPlaces)) {
        ec = INVALID_STATE_ERR;
        return;
    }

    const double nan = numeric_limits<double>::quiet_NaN();
    double current = m_inputType->parseToDoubleWithDecimalPlaces(value(), nan, &currentDecimalPlaces);
    if (!isfinite(current)) {
        ec = INVALID_STATE_ERR;
        return;
    }
    double newValue = current + step * count;
    if (isinf(newValue)) {
        ec = INVALID_STATE_ERR;
        return;
    }

    double acceptableError = m_inputType->acceptableError(step);
    if (newValue - m_inputType->minimum() < -acceptableError) {
        ec = INVALID_STATE_ERR;
        return;
    }
    if (newValue < m_inputType->minimum())
        newValue = m_inputType->minimum();

    const AtomicString& stepString = fastGetAttribute(stepAttr);
    if (!equalIgnoringCase(stepString, "any"))
        newValue = alignValueForStep(newValue, step, currentDecimalPlaces, stepDecimalPlaces);

    if (newValue - m_inputType->maximum() > acceptableError) {
        ec = INVALID_STATE_ERR;
        return;
    }
    if (newValue > m_inputType->maximum())
        newValue = m_inputType->maximum();

    setValueAsNumber(newValue, ec, sendChangeEvent);

    if (AXObjectCache::accessibilityEnabled())
         document()->axObjectCache()->postNotification(renderer(), AXObjectCache::AXValueChanged, true);
}

double HTMLInputElement::alignValueForStep(double newValue, double step, unsigned currentDecimalPlaces, unsigned stepDecimalPlaces)
{
    if (newValue >= pow(10.0, 21.0))
        return newValue;

    unsigned baseDecimalPlaces;
    double base = m_inputType->stepBaseWithDecimalPlaces(&baseDecimalPlaces);
    baseDecimalPlaces = min(baseDecimalPlaces, 16u);
    if (stepMismatch(value())) {
        double scale = pow(10.0, static_cast<double>(max(stepDecimalPlaces, currentDecimalPlaces)));
        newValue = round(newValue * scale) / scale;
    } else {
        double scale = pow(10.0, static_cast<double>(max(stepDecimalPlaces, baseDecimalPlaces)));
        newValue = round((base + round((newValue - base) / step) * step) * scale) / scale;
    }

    return newValue;
}

void HTMLInputElement::stepUp(int n, ExceptionCode& ec)
{
    bool sendChangeEvent = false;
    applyStep(n, RejectAny, sendChangeEvent, ec);
}

void HTMLInputElement::stepDown(int n, ExceptionCode& ec)
{
    bool sendChangeEvent = false;
    applyStep(-n, RejectAny, sendChangeEvent, ec);
}

bool HTMLInputElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (isTextField())
        return HTMLTextFormControlElement::isFocusable();
    return HTMLTextFormControlElement::isKeyboardFocusable(event) && m_inputType->isKeyboardFocusable();
}

bool HTMLInputElement::isMouseFocusable() const
{
    if (isTextField())
        return HTMLTextFormControlElement::isFocusable();
    return HTMLTextFormControlElement::isMouseFocusable();
}

void HTMLInputElement::updateFocusAppearance(bool restorePreviousSelection)
{
    if (isTextField()) {
        if (!restorePreviousSelection || !hasCachedSelection())
            select();
        else
            restoreCachedSelection();
        if (document()->frame())
            document()->frame()->selection()->revealSelection();
    } else
        HTMLTextFormControlElement::updateFocusAppearance(restorePreviousSelection);
}

void HTMLInputElement::aboutToUnload()
{
    if (!isTextField() || !focused())
        return;

    Frame* frame = document()->frame();
    if (!frame)
        return;

    frame->editor()->textFieldDidEndEditing(this);
}

bool HTMLInputElement::shouldUseInputMethod()
{
    return m_inputType->shouldUseInputMethod();
}

void HTMLInputElement::handleFocusEvent()
{
    if (!isTextField())
        return;
    if (isPasswordField() && document()->frame())
        document()->setUseSecureKeyboardEntryWhenActive(true);
}

void HTMLInputElement::handleBlurEvent()
{
    m_inputType->handleBlurEvent();
    if (!isTextField())
        return;
    Frame* frame = document()->frame();
    if (!frame)
        return;
    if (isPasswordField())
        document()->setUseSecureKeyboardEntryWhenActive(false);
    frame->editor()->textFieldDidEndEditing(this);
}

void HTMLInputElement::setType(const String& type)
{
    // FIXME: This should just call setAttribute. No reason to handle the empty string specially.
    // We should write a test case to show that setting to the empty string does not remove the
    // attribute in other browsers and then fix this. Note that setting to null *does* remove
    // the attribute and setAttribute implements that.
    if (type.isEmpty())
        removeAttribute(typeAttr);
    else
        setAttribute(typeAttr, type);
}

void HTMLInputElement::updateType()
{
    OwnPtr<InputType> newType = InputType::create(this, fastGetAttribute(typeAttr));
    bool hadType = m_hasType;
    m_hasType = true;
    if (m_inputType->formControlType() == newType->formControlType())
        return;

    if (hadType && !newType->canChangeFromAnotherType()) {
        // Set the attribute back to the old value.
        // Useful in case we were called from inside parseMappedAttribute.
        setAttribute(typeAttr, type());
        return;
    }

    checkedRadioButtons().removeButton(this);

    bool wasAttached = attached();
    if (wasAttached)
        detach();

    bool didStoreValue = m_inputType->storesValueSeparateFromAttribute();
    bool neededSuspensionCallback = needsSuspensionCallback();
    bool didRespectHeightAndWidth = m_inputType->shouldRespectHeightAndWidthAttributes();

    m_inputType->destroyShadowSubtree();
    m_inputType = newType.release();
    m_inputType->createShadowSubtree();

    setNeedsWillValidateCheck();

    bool willStoreValue = m_inputType->storesValueSeparateFromAttribute();

    if (didStoreValue && !willStoreValue && hasDirtyValue()) {
        setAttribute(valueAttr, m_valueIfDirty);
        m_valueIfDirty = String();
    }
    if (!didStoreValue && willStoreValue)
        m_valueIfDirty = sanitizeValue(fastGetAttribute(valueAttr));
    else
        updateValueIfNeeded();

    setFormControlValueMatchesRenderer(false);
    updateInnerTextValue();

    m_wasModifiedByUser = false;

    if (neededSuspensionCallback)
        unregisterForSuspensionCallbackIfNeeded();
    else
        registerForSuspensionCallbackIfNeeded();

    if (didRespectHeightAndWidth != m_inputType->shouldRespectHeightAndWidthAttributes()) {
        NamedNodeMap* map = attributeMap();
        ASSERT(map);
        if (Attribute* height = map->getAttributeItem(heightAttr))
            attributeChanged(height, false);
        if (Attribute* width = map->getAttributeItem(widthAttr))
            attributeChanged(width, false);
        if (Attribute* align = map->getAttributeItem(alignAttr))
            attributeChanged(align, false);
    }

    if (wasAttached) {
        attach();
        if (document()->focusedNode() == this)
            updateFocusAppearance(true);
    }

    setChangedSinceLastFormControlChangeEvent(false);

    checkedRadioButtons().addButton(this);

    setNeedsValidityCheck();
    notifyFormStateChanged();
}

void HTMLInputElement::updateInnerTextValue()
{
    if (!isTextField())
        return;

    if (!suggestedValue().isNull()) {
        setInnerTextValue(suggestedValue());
        updatePlaceholderVisibility(false);
    } else if (!formControlValueMatchesRenderer()) {
        // Update the renderer value if the formControlValueMatchesRenderer() flag is false.
        // It protects an unacceptable renderer value from being overwritten with the DOM value.
        setInnerTextValue(visibleValue());
        updatePlaceholderVisibility(false);
    }
}

void HTMLInputElement::subtreeHasChanged()
{
    ASSERT(isTextField());
    ASSERT(renderer());
    RenderTextControlSingleLine* renderTextControl = toRenderTextControlSingleLine(renderer());

    bool wasChanged = wasChangedSinceLastFormControlChangeEvent();
    setChangedSinceLastFormControlChangeEvent(true);

    // We don't need to call sanitizeUserInputValue() function here because
    // HTMLInputElement::handleBeforeTextInsertedEvent() has already called
    // sanitizeUserInputValue().
    // sanitizeValue() is needed because IME input doesn't dispatch BeforeTextInsertedEvent.
    String value = innerTextValue();
    if (isAcceptableValue(value))
        setValueFromRenderer(sanitizeValue(convertFromVisibleValue(value)));
    updatePlaceholderVisibility(false);
    // Recalc for :invalid and hasUnacceptableValue() change.
    setNeedsStyleRecalc();

    if (cancelButtonElement())
        renderTextControl->updateCancelButtonVisibility();

    // If the incremental attribute is set, then dispatch the search event
    if (searchEventsShouldBeDispatched() && isSearchField() && m_inputType)
        static_cast<SearchInputType*>(m_inputType.get())->startSearchEventTimer();

    if (!wasChanged && focused()) {
        if (Frame* frame = document()->frame())
            frame->editor()->textFieldDidBeginEditing(this);
    }

    if (focused()) {
        if (Frame* frame = document()->frame())
            frame->editor()->textDidChangeInTextField(this);
    }
    // When typing in an input field, childrenChanged is not called, so we need to force the directionality check.
    if (isTextField())
        calculateAndAdjustDirectionality();
}

const AtomicString& HTMLInputElement::formControlType() const
{
    return m_inputType->formControlType();
}

bool HTMLInputElement::saveFormControlState(String& result) const
{
    return m_inputType->saveFormControlState(result);
}

void HTMLInputElement::restoreFormControlState(const String& state)
{
    m_inputType->restoreFormControlState(state);
    m_stateRestored = true;
}

bool HTMLInputElement::canStartSelection() const
{
    if (!isTextField())
        return false;
    return HTMLTextFormControlElement::canStartSelection();
}

bool HTMLInputElement::canHaveSelection() const
{
    return isTextField();
}

void HTMLInputElement::accessKeyAction(bool sendMouseEvents)
{
    m_inputType->accessKeyAction(sendMouseEvents);
}

bool HTMLInputElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (((attrName == heightAttr || attrName == widthAttr) && m_inputType->shouldRespectHeightAndWidthAttributes())
        || attrName == vspaceAttr
        || attrName == hspaceAttr) {
        result = eUniversal;
        return false;
    }

    if (attrName == alignAttr && m_inputType->shouldRespectAlignAttribute()) {
        // Share with <img> since the alignment behavior is the same.
        result = eReplaced;
        return false;
    }

    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLInputElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == nameAttr) {
        checkedRadioButtons().removeButton(this);
        m_name = attr->value();
        checkedRadioButtons().addButton(this);
        HTMLTextFormControlElement::parseMappedAttribute(attr);
    } else if (attr->name() == autocompleteAttr) {
        if (equalIgnoringCase(attr->value(), "off")) {
            m_autocomplete = Off;
            registerForSuspensionCallbackIfNeeded();
        } else {
            bool needsToUnregister = m_autocomplete == Off;

            if (attr->isEmpty())
                m_autocomplete = Uninitialized;
            else
                m_autocomplete = On;

            if (needsToUnregister)
                unregisterForSuspensionCallbackIfNeeded();
        }
    } else if (attr->name() == typeAttr) {
        updateType();
    } else if (attr->name() == valueAttr) {
        // We only need to setChanged if the form is looking at the default value right now.
        if (!hasDirtyValue()) {
            updatePlaceholderVisibility(false);
            setNeedsStyleRecalc();
        }
        setFormControlValueMatchesRenderer(false);
        setNeedsValidityCheck();
    } else if (attr->name() == checkedAttr) {
        // Another radio button in the same group might be checked by state
        // restore. We shouldn't call setChecked() even if this has the checked
        // attribute. So, delay the setChecked() call until
        // finishParsingChildren() is called if parsing is in progress.
        if (!m_parsingInProgress && m_reflectsCheckedAttribute) {
            setChecked(!attr->isNull());
            m_reflectsCheckedAttribute = true;
        }
    } else if (attr->name() == maxlengthAttr)
        parseMaxLengthAttribute(attr);
    else if (attr->name() == sizeAttr) {
        int oldSize = m_size;
        int value = attr->value().toInt();
        m_size = value > 0 ? value : defaultSize;
        if (m_size != oldSize && renderer())
            renderer()->setNeedsLayoutAndPrefWidthsRecalc();
    } else if (attr->name() == altAttr)
        m_inputType->altAttributeChanged();
    else if (attr->name() == srcAttr)
        m_inputType->srcAttributeChanged();
    else if (attr->name() == usemapAttr || attr->name() == accesskeyAttr) {
        // FIXME: ignore for the moment
    } else if (attr->name() == vspaceAttr) {
        addCSSLength(attr, CSSPropertyMarginTop, attr->value());
        addCSSLength(attr, CSSPropertyMarginBottom, attr->value());
    } else if (attr->name() == hspaceAttr) {
        addCSSLength(attr, CSSPropertyMarginLeft, attr->value());
        addCSSLength(attr, CSSPropertyMarginRight, attr->value());
    } else if (attr->name() == alignAttr) {
        if (m_inputType->shouldRespectAlignAttribute())
            addHTMLAlignment(attr);
    } else if (attr->name() == widthAttr) {
        if (m_inputType->shouldRespectHeightAndWidthAttributes())
            addCSSLength(attr, CSSPropertyWidth, attr->value());
    } else if (attr->name() == heightAttr) {
        if (m_inputType->shouldRespectHeightAndWidthAttributes())
            addCSSLength(attr, CSSPropertyHeight, attr->value());
    } else if (attr->name() == borderAttr && isImageButton()) {
        applyBorderAttribute(attr);
    } else if (attr->name() == onsearchAttr) {
        // Search field and slider attributes all just cause updateFromElement to be called through style recalcing.
        setAttributeEventListener(eventNames().searchEvent, createAttributeEventListener(this, attr));
    } else if (attr->name() == resultsAttr) {
        int oldResults = m_maxResults;
        m_maxResults = !attr->isNull() ? std::min(attr->value().toInt(), maxSavedResults) : -1;
        // FIXME: Detaching just for maxResults change is not ideal.  We should figure out the right
        // time to relayout for this change.
        if (m_maxResults != oldResults && (m_maxResults <= 0 || oldResults <= 0))
            reattachIfAttached();
        setNeedsStyleRecalc();
    } else if (attr->name() == autosaveAttr || attr->name() == incrementalAttr)
        setNeedsStyleRecalc();
    else if (attr->name() == minAttr || attr->name() == maxAttr) {
        m_inputType->minOrMaxAttributeChanged();
        setNeedsValidityCheck();
    } else if (attr->name() == multipleAttr) {
        m_inputType->multipleAttributeChanged();
        setNeedsValidityCheck();
    } else if (attr->name() == stepAttr) {
        m_inputType->stepAttributeChanged();
        setNeedsValidityCheck();
    } else if (attr->name() == patternAttr || attr->name() == precisionAttr)
        setNeedsValidityCheck();
    else if (attr->name() == disabledAttr) {
        m_inputType->disabledAttributeChanged();
        HTMLTextFormControlElement::parseMappedAttribute(attr);
    } else if (attr->name() == readonlyAttr) {
        m_inputType->readonlyAttributeChanged();
        HTMLTextFormControlElement::parseMappedAttribute(attr);
    }
#if ENABLE(DATALIST)
    else if (attr->name() == listAttr)
        m_hasNonEmptyList = !attr->isEmpty();
        // FIXME: we need to tell this change to a renderer if the attribute affects the appearance.
#endif
#if ENABLE(INPUT_SPEECH)
    else if (attr->name() == webkitspeechAttr) {
        if (renderer()) {
            // This renderer and its children have quite different layouts and styles depending on
            // whether the speech button is visible or not. So we reset the whole thing and recreate
            // to get the right styles and layout.
            detach();
            m_inputType->destroyShadowSubtree();
            m_inputType->createShadowSubtree();
            attach();
        } else {
            m_inputType->destroyShadowSubtree();
            m_inputType->createShadowSubtree();
        }
        setFormControlValueMatchesRenderer(false);
        setNeedsStyleRecalc();
    } else if (attr->name() == onwebkitspeechchangeAttr)
        setAttributeEventListener(eventNames().webkitspeechchangeEvent, createAttributeEventListener(this, attr));
#endif
    else
        HTMLTextFormControlElement::parseMappedAttribute(attr);
    updateInnerTextValue();
}

void HTMLInputElement::finishParsingChildren()
{
    m_parsingInProgress = false;
    HTMLTextFormControlElement::finishParsingChildren();
    if (!m_stateRestored) {
        bool checked = hasAttribute(checkedAttr);
        if (checked)
            setChecked(checked);
        m_reflectsCheckedAttribute = true;
    }
}

bool HTMLInputElement::rendererIsNeeded(const NodeRenderingContext& context)
{
    return m_inputType->rendererIsNeeded() && HTMLTextFormControlElement::rendererIsNeeded(context);
}

RenderObject* HTMLInputElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return m_inputType->createRenderer(arena, style);
}

void HTMLInputElement::attach()
{
    suspendPostAttachCallbacks();

    if (!m_hasType)
        updateType();

    HTMLTextFormControlElement::attach();

    m_inputType->attach();

    if (document()->focusedNode() == this)
        document()->updateFocusAppearanceSoon(true /* restore selection */);

    resumePostAttachCallbacks();
}

void HTMLInputElement::detach()
{
    HTMLTextFormControlElement::detach();
    setFormControlValueMatchesRenderer(false);
    m_inputType->detach();
}

String HTMLInputElement::altText() const
{
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    // note this is intentionally different to HTMLImageElement::altText()
    String alt = fastGetAttribute(altAttr);
    // fall back to title attribute
    if (alt.isNull())
        alt = getAttribute(titleAttr);
    if (alt.isNull())
        alt = getAttribute(valueAttr);
    if (alt.isEmpty())
        alt = inputElementAltText();
    return alt;
}

bool HTMLInputElement::isSuccessfulSubmitButton() const
{
    // HTML spec says that buttons must have names to be considered successful.
    // However, other browsers do not impose this constraint. So we do not.
    return !disabled() && m_inputType->canBeSuccessfulSubmitButton();
}

bool HTMLInputElement::isActivatedSubmit() const
{
    return m_isActivatedSubmit;
}

void HTMLInputElement::setActivatedSubmit(bool flag)
{
    m_isActivatedSubmit = flag;
}

bool HTMLInputElement::appendFormData(FormDataList& encoding, bool multipart)
{
    return m_inputType->isFormDataAppendable() && m_inputType->appendFormData(encoding, multipart);
}

void HTMLInputElement::reset()
{
    if (m_inputType->storesValueSeparateFromAttribute())
        setValue(String());

    setAutofilled(false);
    setChecked(hasAttribute(checkedAttr));
    m_reflectsCheckedAttribute = true;
}

bool HTMLInputElement::isTextField() const
{
    return m_inputType->isTextField();
}

bool HTMLInputElement::isTextType() const
{
    return m_inputType->isTextType();
}

void HTMLInputElement::setChecked(bool nowChecked, bool sendChangeEvent)
{
    if (checked() == nowChecked)
        return;

    checkedRadioButtons().removeButton(this);

    m_reflectsCheckedAttribute = false;
    m_isChecked = nowChecked;
    setNeedsStyleRecalc();
    if (isRadioButton())
        updateCheckedRadioButtons();
    if (renderer() && renderer()->style()->hasAppearance())
        renderer()->theme()->stateChanged(renderer(), CheckedState);
    setNeedsValidityCheck();

    // Ideally we'd do this from the render tree (matching
    // RenderTextView), but it's not possible to do it at the moment
    // because of the way the code is structured.
    if (renderer() && AXObjectCache::accessibilityEnabled())
        renderer()->document()->axObjectCache()->checkedStateChanged(renderer());

    // Only send a change event for items in the document (avoid firing during
    // parsing) and don't send a change event for a radio button that's getting
    // unchecked to match other browsers. DOM is not a useful standard for this
    // because it says only to fire change events at "lose focus" time, which is
    // definitely wrong in practice for these types of elements.
    if (sendChangeEvent && inDocument() && m_inputType->shouldSendChangeEventAfterCheckedChanged()) {
        setTextAsOfLastFormControlChangeEvent(String());
        dispatchFormControlChangeEvent();
    }
}

void HTMLInputElement::setIndeterminate(bool newValue)
{
    if (!m_inputType->isCheckable() || indeterminate() == newValue)
        return;

    m_isIndeterminate = newValue;

    setNeedsStyleRecalc();

    if (renderer() && renderer()->style()->hasAppearance())
        renderer()->theme()->stateChanged(renderer(), CheckedState);
}

int HTMLInputElement::size() const
{
    return m_size;
}

bool HTMLInputElement::sizeShouldIncludeDecoration(int& preferredSize) const
{
    return m_inputType->sizeShouldIncludeDecoration(defaultSize, preferredSize);
}

void HTMLInputElement::copyNonAttributeProperties(const Element* source)
{
    const HTMLInputElement* sourceElement = static_cast<const HTMLInputElement*>(source);

    m_valueIfDirty = sourceElement->m_valueIfDirty;
    m_wasModifiedByUser = false;
    setChecked(sourceElement->m_isChecked);
    m_reflectsCheckedAttribute = sourceElement->m_reflectsCheckedAttribute;
    m_isIndeterminate = sourceElement->m_isIndeterminate;

    HTMLTextFormControlElement::copyNonAttributeProperties(source);

    setFormControlValueMatchesRenderer(false);
    updateInnerTextValue();
}

String HTMLInputElement::value() const
{
    String value;
    if (m_inputType->getTypeSpecificValue(value))
        return value;

    value = m_valueIfDirty;
    if (!value.isNull())
        return value;

    value = sanitizeValue(fastGetAttribute(valueAttr));
    if (!value.isNull())
        return value;

    return m_inputType->fallbackValue();
}

String HTMLInputElement::valueWithDefault() const
{
    String value = this->value();
    if (!value.isNull())
        return value;

    return m_inputType->defaultValue();
}

void HTMLInputElement::setValueForUser(const String& value)
{
    // Call setValue and make it send a change event.
    setValue(value, true);
}

const String& HTMLInputElement::suggestedValue() const
{
    return m_suggestedValue;
}

void HTMLInputElement::setSuggestedValue(const String& value)
{
    if (!m_inputType->canSetSuggestedValue())
        return;
    setFormControlValueMatchesRenderer(false);
    m_suggestedValue = sanitizeValue(value);
    setNeedsStyleRecalc();
    updateInnerTextValue();
}

void HTMLInputElement::setValue(const String& value, bool sendChangeEvent)
{
    if (!m_inputType->canSetValue(value))
        return;

    RefPtr<HTMLInputElement> protector(this);
    String sanitizedValue = sanitizeValue(value);
    bool valueChanged = sanitizedValue != this->value();

    setLastChangeWasNotUserEdit();
    setFormControlValueMatchesRenderer(false);
    m_suggestedValue = String(); // Prevent TextFieldInputType::setValue from using the suggested value.
    m_inputType->setValue(sanitizedValue, valueChanged, sendChangeEvent);

    if (!valueChanged)
        return;

    if (sendChangeEvent)
        m_inputType->dispatchChangeEventInResponseToSetValue();

    // FIXME: Why do we do this when !sendChangeEvent?
    if (isTextField() && (!focused() || !sendChangeEvent))
        setTextAsOfLastFormControlChangeEvent(value);

    notifyFormStateChanged();
}

void HTMLInputElement::setValueInternal(const String& sanitizedValue, bool sendChangeEvent)
{
    m_valueIfDirty = sanitizedValue;
    m_wasModifiedByUser = sendChangeEvent;
    setNeedsValidityCheck();
}

double HTMLInputElement::valueAsDate() const
{
    return m_inputType->valueAsDate();
}

void HTMLInputElement::setValueAsDate(double value, ExceptionCode& ec)
{
    m_inputType->setValueAsDate(value, ec);
}

double HTMLInputElement::valueAsNumber() const
{
    return m_inputType->valueAsNumber();
}

void HTMLInputElement::setValueAsNumber(double newValue, ExceptionCode& ec, bool sendChangeEvent)
{
    if (!isfinite(newValue)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    m_inputType->setValueAsNumber(newValue, sendChangeEvent, ec);
}

String HTMLInputElement::placeholder() const
{
    return fastGetAttribute(placeholderAttr).string();
}

void HTMLInputElement::setPlaceholder(const String& value)
{
    setAttribute(placeholderAttr, value);
}

bool HTMLInputElement::searchEventsShouldBeDispatched() const
{
    return hasAttribute(incrementalAttr);
}

void HTMLInputElement::setValueFromRenderer(const String& value)
{
    // File upload controls will never use this.
    ASSERT(!isFileUpload());

    m_suggestedValue = String();

    // Renderer and our event handler are responsible for sanitizing values.
    ASSERT(value == sanitizeValue(value) || sanitizeValue(value).isEmpty());

    // Workaround for bug where trailing \n is included in the result of textContent.
    // The assert macro above may also be simplified to: value == constrainValue(value)
    // http://bugs.webkit.org/show_bug.cgi?id=9661
    m_valueIfDirty = value == "\n" ? String("") : value;

    setFormControlValueMatchesRenderer(true);
    m_wasModifiedByUser = true;

    // Input event is fired by the Node::defaultEventHandler for editable controls.
    if (!isTextField())
        dispatchInputEvent();
    notifyFormStateChanged();

    setNeedsValidityCheck();

    // Clear autofill flag (and yellow background) on user edit.
    setAutofilled(false);
}

void* HTMLInputElement::preDispatchEventHandler(Event* event)
{
    if (event->type() == eventNames().textInputEvent && m_inputType->shouldSubmitImplicitly(event)) {
        event->stopPropagation();
        return 0;
    }
    if (event->type() != eventNames().clickEvent)
        return 0;
    if (!event->isMouseEvent() || static_cast<MouseEvent*>(event)->button() != LeftButton)
        return 0;
    // FIXME: Check whether there are any cases where this actually ends up leaking.
    return m_inputType->willDispatchClick().leakPtr();
}

void HTMLInputElement::postDispatchEventHandler(Event* event, void* dataFromPreDispatch)
{
    OwnPtr<ClickHandlingState> state = adoptPtr(static_cast<ClickHandlingState*>(dataFromPreDispatch));
    if (!state)
        return;
    m_inputType->didDispatchClick(event, *state);
}

void HTMLInputElement::defaultEventHandler(Event* evt)
{
    if (evt->isMouseEvent() && evt->type() == eventNames().clickEvent && static_cast<MouseEvent*>(evt)->button() == LeftButton) {
        m_inputType->handleClickEvent(static_cast<MouseEvent*>(evt));
        if (evt->defaultHandled())
            return;
    }

    if (evt->isKeyboardEvent() && evt->type() == eventNames().keydownEvent) {
        m_inputType->handleKeydownEvent(static_cast<KeyboardEvent*>(evt));
        if (evt->defaultHandled())
            return;
    }

    // Call the base event handler before any of our own event handling for almost all events in text fields.
    // Makes editing keyboard handling take precedence over the keydown and keypress handling in this function.
    bool callBaseClassEarly = isTextField() && (evt->type() == eventNames().keydownEvent || evt->type() == eventNames().keypressEvent);
    if (callBaseClassEarly) {
        HTMLTextFormControlElement::defaultEventHandler(evt);
        if (evt->defaultHandled())
            return;
    }

    // DOMActivate events cause the input to be "activated" - in the case of image and submit inputs, this means
    // actually submitting the form. For reset inputs, the form is reset. These events are sent when the user clicks
    // on the element, or presses enter while it is the active element. JavaScript code wishing to activate the element
    // must dispatch a DOMActivate event - a click event will not do the job.
    if (evt->type() == eventNames().DOMActivateEvent) {
        m_inputType->handleDOMActivateEvent(evt);
        if (evt->defaultHandled())
            return;
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (evt->isKeyboardEvent() && evt->type() == eventNames().keypressEvent) {
        m_inputType->handleKeypressEvent(static_cast<KeyboardEvent*>(evt));
        if (evt->defaultHandled())
            return;
    }

    if (evt->isKeyboardEvent() && evt->type() == eventNames().keyupEvent) {
        m_inputType->handleKeyupEvent(static_cast<KeyboardEvent*>(evt));
        if (evt->defaultHandled())
            return;
    }

    if (m_inputType->shouldSubmitImplicitly(evt)) {
        if (isSearchField()) {
            addSearchResult();
            onSearch();
        }
        // Form submission finishes editing, just as loss of focus does.
        // If there was a change, send the event now.
        if (wasChangedSinceLastFormControlChangeEvent())
            dispatchFormControlChangeEvent();

        RefPtr<HTMLFormElement> formForSubmission = m_inputType->formForSubmission();
        // Form may never have been present, or may have been destroyed by code responding to the change event.
        if (formForSubmission)
            formForSubmission->submitImplicitly(evt, canTriggerImplicitSubmission());

        evt->setDefaultHandled();
        return;
    }

    if (evt->isBeforeTextInsertedEvent())
        m_inputType->handleBeforeTextInsertedEvent(static_cast<BeforeTextInsertedEvent*>(evt));

    if (evt->hasInterface(eventNames().interfaceForWheelEvent)) {
        m_inputType->handleWheelEvent(static_cast<WheelEvent*>(evt));
        if (evt->defaultHandled())
            return;
    }

    if (evt->isMouseEvent() && evt->type() == eventNames().mousedownEvent) {
        m_inputType->handleMouseDownEvent(static_cast<MouseEvent*>(evt));
        if (evt->defaultHandled())
            return;
    }

    m_inputType->forwardEvent(evt);

    if (!callBaseClassEarly && !evt->defaultHandled())
        HTMLTextFormControlElement::defaultEventHandler(evt);
}

bool HTMLInputElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == srcAttr || attr->name() == formactionAttr || HTMLTextFormControlElement::isURLAttribute(attr);
}

String HTMLInputElement::defaultValue() const
{
    return fastGetAttribute(valueAttr);
}

void HTMLInputElement::setDefaultValue(const String &value)
{
    setAttribute(valueAttr, value);
}

void HTMLInputElement::setDefaultName(const AtomicString& name)
{
    m_name = name;
}

static inline bool isRFC2616TokenCharacter(UChar ch)
{
    return isASCII(ch) && ch > ' ' && ch != '"' && ch != '(' && ch != ')' && ch != ',' && ch != '/' && (ch < ':' || ch > '@') && (ch < '[' || ch > ']') && ch != '{' && ch != '}' && ch != 0x7f;
}

static inline bool isValidMIMEType(const String& type)
{
    size_t slashPosition = type.find('/');
    if (slashPosition == notFound || !slashPosition || slashPosition == type.length() - 1)
        return false;
    for (size_t i = 0; i < type.length(); ++i) {
        if (!isRFC2616TokenCharacter(type[i]) && i != slashPosition)
            return false;
    }
    return true;
}

Vector<String> HTMLInputElement::acceptMIMETypes()
{
    Vector<String> mimeTypes;

    String acceptString = accept();
    if (acceptString.isEmpty())
        return mimeTypes;

    Vector<String> splitTypes;
    acceptString.split(',', false, splitTypes);
    for (size_t i = 0; i < splitTypes.size(); ++i) {
        String trimmedMimeType = stripLeadingAndTrailingHTMLSpaces(splitTypes[i]);
        if (trimmedMimeType.isEmpty())
            continue;
        if (!isValidMIMEType(trimmedMimeType))
            continue;
        mimeTypes.append(trimmedMimeType.lower());
    }

    return mimeTypes;
}

String HTMLInputElement::accept() const
{
    return fastGetAttribute(acceptAttr);
}

String HTMLInputElement::alt() const
{
    return fastGetAttribute(altAttr);
}

int HTMLInputElement::maxLength() const
{
    return m_maxLength;
}

void HTMLInputElement::setMaxLength(int maxLength, ExceptionCode& ec)
{
    if (maxLength < 0)
        ec = INDEX_SIZE_ERR;
    else
        setAttribute(maxlengthAttr, String::number(maxLength));
}

bool HTMLInputElement::multiple() const
{
    return fastHasAttribute(multipleAttr);
}

void HTMLInputElement::setSize(unsigned size)
{
    setAttribute(sizeAttr, String::number(size));
}

KURL HTMLInputElement::src() const
{
    return document()->completeURL(fastGetAttribute(srcAttr));
}

void HTMLInputElement::setAutofilled(bool autofilled)
{
    if (autofilled == m_isAutofilled)
        return;

    m_isAutofilled = autofilled;
    setNeedsStyleRecalc();
}

FileList* HTMLInputElement::files()
{
    return m_inputType->files();
}

void HTMLInputElement::receiveDroppedFiles(const Vector<String>& filenames)
{
    m_inputType->receiveDroppedFiles(filenames);
}

Icon* HTMLInputElement::icon() const
{
    return m_inputType->icon();
}

bool HTMLInputElement::canReceiveDroppedFiles() const
{
    return m_canReceiveDroppedFiles;
}

void HTMLInputElement::setCanReceiveDroppedFiles(bool canReceiveDroppedFiles)
{
    if (m_canReceiveDroppedFiles == canReceiveDroppedFiles)
        return;
    m_canReceiveDroppedFiles = canReceiveDroppedFiles;
    renderer()->updateFromElement();
}

String HTMLInputElement::visibleValue() const
{
    return m_inputType->visibleValue();
}

String HTMLInputElement::convertFromVisibleValue(const String& visibleValue) const
{
    return m_inputType->convertFromVisibleValue(visibleValue);
}

bool HTMLInputElement::isAcceptableValue(const String& proposedValue) const
{
    return m_inputType->isAcceptableValue(proposedValue);
}

String HTMLInputElement::sanitizeValue(const String& proposedValue) const
{
    if (proposedValue.isNull())
        return proposedValue;
    return m_inputType->sanitizeValue(proposedValue);
}

bool HTMLInputElement::hasUnacceptableValue() const
{
    return m_inputType->hasUnacceptableValue();
}

bool HTMLInputElement::isInRange() const
{
    return m_inputType->supportsRangeLimitation() && !rangeUnderflow(value()) && !rangeOverflow(value());
}

bool HTMLInputElement::isOutOfRange() const
{
    return m_inputType->supportsRangeLimitation() && (rangeUnderflow(value()) || rangeOverflow(value()));
}

bool HTMLInputElement::needsSuspensionCallback()
{
    return m_autocomplete == Off || m_inputType->shouldResetOnDocumentActivation();
}

void HTMLInputElement::registerForSuspensionCallbackIfNeeded()
{
    if (needsSuspensionCallback())
        document()->registerForPageCacheSuspensionCallbacks(this);
}

void HTMLInputElement::unregisterForSuspensionCallbackIfNeeded()
{
    if (!needsSuspensionCallback())
        document()->unregisterForPageCacheSuspensionCallbacks(this);
}

bool HTMLInputElement::isRequiredFormControl() const
{
    return m_inputType->supportsRequired() && required();
}

void HTMLInputElement::addSearchResult()
{
    ASSERT(isSearchField());
    if (renderer())
        toRenderTextControlSingleLine(renderer())->addSearchResult();
}

void HTMLInputElement::onSearch()
{
    ASSERT(isSearchField());
    if (m_inputType)
        static_cast<SearchInputType*>(m_inputType.get())->stopSearchEventTimer();
    dispatchEvent(Event::create(eventNames().searchEvent, true, false));
}

void HTMLInputElement::documentDidResumeFromPageCache()
{
    ASSERT(needsSuspensionCallback());
    reset();
}

void HTMLInputElement::willMoveToNewOwnerDocument()
{
    m_inputType->willMoveToNewOwnerDocument();

    // Always unregister for cache callbacks when leaving a document, even if we would otherwise like to be registered
    if (needsSuspensionCallback())
        document()->unregisterForPageCacheSuspensionCallbacks(this);

    document()->checkedRadioButtons().removeButton(this);

    HTMLTextFormControlElement::willMoveToNewOwnerDocument();
}

void HTMLInputElement::didMoveToNewOwnerDocument()
{
    registerForSuspensionCallbackIfNeeded();

    HTMLTextFormControlElement::didMoveToNewOwnerDocument();
}

void HTMLInputElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    HTMLTextFormControlElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, src());
}

bool HTMLInputElement::recalcWillValidate() const
{
    return m_inputType->supportsValidation() && HTMLTextFormControlElement::recalcWillValidate();
}

#if ENABLE(INPUT_COLOR)
void HTMLInputElement::selectColorInColorChooser(const Color& color)
{
    if (!m_inputType->isColorControl())
        return;
    static_cast<ColorInputType*>(m_inputType.get())->didChooseColor(color);
}
#endif
    
#if ENABLE(DATALIST)

HTMLElement* HTMLInputElement::list() const
{
    return dataList();
}

HTMLDataListElement* HTMLInputElement::dataList() const
{
    if (!m_hasNonEmptyList)
        return 0;

    if (!m_inputType->shouldRespectListAttribute())
        return 0;

    Element* element = treeScope()->getElementById(fastGetAttribute(listAttr));
    if (!element)
        return 0;
    if (!element->hasTagName(datalistTag))
        return 0;

    return static_cast<HTMLDataListElement*>(element);
}

HTMLOptionElement* HTMLInputElement::selectedOption() const
{
    String value = this->value();

    // The empty string never matches to a datalist option because it
    // doesn't represent a suggestion according to the standard.
    if (value.isEmpty())
        return 0;

    HTMLDataListElement* sourceElement = dataList();
    if (!sourceElement)
        return 0;
    RefPtr<HTMLCollection> options = sourceElement->options();
    if (!options)
        return 0;
    unsigned length = options->length();
    for (unsigned i = 0; i < length; ++i) {
        HTMLOptionElement* option = static_cast<HTMLOptionElement*>(options->item(i));
        if (!option->disabled() && value == option->value())
            return option;
    }
    return 0;
}

#endif // ENABLE(DATALIST)

bool HTMLInputElement::isSteppable() const
{
    return m_inputType->isSteppable();
}

void HTMLInputElement::stepUpFromRenderer(int n)
{
    // The differences from stepUp()/stepDown():
    //
    // Difference 1: the current value
    // If the current value is not a number, including empty, the current value is assumed as 0.
    //   * If 0 is in-range, and matches to step value
    //     - The value should be the +step if n > 0
    //     - The value should be the -step if n < 0
    //     If -step or +step is out of range, new value should be 0.
    //   * If 0 is smaller than the minimum value
    //     - The value should be the minimum value for any n
    //   * If 0 is larger than the maximum value
    //     - The value should be the maximum value for any n
    //   * If 0 is in-range, but not matched to step value
    //     - The value should be the larger matched value nearest to 0 if n > 0
    //       e.g. <input type=number min=-100 step=3> -> 2
    //     - The value should be the smaler matched value nearest to 0 if n < 0
    //       e.g. <input type=number min=-100 step=3> -> -1
    //   As for date/datetime-local/month/time/week types, the current value is assumed as "the current local date/time".
    //   As for datetime type, the current value is assumed as "the current date/time in UTC".
    // If the current value is smaller than the minimum value:
    //  - The value should be the minimum value if n > 0
    //  - Nothing should happen if n < 0
    // If the current value is larger than the maximum value:
    //  - The value should be the maximum value if n < 0
    //  - Nothing should happen if n > 0
    //
    // Difference 2: clamping steps
    // If the current value is not matched to step value:
    // - The value should be the larger matched value nearest to 0 if n > 0
    //   e.g. <input type=number value=3 min=-100 step=3> -> 5
    // - The value should be the smaler matched value nearest to 0 if n < 0
    //   e.g. <input type=number value=3 min=-100 step=3> -> 2
    //
    // n is assumed as -n if step < 0.

    ASSERT(isSteppable());
    if (!isSteppable())
        return;
    ASSERT(n);
    if (!n)
        return;

    unsigned stepDecimalPlaces, baseDecimalPlaces;
    double step, base;
    // FIXME: Not any changes after stepping, even if it is an invalid value, may be better.
    // (e.g. Stepping-up for <input type="number" value="foo" step="any" /> => "foo")
    if (!getAllowedValueStepWithDecimalPlaces(AnyIsDefaultStep, &step, &stepDecimalPlaces))
      return;
    base = m_inputType->stepBaseWithDecimalPlaces(&baseDecimalPlaces);
    baseDecimalPlaces = min(baseDecimalPlaces, 16u);

    int sign;
    if (step > 0)
        sign = n;
    else if (step < 0)
        sign = -n;
    else
        sign = 0;

    const double nan = numeric_limits<double>::quiet_NaN();
    String currentStringValue = value();
    double current = m_inputType->parseToDouble(currentStringValue, nan);
    const bool sendChangeEvent = true;
    if (!isfinite(current)) {
        ExceptionCode ec;
        current = m_inputType->defaultValueForStepUp();
        double nextDiff = step * n;
        if (current < m_inputType->minimum() - nextDiff)
            current = m_inputType->minimum() - nextDiff;
        if (current > m_inputType->maximum() - nextDiff)
            current = m_inputType->maximum() - nextDiff;
        setValueAsNumber(current, ec, sendChangeEvent);
    }
    if ((sign > 0 && current < m_inputType->minimum()) || (sign < 0 && current > m_inputType->maximum()))
        setValue(m_inputType->serialize(sign > 0 ? m_inputType->minimum() : m_inputType->maximum()), sendChangeEvent);
    else {
        ExceptionCode ec;
        if (stepMismatch(value())) {
            ASSERT(step);
            double newValue;
            double scale = pow(10.0, static_cast<double>(max(stepDecimalPlaces, baseDecimalPlaces)));

            if (sign < 0)
                newValue = round((base + floor((current - base) / step) * step) * scale) / scale;
            else if (sign > 0)
                newValue = round((base + ceil((current - base) / step) * step) * scale) / scale;
            else
                newValue = current;

            if (newValue < m_inputType->minimum())
                newValue = m_inputType->minimum();
            if (newValue > m_inputType->maximum())
                newValue = m_inputType->maximum();

            setValueAsNumber(newValue, ec, n == 1 || n == -1);
            current = newValue;
            if (n > 1)
                applyStep(n - 1, AnyIsDefaultStep, sendChangeEvent, ec);
            else if (n < -1)
                applyStep(n + 1, AnyIsDefaultStep, sendChangeEvent, ec);
        } else
            applyStep(n, AnyIsDefaultStep, sendChangeEvent, ec);
    }
}

#if ENABLE(INPUT_SPEECH)

bool HTMLInputElement::isSpeechEnabled() const
{
    // FIXME: Add support for RANGE, EMAIL, URL, COLOR and DATE/TIME input types.
    return m_inputType->shouldRespectSpeechAttribute() && RuntimeEnabledFeatures::speechInputEnabled() && hasAttribute(webkitspeechAttr);
}

#endif

bool HTMLInputElement::isTextButton() const
{
    return m_inputType->isTextButton();
}

bool HTMLInputElement::isRadioButton() const
{
    return m_inputType->isRadioButton();
}

bool HTMLInputElement::isSearchField() const
{
    return m_inputType->isSearchField();
}

bool HTMLInputElement::isInputTypeHidden() const
{
    return m_inputType->isHiddenType();
}

bool HTMLInputElement::isPasswordField() const
{
    return m_inputType->isPasswordField();
}

bool HTMLInputElement::isCheckbox() const
{
    return m_inputType->isCheckbox();
}

bool HTMLInputElement::isRangeControl() const
{
    return m_inputType->isRangeControl();
}

bool HTMLInputElement::isText() const
{
    return m_inputType->isTextType();
}

bool HTMLInputElement::isEmailField() const
{
    return m_inputType->isEmailField();
}

bool HTMLInputElement::isFileUpload() const
{
    return m_inputType->isFileUpload();
}

bool HTMLInputElement::isImageButton() const
{
    return m_inputType->isImageButton();
}

bool HTMLInputElement::isNumberField() const
{
    return m_inputType->isNumberField();
}

bool HTMLInputElement::isSubmitButton() const
{
    return m_inputType->isSubmitButton();
}

bool HTMLInputElement::isTelephoneField() const
{
    return m_inputType->isTelephoneField();
}

bool HTMLInputElement::isURLField() const
{
    return m_inputType->isURLField();
}

bool HTMLInputElement::isEnumeratable() const
{
    return m_inputType->isEnumeratable();
}

bool HTMLInputElement::shouldAppearChecked() const
{
    return checked() && m_inputType->isCheckable();
}

bool HTMLInputElement::supportsPlaceholder() const
{
    return m_inputType->supportsPlaceholder();
}

void HTMLInputElement::updatePlaceholderText()
{
    return m_inputType->updatePlaceholderText();
}

CheckedRadioButtons& HTMLInputElement::checkedRadioButtons() const
{
    if (HTMLFormElement* formElement = form())
        return formElement->checkedRadioButtons();
    return document()->checkedRadioButtons();
}

void HTMLInputElement::parseMaxLengthAttribute(Attribute* attribute)
{
    int maxLength;
    if (!parseHTMLInteger(attribute->value(), maxLength))
        maxLength = maximumLength;
    if (maxLength < 0 || maxLength > maximumLength)
        maxLength = maximumLength;
    int oldMaxLength = m_maxLength;
    m_maxLength = maxLength;
    if (oldMaxLength != maxLength)
        updateValueIfNeeded();
    setNeedsStyleRecalc();
    setNeedsValidityCheck();
}

void HTMLInputElement::updateValueIfNeeded()
{
    String newValue = sanitizeValue(m_valueIfDirty);
    ASSERT(!m_valueIfDirty.isNull() || newValue.isNull());
    if (newValue != m_valueIfDirty)
        setValue(newValue);
}

String HTMLInputElement::defaultToolTip() const
{
    return m_inputType->defaultToolTip();
}

} // namespace
