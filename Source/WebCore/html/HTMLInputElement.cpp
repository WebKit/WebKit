/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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
#include "CSSGradientValue.h"
#include "CSSPropertyNames.h"
#include "CSSValuePool.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DateTimeChooser.h"
#include "Document.h"
#include "Editor.h"
#include "EventNames.h"
#include "FileInputType.h"
#include "FileList.h"
#include "FormController.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLDataListElement.h"
#include "HTMLFormElement.h"
#include "HTMLImageLoader.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
#include "IdTargetObserver.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "RenderTextControlSingleLine.h"
#include "RenderTheme.h"
#include "ScopedEventQueue.h"
#include "SearchInputType.h"
#include "Settings.h"
#include "StyleGeneratedImage.h"
#include "TextControlInnerElements.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Language.h>
#include <wtf/MathExtras.h>
#include <wtf/Ref.h>

#if ENABLE(TOUCH_EVENTS)
#include "TouchEvent.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLInputElement);

using namespace HTMLNames;

#if ENABLE(DATALIST_ELEMENT)
class ListAttributeTargetObserver : IdTargetObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ListAttributeTargetObserver(const AtomString& id, HTMLInputElement*);

    void idTargetChanged() override;

private:
    HTMLInputElement* m_element;
};
#endif

// FIXME: According to HTML4, the length attribute's value can be arbitrarily
// large. However, due to https://bugs.webkit.org/show_bug.cgi?id=14536 things
// get rather sluggish when a text field has a larger number of characters than
// this, even when just clicking in the text field.
const unsigned HTMLInputElement::maxEffectiveLength = 524288;
const int defaultSize = 20;
const int maxSavedResults = 256;

HTMLInputElement::HTMLInputElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form, bool createdByParser)
    : HTMLTextFormControlElement(tagName, document, form)
    , m_size(defaultSize)
    , m_isChecked(false)
    , m_dirtyCheckednessFlag(false)
    , m_isIndeterminate(false)
    , m_hasType(false)
    , m_isActivatedSubmit(false)
    , m_autocomplete(Uninitialized)
    , m_isAutoFilled(false)
    , m_isAutoFilledAndViewable(false)
    , m_autoFillButtonType(static_cast<uint8_t>(AutoFillButtonType::None))
    , m_lastAutoFillButtonType(static_cast<uint8_t>(AutoFillButtonType::None))
    , m_isAutoFillAvailable(false)
#if ENABLE(DATALIST_ELEMENT)
    , m_hasNonEmptyList(false)
#endif
    , m_stateRestored(false)
    , m_parsingInProgress(createdByParser)
    , m_valueAttributeWasUpdatedAfterParsing(false)
    , m_wasModifiedByUser(false)
    , m_canReceiveDroppedFiles(false)
#if ENABLE(TOUCH_EVENTS)
    , m_hasTouchEventHandler(false)
#endif
    , m_isSpellcheckDisabledExceptTextReplacement(false)
{
    // m_inputType is lazily created when constructed by the parser to avoid constructing unnecessarily a text inputType and
    // its shadow subtree, just to destroy them when the |type| attribute gets set by the parser to something else than 'text'.
    if (!createdByParser)
        m_inputType = InputType::createText(*this);

    ASSERT(hasTagName(inputTag));
    setHasCustomStyleResolveCallbacks();
}

Ref<HTMLInputElement> HTMLInputElement::create(const QualifiedName& tagName, Document& document, HTMLFormElement* form, bool createdByParser)
{
    bool shouldCreateShadowRootLazily = createdByParser;
    Ref<HTMLInputElement> inputElement = adoptRef(*new HTMLInputElement(tagName, document, form, createdByParser));
    if (!shouldCreateShadowRootLazily)
        inputElement->ensureUserAgentShadowRoot();
    return inputElement;
}

HTMLImageLoader& HTMLInputElement::ensureImageLoader()
{
    if (!m_imageLoader)
        m_imageLoader = makeUnique<HTMLImageLoader>(*this);
    return *m_imageLoader;
}

void HTMLInputElement::didAddUserAgentShadowRoot(ShadowRoot&)
{
    Ref<InputType> protectedInputType(*m_inputType);
    protectedInputType->createShadowSubtree();
    updateInnerTextElementEditability();
}

HTMLInputElement::~HTMLInputElement()
{
    if (needsSuspensionCallback())
        document().unregisterForDocumentSuspensionCallbacks(*this);

    // Need to remove form association while this is still an HTMLInputElement
    // so that virtual functions are called correctly.
    setForm(nullptr);

    // This is needed for a radio button that was not in a form, and also for
    // a radio button that was in a form. The call to setForm(nullptr) above
    // actually adds the button to the document groups in the latter case.
    // That is inelegant, but harmless since we remove it here.
    if (isRadioButton())
        treeScope().radioButtonGroups().removeButton(*this);

#if ENABLE(TOUCH_EVENTS)
    if (m_hasTouchEventHandler)
        document().didRemoveEventTargetNode(*this);
#endif
}

const AtomString& HTMLInputElement::name() const
{
    return m_name.isNull() ? emptyAtom() : m_name;
}

Vector<FileChooserFileInfo> HTMLInputElement::filesFromFileInputFormControlState(const FormControlState& state)
{
    return FileInputType::filesFromFormControlState(state);
}

HTMLElement* HTMLInputElement::containerElement() const
{
    return m_inputType->containerElement();
}

RefPtr<TextControlInnerTextElement> HTMLInputElement::innerTextElement() const
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

HTMLElement* HTMLInputElement::capsLockIndicatorElement() const
{
    return m_inputType->capsLockIndicatorElement();
}

HTMLElement* HTMLInputElement::autoFillButtonElement() const
{
    return m_inputType->autoFillButtonElement();
}

HTMLElement* HTMLInputElement::resultsButtonElement() const
{
    return m_inputType->resultsButtonElement();
}

HTMLElement* HTMLInputElement::cancelButtonElement() const
{
    return m_inputType->cancelButtonElement();
}

HTMLElement* HTMLInputElement::sliderThumbElement() const
{
    return m_inputType->sliderThumbElement();
}

HTMLElement* HTMLInputElement::sliderTrackElement() const
{
    return m_inputType->sliderTrackElement();
}

HTMLElement* HTMLInputElement::placeholderElement() const
{
    return m_inputType->placeholderElement();
}

#if ENABLE(DATALIST_ELEMENT)
HTMLElement* HTMLInputElement::dataListButtonElement() const
{
    return m_inputType->dataListButtonElement();
}
#endif

bool HTMLInputElement::shouldAutocomplete() const
{
    if (m_autocomplete != Uninitialized)
        return m_autocomplete == On;
    return HTMLTextFormControlElement::shouldAutocomplete();
}

bool HTMLInputElement::isValidValue(const String& value) const
{
    if (!m_inputType->canSetStringValue()) {
        ASSERT_NOT_REACHED();
        return false;
    }
    return !m_inputType->typeMismatchFor(value)
        && !m_inputType->stepMismatch(value)
        && !m_inputType->rangeUnderflow(value)
        && !m_inputType->rangeOverflow(value)
        && !tooShort(value, IgnoreDirtyFlag)
        && !tooLong(value, IgnoreDirtyFlag)
        && !m_inputType->patternMismatch(value)
        && !m_inputType->valueMissing(value);
}

bool HTMLInputElement::tooShort() const
{
    return willValidate() && tooShort(value(), CheckDirtyFlag);
}

bool HTMLInputElement::tooLong() const
{
    return willValidate() && tooLong(value(), CheckDirtyFlag);
}

bool HTMLInputElement::typeMismatch() const
{
    return willValidate() && m_inputType->typeMismatch();
}

bool HTMLInputElement::valueMissing() const
{
    return willValidate() && m_inputType->valueMissing(value());
}

bool HTMLInputElement::hasBadInput() const
{
    return willValidate() && m_inputType->hasBadInput();
}

bool HTMLInputElement::patternMismatch() const
{
    return willValidate() && m_inputType->patternMismatch(value());
}

bool HTMLInputElement::tooShort(StringView value, NeedsToCheckDirtyFlag check) const
{
    if (!supportsMinLength())
        return false;

    int min = minLength();
    if (min <= 0)
        return false;

    if (check == CheckDirtyFlag) {
        // Return false for the default value or a value set by a script even if
        // it is shorter than minLength.
        if (!hasDirtyValue() || !m_wasModifiedByUser)
            return false;
    }

    // The empty string is excluded from tooShort validation.
    if (value.isEmpty())
        return false;

    // FIXME: The HTML specification says that the "number of characters" is measured using code-unit length.
    return numGraphemeClusters(value) < static_cast<unsigned>(min);
}

bool HTMLInputElement::tooLong(StringView value, NeedsToCheckDirtyFlag check) const
{
    if (!supportsMaxLength())
        return false;
    unsigned max = effectiveMaxLength();
    if (check == CheckDirtyFlag) {
        // Return false for the default value or a value set by a script even if
        // it is longer than maxLength.
        if (!hasDirtyValue() || !m_wasModifiedByUser)
            return false;
    }
    // FIXME: The HTML specification says that the "number of characters" is measured using code-unit length.
    return numGraphemeClusters(value) > max;
}

bool HTMLInputElement::rangeUnderflow() const
{
    return willValidate() && m_inputType->rangeUnderflow(value());
}

bool HTMLInputElement::rangeOverflow() const
{
    return willValidate() && m_inputType->rangeOverflow(value());
}

String HTMLInputElement::validationMessage() const
{
    if (!willValidate())
        return String();

    if (customError())
        return customValidationMessage();

    return m_inputType->validationMessage();
}

double HTMLInputElement::minimum() const
{
    return m_inputType->minimum();
}

double HTMLInputElement::maximum() const
{
    return m_inputType->maximum();
}

bool HTMLInputElement::stepMismatch() const
{
    return willValidate() && m_inputType->stepMismatch(value());
}

bool HTMLInputElement::isValid() const
{
    if (!willValidate())
        return true;

    String value = this->value();
    bool someError = m_inputType->typeMismatch() || m_inputType->stepMismatch(value) || m_inputType->rangeUnderflow(value) || m_inputType->rangeOverflow(value)
        || tooShort(value, CheckDirtyFlag) || tooLong(value, CheckDirtyFlag) || m_inputType->patternMismatch(value) || m_inputType->valueMissing(value)
        || m_inputType->hasBadInput() || customError();
    return !someError;
}

bool HTMLInputElement::getAllowedValueStep(Decimal* step) const
{
    return m_inputType->getAllowedValueStep(step);
}

StepRange HTMLInputElement::createStepRange(AnyStepHandling anyStepHandling) const
{
    return m_inputType->createStepRange(anyStepHandling);
}

#if ENABLE(DATALIST_ELEMENT)
Optional<Decimal> HTMLInputElement::findClosestTickMarkValue(const Decimal& value)
{
    return m_inputType->findClosestTickMarkValue(value);
}
#endif

ExceptionOr<void> HTMLInputElement::stepUp(int n)
{
    return m_inputType->stepUp(n);
}

ExceptionOr<void> HTMLInputElement::stepDown(int n)
{
    return m_inputType->stepUp(-n);
}

void HTMLInputElement::blur()
{
    m_inputType->blur();
}

void HTMLInputElement::defaultBlur()
{
    HTMLTextFormControlElement::blur();
}

bool HTMLInputElement::hasCustomFocusLogic() const
{
    return m_inputType->hasCustomFocusLogic();
}

int HTMLInputElement::defaultTabIndex() const
{
    return 0;
}

bool HTMLInputElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    return m_inputType->isKeyboardFocusable(event);
}

bool HTMLInputElement::isMouseFocusable() const
{
    return m_inputType->isMouseFocusable();
}

bool HTMLInputElement::isInteractiveContent() const
{
    return m_inputType->isInteractiveContent();
}

bool HTMLInputElement::isTextFormControlFocusable() const
{
    return HTMLTextFormControlElement::isFocusable();
}

bool HTMLInputElement::isTextFormControlKeyboardFocusable(KeyboardEvent* event) const
{
    return HTMLTextFormControlElement::isKeyboardFocusable(event);
}

bool HTMLInputElement::isTextFormControlMouseFocusable() const
{
    return HTMLTextFormControlElement::isMouseFocusable();
}

void HTMLInputElement::updateFocusAppearance(SelectionRestorationMode restorationMode, SelectionRevealMode revealMode)
{
    if (isTextField()) {
        if (restorationMode == SelectionRestorationMode::SetDefault || !hasCachedSelection())
            setDefaultSelectionAfterFocus(revealMode);
        else
            restoreCachedSelection(revealMode);
    } else
        HTMLTextFormControlElement::updateFocusAppearance(restorationMode, revealMode);
}

void HTMLInputElement::setDefaultSelectionAfterFocus(SelectionRevealMode revealMode)
{
    ASSERT(isTextField());
    int start = 0;
    auto direction = SelectionHasNoDirection;
    auto* frame = document().frame();
    if (frame && frame->editor().behavior().shouldMoveSelectionToEndWhenFocusingTextInput()) {
        start = std::numeric_limits<int>::max();
        direction = SelectionHasForwardDirection;
    }
    setSelectionRange(start, std::numeric_limits<int>::max(), direction, revealMode, Element::defaultFocusTextStateChangeIntent());
}

void HTMLInputElement::endEditing()
{
    if (!isTextField())
        return;

    if (RefPtr<Frame> frame = document().frame())
        frame->editor().textFieldDidEndEditing(this);
}

bool HTMLInputElement::shouldUseInputMethod()
{
    return m_inputType->shouldUseInputMethod();
}

void HTMLInputElement::handleFocusEvent(Node* oldFocusedNode, FocusDirection direction)
{
    m_inputType->handleFocusEvent(oldFocusedNode, direction);
}

void HTMLInputElement::handleBlurEvent()
{
    m_inputType->handleBlurEvent();
}

void HTMLInputElement::setType(const AtomString& type)
{
    setAttributeWithoutSynchronization(typeAttr, type);
}

void HTMLInputElement::resignStrongPasswordAppearance()
{
    if (!hasAutoFillStrongPasswordButton())
        return;
    setAutoFilled(false);
    setAutoFilledAndViewable(false);
    setShowAutoFillButton(AutoFillButtonType::None);
    if (auto* page = document().page())
        page->chrome().client().inputElementDidResignStrongPasswordAppearance(*this);
}

void HTMLInputElement::updateType()
{
    ASSERT(m_inputType);
    auto newType = InputType::create(*this, attributeWithoutSynchronization(typeAttr));
    m_hasType = true;
    if (m_inputType->formControlType() == newType->formControlType())
        return;

    removeFromRadioButtonGroup();
    resignStrongPasswordAppearance();

    bool didStoreValue = m_inputType->storesValueSeparateFromAttribute();
    bool willStoreValue = newType->storesValueSeparateFromAttribute();
    bool neededSuspensionCallback = needsSuspensionCallback();
    bool didRespectHeightAndWidth = m_inputType->shouldRespectHeightAndWidthAttributes();
    bool wasSuccessfulSubmitButtonCandidate = m_inputType->canBeSuccessfulSubmitButton();

    if (didStoreValue && !willStoreValue && hasDirtyValue()) {
        setAttributeWithoutSynchronization(valueAttr, m_valueIfDirty);
        m_valueIfDirty = String();
    }

    m_inputType->destroyShadowSubtree();
    m_inputType->detachFromElement();

    m_inputType = WTFMove(newType);
    m_inputType->createShadowSubtree();
    updateInnerTextElementEditability();

    setNeedsWillValidateCheck();

    if (!didStoreValue && willStoreValue)
        m_valueIfDirty = sanitizeValue(attributeWithoutSynchronization(valueAttr));
    else
        updateValueIfNeeded();

    setFormControlValueMatchesRenderer(false);
    m_inputType->updateInnerTextValue();

    m_wasModifiedByUser = false;

    if (neededSuspensionCallback)
        unregisterForSuspensionCallbackIfNeeded();
    else
        registerForSuspensionCallbackIfNeeded();

    if (didRespectHeightAndWidth != m_inputType->shouldRespectHeightAndWidthAttributes()) {
        ASSERT(elementData());
        // FIXME: We don't have the old attribute values so we pretend that we didn't have the old values.
        if (const Attribute* height = findAttributeByName(heightAttr))
            attributeChanged(heightAttr, nullAtom(), height->value());
        if (const Attribute* width = findAttributeByName(widthAttr))
            attributeChanged(widthAttr, nullAtom(), width->value());
        if (const Attribute* align = findAttributeByName(alignAttr))
            attributeChanged(alignAttr, nullAtom(), align->value());
    }

    if (form() && wasSuccessfulSubmitButtonCandidate != m_inputType->canBeSuccessfulSubmitButton())
        form()->resetDefaultButton();

    runPostTypeUpdateTasks();
}

inline void HTMLInputElement::runPostTypeUpdateTasks()
{
    ASSERT(m_inputType);
#if ENABLE(TOUCH_EVENTS)
    bool hasTouchEventHandler = m_inputType->hasTouchEventHandler();
    if (hasTouchEventHandler != m_hasTouchEventHandler) {
        if (hasTouchEventHandler)
            document().didAddTouchEventHandler(*this);
        else
            document().didRemoveTouchEventHandler(*this);
        m_hasTouchEventHandler = hasTouchEventHandler;
    }
#endif

    if (renderer())
        invalidateStyleAndRenderersForSubtree();

    if (document().focusedElement() == this)
        updateFocusAppearance(SelectionRestorationMode::Restore, SelectionRevealMode::Reveal);

    setChangedSinceLastFormControlChangeEvent(false);

    addToRadioButtonGroup();

    updateValidity();
}

void HTMLInputElement::subtreeHasChanged()
{
    m_inputType->subtreeHasChanged();
    // When typing in an input field, childrenChanged is not called, so we need to force the directionality check.
    calculateAndAdjustDirectionality();
}

const AtomString& HTMLInputElement::formControlType() const
{
    return m_inputType->formControlType();
}

bool HTMLInputElement::shouldSaveAndRestoreFormControlState() const
{
    if (!m_inputType->shouldSaveAndRestoreFormControlState())
        return false;
    return HTMLTextFormControlElement::shouldSaveAndRestoreFormControlState();
}

FormControlState HTMLInputElement::saveFormControlState() const
{
    return m_inputType->saveFormControlState();
}

void HTMLInputElement::restoreFormControlState(const FormControlState& state)
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

bool HTMLInputElement::accessKeyAction(bool sendMouseEvents)
{
    Ref<InputType> protectedInputType(*m_inputType);
    return protectedInputType->accessKeyAction(sendMouseEvents);
}

bool HTMLInputElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == vspaceAttr || name == hspaceAttr || name == widthAttr || name == heightAttr || (name == borderAttr && isImageButton()))
        return true;
    return HTMLTextFormControlElement::isPresentationAttribute(name);
}

void HTMLInputElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == vspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
    } else if (name == hspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
    } else if (name == alignAttr) {
        if (m_inputType->shouldRespectAlignAttribute())
            applyAlignmentAttributeToStyle(value, style);
    } else if (name == widthAttr) {
        if (m_inputType->shouldRespectHeightAndWidthAttributes())
            addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    } else if (name == heightAttr) {
        if (m_inputType->shouldRespectHeightAndWidthAttributes())
            addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    } else if (name == borderAttr && isImageButton())
        applyBorderAttributeToStyle(value, style);
    else
        HTMLTextFormControlElement::collectStyleForPresentationAttribute(name, value, style);
}

inline void HTMLInputElement::initializeInputType()
{
    ASSERT(m_parsingInProgress);
    ASSERT(!m_inputType);

    const AtomString& type = attributeWithoutSynchronization(typeAttr);
    if (type.isNull()) {
        m_inputType = InputType::createText(*this);
        ensureUserAgentShadowRoot();
        setNeedsWillValidateCheck();
        return;
    }

    m_hasType = true;
    m_inputType = InputType::create(*this, type);
    ensureUserAgentShadowRoot();
    setNeedsWillValidateCheck();
    registerForSuspensionCallbackIfNeeded();
    runPostTypeUpdateTasks();
}

void HTMLInputElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    ASSERT(m_inputType);
    Ref<InputType> protectedInputType(*m_inputType);

    if (name == nameAttr) {
        removeFromRadioButtonGroup();
        m_name = value;
        addToRadioButtonGroup();
        HTMLTextFormControlElement::parseAttribute(name, value);
    } else if (name == autocompleteAttr) {
        if (equalLettersIgnoringASCIICase(value, "off")) {
            m_autocomplete = Off;
            registerForSuspensionCallbackIfNeeded();
        } else {
            bool needsToUnregister = m_autocomplete == Off;

            if (value.isEmpty())
                m_autocomplete = Uninitialized;
            else
                m_autocomplete = On;

            if (needsToUnregister)
                unregisterForSuspensionCallbackIfNeeded();
        }
    } else if (name == typeAttr)
        updateType();
    else if (name == valueAttr) {
        // Changes to the value attribute may change whether or not this element has a default value.
        // If this field is autocomplete=off that might affect the return value of needsSuspensionCallback.
        if (m_autocomplete == Off) {
            unregisterForSuspensionCallbackIfNeeded();
            registerForSuspensionCallbackIfNeeded();
        }
        // We only need to setChanged if the form is looking at the default value right now.
        if (!hasDirtyValue()) {
            updatePlaceholderVisibility();
            invalidateStyleForSubtree();
        }
        setFormControlValueMatchesRenderer(false);
        updateValidity();
        m_valueAttributeWasUpdatedAfterParsing = !m_parsingInProgress;
    } else if (name == checkedAttr) {
        if (m_inputType->isCheckable())
            invalidateStyleForSubtree();

        // Another radio button in the same group might be checked by state
        // restore. We shouldn't call setChecked() even if this has the checked
        // attribute. So, delay the setChecked() call until
        // finishParsingChildren() is called if parsing is in progress.
        if ((!m_parsingInProgress || !document().formController().hasFormStateToRestore()) && !m_dirtyCheckednessFlag) {
            setChecked(!value.isNull());
            // setChecked() above sets the dirty checkedness flag so we need to reset it.
            m_dirtyCheckednessFlag = false;
        }
    } else if (name == maxlengthAttr)
        maxLengthAttributeChanged(value);
    else if (name == minlengthAttr)
        minLengthAttributeChanged(value);
    else if (name == sizeAttr) {
        unsigned oldSize = m_size;
        m_size = limitToOnlyHTMLNonNegativeNumbersGreaterThanZero(value, defaultSize);
        if (m_size != oldSize && renderer())
            renderer()->setNeedsLayoutAndPrefWidthsRecalc();
    } else if (name == resultsAttr)
        m_maxResults = !value.isNull() ? std::min(value.toInt(), maxSavedResults) : -1;
    else if (name == autosaveAttr || name == incrementalAttr)
        invalidateStyleForSubtree();
    else if (name == maxAttr || name == minAttr || name == multipleAttr || name == patternAttr || name == precisionAttr || name == stepAttr)
        updateValidity();
#if ENABLE(DATALIST_ELEMENT)
    else if (name == listAttr) {
        m_hasNonEmptyList = !value.isEmpty();
        if (m_hasNonEmptyList) {
            resetListAttributeTargetObserver();
            dataListMayHaveChanged();
        }
    }
#endif
    else
        HTMLTextFormControlElement::parseAttribute(name, value);

    m_inputType->attributeChanged(name);
}

void HTMLInputElement::disabledStateChanged()
{
    HTMLTextFormControlElement::disabledStateChanged();
    m_inputType->disabledStateChanged();
}

void HTMLInputElement::readOnlyStateChanged()
{
    HTMLTextFormControlElement::readOnlyStateChanged();
    m_inputType->readOnlyStateChanged();
}

void HTMLInputElement::parserDidSetAttributes()
{
    ASSERT(m_parsingInProgress);
    initializeInputType();
}

void HTMLInputElement::finishParsingChildren()
{
    m_parsingInProgress = false;
    ASSERT(m_inputType);
    HTMLTextFormControlElement::finishParsingChildren();
    if (!m_stateRestored) {
        bool checked = hasAttributeWithoutSynchronization(checkedAttr);
        if (checked)
            setChecked(checked);
        m_dirtyCheckednessFlag = false;
    }
}

bool HTMLInputElement::rendererIsNeeded(const RenderStyle& style)
{
    return m_inputType->rendererIsNeeded() && HTMLTextFormControlElement::rendererIsNeeded(style);
}

RenderPtr<RenderElement> HTMLInputElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return m_inputType->createInputRenderer(WTFMove(style));
}

void HTMLInputElement::willAttachRenderers()
{
    if (!m_hasType)
        updateType();
}

void HTMLInputElement::didAttachRenderers()
{
    HTMLTextFormControlElement::didAttachRenderers();

    m_inputType->attach();

    if (document().focusedElement() == this) {
        document().view()->queuePostLayoutCallback([protectedThis = makeRef(*this)] {
            protectedThis->updateFocusAppearance(SelectionRestorationMode::Restore, SelectionRevealMode::Reveal);
        });
    }
}

void HTMLInputElement::didDetachRenderers()
{
    setFormControlValueMatchesRenderer(false);
    m_inputType->detach();
}

String HTMLInputElement::altText() const
{
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    // note this is intentionally different to HTMLImageElement::altText()
    String alt = attributeWithoutSynchronization(altAttr);
    // fall back to title attribute
    if (alt.isNull())
        alt = attributeWithoutSynchronization(titleAttr);
    if (alt.isNull())
        alt = attributeWithoutSynchronization(valueAttr);
    if (alt.isEmpty())
        alt = inputElementAltText();
    return alt;
}

bool HTMLInputElement::isSuccessfulSubmitButton() const
{
    // HTML spec says that buttons must have names to be considered successful.
    // However, other browsers do not impose this constraint. So we do not.
    return !isDisabledFormControl() && m_inputType->canBeSuccessfulSubmitButton();
}

bool HTMLInputElement::matchesDefaultPseudoClass() const
{
    ASSERT(m_inputType);
    if (m_inputType->canBeSuccessfulSubmitButton())
        return !isDisabledFormControl() && form() && form()->defaultButton() == this;
    return m_inputType->isCheckable() && hasAttributeWithoutSynchronization(checkedAttr);
}

bool HTMLInputElement::isActivatedSubmit() const
{
    return m_isActivatedSubmit;
}

void HTMLInputElement::setActivatedSubmit(bool flag)
{
    m_isActivatedSubmit = flag;
}

bool HTMLInputElement::appendFormData(DOMFormData& formData, bool multipart)
{
    Ref<InputType> protectedInputType(*m_inputType);
    return m_inputType->isFormDataAppendable() && m_inputType->appendFormData(formData, multipart);
}

void HTMLInputElement::reset()
{
    if (m_inputType->storesValueSeparateFromAttribute())
        setValue(String());

    setAutoFilled(false);
    setAutoFilledAndViewable(false);
    setShowAutoFillButton(AutoFillButtonType::None);
    setChecked(hasAttributeWithoutSynchronization(checkedAttr));
    m_dirtyCheckednessFlag = false;
}

bool HTMLInputElement::isTextField() const
{
    return m_inputType->isTextField();
}

bool HTMLInputElement::isTextType() const
{
    return m_inputType->isTextType();
}

void HTMLInputElement::setChecked(bool nowChecked)
{
    if (checked() == nowChecked)
        return;

    m_dirtyCheckednessFlag = true;
    m_isChecked = nowChecked;
    invalidateStyleForSubtree();

    if (RadioButtonGroups* buttons = radioButtonGroups())
        buttons->updateCheckedState(*this);
    if (renderer() && renderer()->style().hasAppearance())
        renderer()->theme().stateChanged(*renderer(), ControlStates::CheckedState);
    updateValidity();

    // Ideally we'd do this from the render tree (matching
    // RenderTextView), but it's not possible to do it at the moment
    // because of the way the code is structured.
    if (renderer()) {
        if (AXObjectCache* cache = renderer()->document().existingAXObjectCache())
            cache->checkedStateChanged(this);
    }

    invalidateStyleForSubtree();
}

void HTMLInputElement::setIndeterminate(bool newValue)
{
    if (indeterminate() == newValue)
        return;

    m_isIndeterminate = newValue;

    invalidateStyleForSubtree();

    if (renderer() && renderer()->style().hasAppearance())
        renderer()->theme().stateChanged(*renderer(), ControlStates::CheckedState);
}

unsigned HTMLInputElement::size() const
{
    return m_size;
}

bool HTMLInputElement::sizeShouldIncludeDecoration(int& preferredSize) const
{
    return m_inputType->sizeShouldIncludeDecoration(defaultSize, preferredSize);
}

float HTMLInputElement::decorationWidth() const
{
    return m_inputType->decorationWidth();
}

void HTMLInputElement::copyNonAttributePropertiesFromElement(const Element& source)
{
    auto& sourceElement = downcast<HTMLInputElement>(source);

    m_valueIfDirty = sourceElement.m_valueIfDirty;
    m_wasModifiedByUser = false;
    setChecked(sourceElement.m_isChecked);
    m_dirtyCheckednessFlag = sourceElement.m_dirtyCheckednessFlag;
    m_isIndeterminate = sourceElement.m_isIndeterminate;

    HTMLTextFormControlElement::copyNonAttributePropertiesFromElement(source);

    updateValidity();
    setFormControlValueMatchesRenderer(false);
    m_inputType->updateInnerTextValue();
}

String HTMLInputElement::value() const
{
    String value;
    if (m_inputType->getTypeSpecificValue(value))
        return value;

    value = m_valueIfDirty;
    if (!value.isNull())
        return value;

    auto& valueString = attributeWithoutSynchronization(valueAttr);
    value = sanitizeValue(valueString);
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
    setValue(value, DispatchChangeEvent);
}

ExceptionOr<void> HTMLInputElement::setValue(const String& value, TextFieldEventBehavior eventBehavior)
{
    if (isFileUpload() && !value.isEmpty())
        return Exception { InvalidStateError };

    if (!m_inputType->canSetValue(value))
        return { };

    Ref<HTMLInputElement> protectedThis(*this);
    EventQueueScope scope;
    String sanitizedValue = sanitizeValue(value);
    bool valueChanged = sanitizedValue != this->value();

    setLastChangeWasNotUserEdit();
    setFormControlValueMatchesRenderer(false);
    m_inputType->setValue(sanitizedValue, valueChanged, eventBehavior);

    bool wasModifiedProgrammatically = eventBehavior == DispatchNoEvent;
    if (wasModifiedProgrammatically)
        resignStrongPasswordAppearance();
    return { };
}

void HTMLInputElement::setValueInternal(const String& sanitizedValue, TextFieldEventBehavior eventBehavior)
{
    m_valueIfDirty = sanitizedValue;
    m_wasModifiedByUser = eventBehavior != DispatchNoEvent;
    updateValidity();
}

double HTMLInputElement::valueAsDate() const
{
    return m_inputType->valueAsDate();
}

ExceptionOr<void> HTMLInputElement::setValueAsDate(double value)
{
    return m_inputType->setValueAsDate(value);
}

double HTMLInputElement::valueAsNumber() const
{
    return m_inputType->valueAsDouble();
}

ExceptionOr<void> HTMLInputElement::setValueAsNumber(double newValue, TextFieldEventBehavior eventBehavior)
{
    if (!std::isfinite(newValue))
        return Exception { NotSupportedError };
    return m_inputType->setValueAsDouble(newValue, eventBehavior);
}

void HTMLInputElement::setValueFromRenderer(const String& value)
{
    // File upload controls will never use this.
    ASSERT(!isFileUpload());

    // Renderer and our event handler are responsible for sanitizing values.
    // Input types that support the selection API do *not* sanitize their
    // user input in order to retain parity between what's in the model and
    // what's on the screen.
    ASSERT(m_inputType->supportsSelectionAPI() || value == sanitizeValue(value) || sanitizeValue(value).isEmpty());

    // Workaround for bug where trailing \n is included in the result of textContent.
    // The assert macro above may also be simplified by removing the expression
    // that calls isEmpty.
    // http://bugs.webkit.org/show_bug.cgi?id=9661
    m_valueIfDirty = value == "\n" ? emptyString() : value;

    setFormControlValueMatchesRenderer(true);
    m_wasModifiedByUser = true;

    // Input event is fired by the Node::defaultEventHandler for editable controls.
    if (!isTextField())
        dispatchInputEvent();

    updateValidity();

    // Clear auto fill flag (and yellow background) on user edit.
    setAutoFilled(false);
}

void HTMLInputElement::willDispatchEvent(Event& event, InputElementClickState& state)
{
    if (event.type() == eventNames().textInputEvent && m_inputType->shouldSubmitImplicitly(event))
        event.stopPropagation();
    if (event.type() == eventNames().clickEvent && is<MouseEvent>(event) && downcast<MouseEvent>(event).button() == LeftButton) {
        m_inputType->willDispatchClick(state);
        state.stateful = true;
    }
}

void HTMLInputElement::didDispatchClickEvent(Event& event, const InputElementClickState& state)
{
    m_inputType->didDispatchClick(event, state);
}

void HTMLInputElement::didBlur()
{
    m_inputType->elementDidBlur();
}

void HTMLInputElement::defaultEventHandler(Event& event)
{
    if (is<MouseEvent>(event) && event.type() == eventNames().clickEvent && downcast<MouseEvent>(event).button() == LeftButton) {
        m_inputType->handleClickEvent(downcast<MouseEvent>(event));
        if (event.defaultHandled())
            return;
    }

#if ENABLE(TOUCH_EVENTS)
    if (is<TouchEvent>(event)) {
        m_inputType->handleTouchEvent(downcast<TouchEvent>(event));
        if (event.defaultHandled())
            return;
    }
#endif

    if (is<KeyboardEvent>(event) && event.type() == eventNames().keydownEvent) {
        auto shouldCallBaseEventHandler = m_inputType->handleKeydownEvent(downcast<KeyboardEvent>(event));
        if (event.defaultHandled() || shouldCallBaseEventHandler == InputType::ShouldCallBaseEventHandler::No)
            return;
    }

    // Call the base event handler before any of our own event handling for almost all events in text fields.
    // Makes editing keyboard handling take precedence over the keydown and keypress handling in this function.
    bool callBaseClassEarly = isTextField() && (event.type() == eventNames().keydownEvent || event.type() == eventNames().keypressEvent);
    if (callBaseClassEarly) {
        HTMLTextFormControlElement::defaultEventHandler(event);
        if (event.defaultHandled())
            return;
    }

    // DOMActivate events cause the input to be "activated" - in the case of image and submit inputs, this means
    // actually submitting the form. For reset inputs, the form is reset. These events are sent when the user clicks
    // on the element, or presses enter while it is the active element. JavaScript code wishing to activate the element
    // must dispatch a DOMActivate event - a click event will not do the job.
    if (event.type() == eventNames().DOMActivateEvent) {
        m_inputType->handleDOMActivateEvent(event);
        if (event.defaultHandled())
            return;
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (is<KeyboardEvent>(event)) {
        KeyboardEvent& keyboardEvent = downcast<KeyboardEvent>(event);
        if (keyboardEvent.type() == eventNames().keypressEvent) {
            m_inputType->handleKeypressEvent(keyboardEvent);
            if (keyboardEvent.defaultHandled())
                return;
        } else if (keyboardEvent.type() == eventNames().keyupEvent) {
            m_inputType->handleKeyupEvent(keyboardEvent);
            if (keyboardEvent.defaultHandled())
                return;
        }
    }

    if (m_inputType->shouldSubmitImplicitly(event)) {
        if (isSearchField()) {
            addSearchResult();
            onSearch();
        }
        // Form submission finishes editing, just as loss of focus does.
        // If there was a change, send the event now.
        if (wasChangedSinceLastFormControlChangeEvent())
            dispatchFormControlChangeEvent();

        // Form may never have been present, or may have been destroyed by code responding to the change event.
        if (auto formElement = makeRefPtr(form()))
            formElement->submitImplicitly(event, canTriggerImplicitSubmission());

        event.setDefaultHandled();
        return;
    }

    if (is<BeforeTextInsertedEvent>(event))
        m_inputType->handleBeforeTextInsertedEvent(downcast<BeforeTextInsertedEvent>(event));

    if (is<MouseEvent>(event) && event.type() == eventNames().mousedownEvent) {
        m_inputType->handleMouseDownEvent(downcast<MouseEvent>(event));
        if (event.defaultHandled())
            return;
    }

    m_inputType->forwardEvent(event);

    if (!callBaseClassEarly && !event.defaultHandled())
        HTMLTextFormControlElement::defaultEventHandler(event);
}

bool HTMLInputElement::willRespondToMouseClickEvents()
{
    if (!isDisabledFormControl())
        return true;

    return HTMLTextFormControlElement::willRespondToMouseClickEvents();
}

bool HTMLInputElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || attribute.name() == formactionAttr || HTMLTextFormControlElement::isURLAttribute(attribute);
}

String HTMLInputElement::defaultValue() const
{
    return attributeWithoutSynchronization(valueAttr);
}

void HTMLInputElement::setDefaultValue(const String &value)
{
    setAttributeWithoutSynchronization(valueAttr, value);
}

static inline bool isRFC2616TokenCharacter(UChar ch)
{
    return isASCII(ch) && ch > ' ' && ch != '"' && ch != '(' && ch != ')' && ch != ',' && ch != '/' && (ch < ':' || ch > '@') && (ch < '[' || ch > ']') && ch != '{' && ch != '}' && ch != 0x7f;
}

static bool isValidMIMEType(const String& type)
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

static bool isValidFileExtension(const String& type)
{
    if (type.length() < 2)
        return false;
    return type[0] == '.';
}

static Vector<String> parseAcceptAttribute(const String& acceptString, bool (*predicate)(const String&))
{
    Vector<String> types;
    if (acceptString.isEmpty())
        return types;

    for (auto& splitType : acceptString.split(',')) {
        String trimmedType = stripLeadingAndTrailingHTMLSpaces(splitType);
        if (trimmedType.isEmpty())
            continue;
        if (!predicate(trimmedType))
            continue;
        types.append(trimmedType.convertToASCIILowercase());
    }

    return types;
}

Vector<String> HTMLInputElement::acceptMIMETypes()
{
    return parseAcceptAttribute(attributeWithoutSynchronization(acceptAttr), isValidMIMEType);
}

Vector<String> HTMLInputElement::acceptFileExtensions()
{
    return parseAcceptAttribute(attributeWithoutSynchronization(acceptAttr), isValidFileExtension);
}

String HTMLInputElement::accept() const
{
    return attributeWithoutSynchronization(acceptAttr);
}

String HTMLInputElement::alt() const
{
    return attributeWithoutSynchronization(altAttr);
}

unsigned HTMLInputElement::effectiveMaxLength() const
{
    // The number -1 represents no maximum at all; conveniently it becomes a super-large value when converted to unsigned.
    return std::min<unsigned>(maxLength(), maxEffectiveLength);
}

bool HTMLInputElement::multiple() const
{
    return hasAttributeWithoutSynchronization(multipleAttr);
}

ExceptionOr<void> HTMLInputElement::setSize(unsigned size)
{
    if (!size)
        return Exception { IndexSizeError };
    setUnsignedIntegralAttribute(sizeAttr, limitToOnlyHTMLNonNegativeNumbersGreaterThanZero(size, defaultSize));
    return { };
}

URL HTMLInputElement::src() const
{
    return document().completeURL(attributeWithoutSynchronization(srcAttr));
}

void HTMLInputElement::setAutoFilled(bool autoFilled)
{
    if (autoFilled == m_isAutoFilled)
        return;

    m_isAutoFilled = autoFilled;
    invalidateStyleForSubtree();
}

void HTMLInputElement::setAutoFilledAndViewable(bool autoFilledAndViewable)
{
    if (autoFilledAndViewable == m_isAutoFilledAndViewable)
        return;

    m_isAutoFilledAndViewable = autoFilledAndViewable;
    invalidateStyleForSubtree();
}

void HTMLInputElement::setShowAutoFillButton(AutoFillButtonType autoFillButtonType)
{
    if (static_cast<uint8_t>(autoFillButtonType) == m_autoFillButtonType)
        return;

    m_lastAutoFillButtonType = m_autoFillButtonType;
    m_autoFillButtonType = static_cast<uint8_t>(autoFillButtonType);
    m_inputType->updateAutoFillButton();
    updateInnerTextElementEditability();
    invalidateStyleForSubtree();
}

FileList* HTMLInputElement::files()
{
    return m_inputType->files();
}

void HTMLInputElement::setFiles(RefPtr<FileList>&& files)
{
    m_inputType->setFiles(WTFMove(files));
}

#if ENABLE(DRAG_SUPPORT)
bool HTMLInputElement::receiveDroppedFiles(const DragData& dragData)
{
    return m_inputType->receiveDroppedFiles(dragData);
}
#endif

Icon* HTMLInputElement::icon() const
{
    return m_inputType->icon();
}

String HTMLInputElement::displayString() const
{
    return m_inputType->displayString();
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
    if (renderer())
        renderer()->updateFromElement();
}

String HTMLInputElement::visibleValue() const
{
    return m_inputType->visibleValue();
}

String HTMLInputElement::sanitizeValue(const String& proposedValue) const
{
    if (proposedValue.isNull())
        return proposedValue;
    return m_inputType->sanitizeValue(proposedValue);
}

String HTMLInputElement::localizeValue(const String& proposedValue) const
{
    if (proposedValue.isNull())
        return proposedValue;
    return m_inputType->localizeValue(proposedValue);
}

bool HTMLInputElement::isInRange() const
{
    return willValidate() && m_inputType->isInRange(value());
}

bool HTMLInputElement::isOutOfRange() const
{
    return willValidate() && m_inputType->isOutOfRange(value());
}

bool HTMLInputElement::needsSuspensionCallback()
{
    if (m_inputType->shouldResetOnDocumentActivation())
        return true;

    // Sensitive input elements are marked with autocomplete=off, and we want to wipe them out
    // when going back; returning true here arranges for us to call reset at the time
    // the page is restored. Non-empty textual default values indicate that the field
    // is not really sensitive -- there's no default value for an account number --
    // and we would see unexpected results if we reset to something other than blank.
    bool isSensitive = m_autocomplete == Off && !(m_inputType->isTextType() && !defaultValue().isEmpty());

    return isSensitive;
}

void HTMLInputElement::registerForSuspensionCallbackIfNeeded()
{
    if (needsSuspensionCallback())
        document().registerForDocumentSuspensionCallbacks(*this);
}

void HTMLInputElement::unregisterForSuspensionCallbackIfNeeded()
{
    if (!needsSuspensionCallback())
        document().unregisterForDocumentSuspensionCallbacks(*this);
}

bool HTMLInputElement::isRequiredFormControl() const
{
    return m_inputType->supportsRequired() && isRequired();
}

bool HTMLInputElement::matchesReadWritePseudoClass() const
{
    return m_inputType->supportsReadOnly() && !isDisabledOrReadOnly();
}

void HTMLInputElement::addSearchResult()
{
    m_inputType->addSearchResult();
}

void HTMLInputElement::onSearch()
{
    // The type of the input element could have changed during event handling. If we are no longer
    // a search field, don't try to do search things.
    if (!isSearchField())
        return;

    if (m_inputType)
        downcast<SearchInputType>(*m_inputType.get()).stopSearchEventTimer();
    dispatchEvent(Event::create(eventNames().searchEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
}

void HTMLInputElement::resumeFromDocumentSuspension()
{
    ASSERT(needsSuspensionCallback());

#if ENABLE(INPUT_TYPE_COLOR)
    // <input type=color> uses prepareForDocumentSuspension to detach the color picker UI,
    // so it should not be reset when being loaded from page cache.
    if (isColorControl())
        return;
#endif // ENABLE(INPUT_TYPE_COLOR)
    document().postTask([inputElement = makeRef(*this)] (ScriptExecutionContext&) {
        inputElement->reset();
    });
}

#if ENABLE(INPUT_TYPE_COLOR)
void HTMLInputElement::prepareForDocumentSuspension()
{
    if (!isColorControl())
        return;
    m_inputType->detach();
}
#endif // ENABLE(INPUT_TYPE_COLOR)


void HTMLInputElement::willChangeForm()
{
    removeFromRadioButtonGroup();
    HTMLTextFormControlElement::willChangeForm();
}

void HTMLInputElement::didChangeForm()
{
    HTMLTextFormControlElement::didChangeForm();
    addToRadioButtonGroup();
}

Node::InsertedIntoAncestorResult HTMLInputElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLTextFormControlElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
#if ENABLE(DATALIST_ELEMENT)
    resetListAttributeTargetObserver();
#endif
    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void HTMLInputElement::didFinishInsertingNode()
{
    HTMLTextFormControlElement::didFinishInsertingNode();
    if (isInTreeScope() && !form())
        addToRadioButtonGroup();
#if ENABLE(DATALIST_ELEMENT)
    if (isConnected() && m_hasNonEmptyList)
        dataListMayHaveChanged();
#endif
}

void HTMLInputElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    if (removalType.treeScopeChanged && isRadioButton())
        oldParentOfRemovedTree.treeScope().radioButtonGroups().removeButton(*this);
    if (removalType.disconnectedFromDocument && !form())
        removeFromRadioButtonGroup();
    HTMLTextFormControlElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    ASSERT(!isConnected());
#if ENABLE(DATALIST_ELEMENT)
    resetListAttributeTargetObserver();
#endif
}

void HTMLInputElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    if (imageLoader())
        imageLoader()->elementDidMoveToNewDocument(oldDocument);

    // Always unregister for cache callbacks when leaving a document, even if we would otherwise like to be registered
    if (needsSuspensionCallback()) {
        oldDocument.unregisterForDocumentSuspensionCallbacks(*this);
        newDocument.registerForDocumentSuspensionCallbacks(*this);
    }

#if ENABLE(TOUCH_EVENTS)
    if (m_hasTouchEventHandler) {
        oldDocument.didRemoveEventTargetNode(*this);
        newDocument.didAddTouchEventHandler(*this);
    }
#endif

    HTMLTextFormControlElement::didMoveToNewDocument(oldDocument, newDocument);
}

void HTMLInputElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLTextFormControlElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, src());
}

bool HTMLInputElement::computeWillValidate() const
{
    return m_inputType->supportsValidation() && HTMLTextFormControlElement::computeWillValidate();
}

void HTMLInputElement::requiredStateChanged()
{
    HTMLTextFormControlElement::requiredStateChanged();
    if (auto* buttons = radioButtonGroups())
        buttons->requiredStateChanged(*this);
    m_inputType->requiredStateChanged();
}

Color HTMLInputElement::valueAsColor() const
{
    return m_inputType->valueAsColor();
}

void HTMLInputElement::selectColor(StringView color)
{
    m_inputType->selectColor(color);
}

Vector<Color> HTMLInputElement::suggestedColors() const
{
    return m_inputType->suggestedColors();
}

#if ENABLE(DATALIST_ELEMENT)

RefPtr<HTMLElement> HTMLInputElement::list() const
{
    return dataList();
}

RefPtr<HTMLDataListElement> HTMLInputElement::dataList() const
{
    if (!m_hasNonEmptyList)
        return nullptr;

    if (!m_inputType->shouldRespectListAttribute())
        return nullptr;

    RefPtr<Element> element = treeScope().getElementById(attributeWithoutSynchronization(listAttr));
    if (!is<HTMLDataListElement>(element))
        return nullptr;

    return downcast<HTMLDataListElement>(element.get());
}

void HTMLInputElement::resetListAttributeTargetObserver()
{
    if (isConnected())
        m_listAttributeTargetObserver = makeUnique<ListAttributeTargetObserver>(attributeWithoutSynchronization(listAttr), this);
    else
        m_listAttributeTargetObserver = nullptr;
}

void HTMLInputElement::dataListMayHaveChanged()
{
    m_inputType->dataListMayHaveChanged();
}

#endif // ENABLE(DATALIST_ELEMENT)

bool HTMLInputElement::isPresentingAttachedView() const
{
    return m_inputType->isPresentingAttachedView();
}

bool HTMLInputElement::isSteppable() const
{
    return m_inputType->isSteppable();
}

DateComponents::Type HTMLInputElement::dateType() const
{
    return m_inputType->dateType();
}

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

#if ENABLE(INPUT_TYPE_COLOR)
bool HTMLInputElement::isColorControl() const
{
    return m_inputType->isColorControl();
}
#endif

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

bool HTMLInputElement::isDateField() const
{
    return m_inputType->isDateField();
}

bool HTMLInputElement::isDateTimeLocalField() const
{
    return m_inputType->isDateTimeLocalField();
}

bool HTMLInputElement::isMonthField() const
{
    return m_inputType->isMonthField();
}

bool HTMLInputElement::isTimeField() const
{
    return m_inputType->isTimeField();
}

bool HTMLInputElement::isWeekField() const
{
    return m_inputType->isWeekField();
}

bool HTMLInputElement::isEnumeratable() const
{
    return m_inputType->isEnumeratable();
}

bool HTMLInputElement::supportLabels() const
{
    return m_inputType->supportLabels();
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

bool HTMLInputElement::isEmptyValue() const
{
    return m_inputType->isEmptyValue();
}

void HTMLInputElement::maxLengthAttributeChanged(const AtomString& newValue)
{
    unsigned oldEffectiveMaxLength = effectiveMaxLength();
    internalSetMaxLength(parseHTMLNonNegativeInteger(newValue).value_or(-1));
    if (oldEffectiveMaxLength != effectiveMaxLength())
        updateValueIfNeeded();

    // FIXME: Do we really need to do this if the effective maxLength has not changed?
    invalidateStyleForSubtree();
    updateValidity();
}

void HTMLInputElement::minLengthAttributeChanged(const AtomString& newValue)
{
    int oldMinLength = minLength();
    internalSetMinLength(parseHTMLNonNegativeInteger(newValue).value_or(-1));
    if (oldMinLength != minLength())
        updateValueIfNeeded();

    // FIXME: Do we really need to do this if the effective minLength has not changed?
    invalidateStyleForSubtree();
    updateValidity();
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

bool HTMLInputElement::matchesIndeterminatePseudoClass() const
{
    // For input elements, matchesIndeterminatePseudoClass()
    // is not equivalent to shouldAppearIndeterminate() because of radio button.
    //
    // A group of radio button without any checked button is indeterminate
    // for the :indeterminate selector. On the other hand, RenderTheme
    // currently only supports single element being indeterminate.
    // Because of this, radio is indetermindate for CSS but not for render theme.
    return m_inputType->matchesIndeterminatePseudoClass();
}

bool HTMLInputElement::shouldAppearIndeterminate() const 
{
    return m_inputType->shouldAppearIndeterminate();
}

#if ENABLE(MEDIA_CAPTURE)
MediaCaptureType HTMLInputElement::mediaCaptureType() const
{
    if (!isFileUpload())
        return MediaCaptureTypeNone;
    
    auto& captureAttribute = attributeWithoutSynchronization(captureAttr);
    if (captureAttribute.isNull())
        return MediaCaptureTypeNone;
    
    if (equalLettersIgnoringASCIICase(captureAttribute, "user"))
        return MediaCaptureTypeUser;
    
    return MediaCaptureTypeEnvironment;
}
#endif

bool HTMLInputElement::isInRequiredRadioButtonGroup()
{
    ASSERT(isRadioButton());
    if (RadioButtonGroups* buttons = radioButtonGroups())
        return buttons->isInRequiredGroup(*this);
    return false;
}

Vector<Ref<HTMLInputElement>> HTMLInputElement::radioButtonGroup() const
{
    RadioButtonGroups* buttons = radioButtonGroups();
    if (!buttons)
        return { };
    return buttons->groupMembers(*this);
}

RefPtr<HTMLInputElement> HTMLInputElement::checkedRadioButtonForGroup() const
{
    if (RadioButtonGroups* buttons = radioButtonGroups())
        return buttons->checkedButtonForGroup(name());
    return nullptr;
}

RadioButtonGroups* HTMLInputElement::radioButtonGroups() const
{
    if (!isRadioButton())
        return nullptr;
    if (auto* formElement = form())
        return &formElement->radioButtonGroups();
    if (isInTreeScope())
        return &treeScope().radioButtonGroups();
    return nullptr;
}

inline void HTMLInputElement::addToRadioButtonGroup()
{
    if (auto* buttons = radioButtonGroups())
        buttons->addButton(*this);
}

inline void HTMLInputElement::removeFromRadioButtonGroup()
{
    if (auto* buttons = radioButtonGroups())
        buttons->removeButton(*this);
}

unsigned HTMLInputElement::height() const
{
    return m_inputType->height();
}

unsigned HTMLInputElement::width() const
{
    return m_inputType->width();
}

void HTMLInputElement::setHeight(unsigned height)
{
    setUnsignedIntegralAttribute(heightAttr, height);
}

void HTMLInputElement::setWidth(unsigned width)
{
    setUnsignedIntegralAttribute(widthAttr, width);
}

#if ENABLE(DATALIST_ELEMENT)
ListAttributeTargetObserver::ListAttributeTargetObserver(const AtomString& id, HTMLInputElement* element)
    : IdTargetObserver(element->treeScope().idTargetObserverRegistry(), id)
    , m_element(element)
{
}

void ListAttributeTargetObserver::idTargetChanged()
{
    m_element->dataListMayHaveChanged();
}
#endif

ExceptionOr<void> HTMLInputElement::setRangeText(const String& replacement)
{
    if (!m_inputType->supportsSelectionAPI())
        return Exception { InvalidStateError };

    return HTMLTextFormControlElement::setRangeText(replacement);
}

ExceptionOr<void> HTMLInputElement::setRangeText(const String& replacement, unsigned start, unsigned end, const String& selectionMode)
{
    if (!m_inputType->supportsSelectionAPI())
        return Exception { InvalidStateError };

    return HTMLTextFormControlElement::setRangeText(replacement, start, end, selectionMode);
}

bool HTMLInputElement::shouldTruncateText(const RenderStyle& style) const
{
    if (!isTextField())
        return false;
    return document().focusedElement() != this && style.textOverflow() == TextOverflow::Ellipsis;
}

ExceptionOr<int> HTMLInputElement::selectionStartForBindings() const
{
    if (!canHaveSelection())
        return Exception { TypeError };

    return selectionStart();
}

ExceptionOr<void> HTMLInputElement::setSelectionStartForBindings(int start)
{
    if (!canHaveSelection())
        return Exception { TypeError };

    setSelectionStart(start);
    return { };
}

ExceptionOr<int> HTMLInputElement::selectionEndForBindings() const
{
    if (!canHaveSelection())
        return Exception { TypeError };

    return selectionEnd();
}

ExceptionOr<void> HTMLInputElement::setSelectionEndForBindings(int end)
{
    if (!canHaveSelection())
        return Exception { TypeError };

    setSelectionEnd(end);
    return { };
}

ExceptionOr<String> HTMLInputElement::selectionDirectionForBindings() const
{
    if (!canHaveSelection())
        return Exception { TypeError };

    return String { selectionDirection() };
}

ExceptionOr<void> HTMLInputElement::setSelectionDirectionForBindings(const String& direction)
{
    if (!canHaveSelection())
        return Exception { TypeError };

    setSelectionDirection(direction);
    return { };
}

ExceptionOr<void> HTMLInputElement::setSelectionRangeForBindings(int start, int end, const String& direction)
{
    if (!canHaveSelection())
        return Exception { TypeError };
    
    setSelectionRange(start, end, direction);
    return { };
}

static Ref<CSSLinearGradientValue> autoFillStrongPasswordMaskImage()
{
    CSSGradientColorStop firstStop;
    firstStop.color = CSSValuePool::singleton().createColorValue(Color::black);
    firstStop.position = CSSValuePool::singleton().createValue(50, CSSUnitType::CSS_PERCENTAGE);

    CSSGradientColorStop secondStop;
    secondStop.color = CSSValuePool::singleton().createColorValue(Color::transparent);
    secondStop.position = CSSValuePool::singleton().createValue(100, CSSUnitType::CSS_PERCENTAGE);

    auto gradient = CSSLinearGradientValue::create(CSSGradientRepeat::NonRepeating, CSSGradientType::CSSLinearGradient);
    gradient->setAngle(CSSValuePool::singleton().createValue(90, CSSUnitType::CSS_DEG));
    gradient->addStop(WTFMove(firstStop));
    gradient->addStop(WTFMove(secondStop));
    gradient->doneAddingStops();
    gradient->resolveRGBColors();
    return gradient;
}

RenderStyle HTMLInputElement::createInnerTextStyle(const RenderStyle& style)
{
    auto textBlockStyle = RenderStyle::create();
    textBlockStyle.inheritFrom(style);
    adjustInnerTextStyle(style, textBlockStyle);

    textBlockStyle.setWhiteSpace(WhiteSpace::Pre);
    textBlockStyle.setOverflowWrap(OverflowWrap::Normal);
    textBlockStyle.setOverflowX(Overflow::Hidden);
    textBlockStyle.setOverflowY(Overflow::Hidden);
    textBlockStyle.setTextOverflow(shouldTruncateText(style) ? TextOverflow::Ellipsis : TextOverflow::Clip);

    textBlockStyle.setDisplay(DisplayType::Block);

    if (hasAutoFillStrongPasswordButton() && !isDisabledOrReadOnly()) {
        textBlockStyle.setDisplay(DisplayType::InlineBlock);
        textBlockStyle.setMaxWidth(Length { 100, Percent });
        textBlockStyle.setColor(Color::black.colorWithAlphaByte(153));
        textBlockStyle.setTextOverflow(TextOverflow::Clip);
        textBlockStyle.setMaskImage(StyleGeneratedImage::create(autoFillStrongPasswordMaskImage()));
        // A stacking context is needed for the mask.
        if (textBlockStyle.hasAutoUsedZIndex())
            textBlockStyle.setUsedZIndex(0);
    }

    // Do not allow line-height to be smaller than our default.
    if (textBlockStyle.fontMetrics().lineSpacing() > style.computedLineHeight())
        textBlockStyle.setLineHeight(RenderStyle::initialLineHeight());

    return textBlockStyle;
}

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

bool HTMLInputElement::setupDateTimeChooserParameters(DateTimeChooserParameters& parameters)
{
    if (!document().view())
        return false;

    parameters.type = type();
    parameters.minimum = minimum();
    parameters.maximum = maximum();
    parameters.required = isRequired();

    if (!document().settings().langAttributeAwareFormControlUIEnabled())
        parameters.locale = defaultLanguage();
    else {
        AtomString computedLocale = computeInheritedLanguage();
        parameters.locale = computedLocale.isEmpty() ? AtomString(defaultLanguage()) : computedLocale;
    }

    StepRange stepRange = createStepRange(RejectAny);
    if (stepRange.hasStep()) {
        parameters.step = stepRange.step().toDouble();
        parameters.stepBase = stepRange.stepBase().toDouble();
    } else {
        parameters.step = 1.0;
        parameters.stepBase = 0;
    }

    if (RenderElement* renderer = this->renderer())
        parameters.anchorRectInRootView = document().view()->contentsToRootView(renderer->absoluteBoundingBoxRect());
    else
        parameters.anchorRectInRootView = IntRect();
    parameters.currentValue = value();
    parameters.isAnchorElementRTL = computedStyle()->direction() == TextDirection::RTL;

#if ENABLE(DATALIST_ELEMENT)
    if (auto dataList = this->dataList()) {
        for (auto& option : dataList->suggestions()) {
            auto label = option.label();
            auto value = option.value();
            if (!isValidValue(value))
                continue;
            parameters.suggestionValues.append(sanitizeValue(value));
            parameters.localizedSuggestionValues.append(localizeValue(value));
            parameters.suggestionLabels.append(value == label ? String() : label);
        }
    }
#endif

    return true;
}

#endif

void HTMLInputElement::capsLockStateMayHaveChanged()
{
    m_inputType->capsLockStateMayHaveChanged();
}

} // namespace
