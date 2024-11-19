/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2024 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2010-2021 Google Inc. All rights reserved.
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
#include "CheckboxInputType.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ColorInputType.h"
#include "DateComponents.h"
#include "DateTimeChooser.h"
#include "DocumentInlines.h"
#include "Editor.h"
#include "ElementInlines.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FileChooser.h"
#include "FileInputType.h"
#include "FileList.h"
#include "FormController.h"
#include "FrameSelection.h"
#include "HTMLDataListElement.h"
#include "HTMLFormElement.h"
#include "HTMLImageLoader.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
#include "IdTargetObserver.h"
#include "KeyboardEvent.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "NodeName.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PseudoClassChangeInvalidation.h"
#include "RadioInputType.h"
#include "RenderStyleSetters.h"
#include "RenderTextControlSingleLine.h"
#include "RenderTheme.h"
#include "ScopedEventQueue.h"
#include "SearchInputType.h"
#include "Settings.h"
#include "StepRange.h"
#include "StyleGradientImage.h"
#include "TextControlInnerElements.h"
#include "TextInputType.h"
#include "TreeScopeInlines.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <wtf/Language.h>
#include <wtf/MathExtras.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>

#if ENABLE(TOUCH_EVENTS)
#include "TouchEvent.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(HTMLInputElement);

using namespace HTMLNames;

#if ENABLE(DATALIST_ELEMENT)
class ListAttributeTargetObserver final : public IdTargetObserver {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(ListAttributeTargetObserver);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ListAttributeTargetObserver);
public:
    ListAttributeTargetObserver(const AtomString& id, HTMLInputElement&);

    void idTargetChanged() override;

private:
    WeakPtr<HTMLInputElement, WeakPtrImplWithEventTargetData> m_element;
};
#endif

static constexpr int maxSavedResults = 256;

HTMLInputElement::HTMLInputElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form, CreationType creationType)
    : HTMLTextFormControlElement(tagName, document, form)
    , m_parsingInProgress(creationType == CreationType::ByParser)
    // m_inputType is lazily created when constructed by the parser to avoid constructing unnecessarily a text inputType,
    // just to destroy them when the |type| attribute gets set by the parser to something else than 'text'.
    , m_inputType(creationType != CreationType::Normal ? nullptr : RefPtr { TextInputType::create(*this) })
{
    ASSERT(hasTagName(inputTag));
}

Ref<HTMLInputElement> HTMLInputElement::create(const QualifiedName& tagName, Document& document, HTMLFormElement* form, bool createdByParser)
{
    return adoptRef(*new HTMLInputElement(tagName, document, form, createdByParser ? CreationType::ByParser : CreationType::Normal));
}

Ref<Element> HTMLInputElement::cloneElementWithoutAttributesAndChildren(Document& targetDocument)
{
    return adoptRef(*new HTMLInputElement(tagQName(), targetDocument, nullptr, CreationType::ByCloning));
}

HTMLImageLoader& HTMLInputElement::ensureImageLoader()
{
    if (!m_imageLoader)
        m_imageLoader = makeUniqueWithoutRefCountedCheck<HTMLImageLoader>(*this);
    return *m_imageLoader;
}

HTMLInputElement::~HTMLInputElement()
{
    // Need to remove form association while this is still an HTMLInputElement
    // so that virtual functions are called correctly.
    setForm(nullptr);

    // This is needed for a radio button that was not in a form, and also for
    // a radio button that was in a form. The call to setForm(nullptr) above
    // actually adds the button to the document groups in the latter case.
    // That is inelegant, but harmless since we remove it here.
    if (m_inputType && isRadioButton())
        treeScope().radioButtonGroups().removeButton(*this);

#if ENABLE(TOUCH_EVENTS)
    if (m_hasTouchEventHandler) {
#if ENABLE(IOS_TOUCH_EVENTS)
        document().removeTouchEventHandler(*this);
#else
        document().didRemoveEventTargetNode(*this);
#endif
    }
#endif
}

const AtomString& HTMLInputElement::name() const
{
    return m_name.isNull() ? emptyAtom() : m_name;
}

Vector<FileChooserFileInfo> HTMLInputElement::filesFromFileInputFormControlState(const FormControlState& state)
{
    auto [files, _] = FileInputType::filesFromFormControlState(state);
    return files;
}

HTMLElement* HTMLInputElement::containerElement() const
{
    return m_inputType->containerElement();
}

RefPtr<TextControlInnerTextElement> HTMLInputElement::innerTextElement() const
{
    return m_inputType->innerTextElement();
}

RefPtr<TextControlInnerTextElement> HTMLInputElement::innerTextElementCreatingShadowSubtreeIfNeeded()
{
    return m_inputType->innerTextElementCreatingShadowSubtreeIfNeeded();
}

HTMLElement* HTMLInputElement::innerBlockElement() const
{
    return m_inputType->innerBlockElement();
}

HTMLElement* HTMLInputElement::innerSpinButtonElement() const
{
    return m_inputType->innerSpinButtonElement();
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
    if (!m_inputType->isValidValue(value))
        return false;

    return !tooShort(value, IgnoreDirtyFlag) && !tooLong(value, IgnoreDirtyFlag);
}

bool HTMLInputElement::tooShort() const
{
    return tooShort(value(), CheckDirtyFlag);
}

bool HTMLInputElement::tooLong() const
{
    return tooLong(value(), CheckDirtyFlag);
}

bool HTMLInputElement::typeMismatch() const
{
    return m_inputType->typeMismatch();
}

bool HTMLInputElement::valueMissing() const
{
    return m_inputType->valueMissing(value());
}

bool HTMLInputElement::hasBadInput() const
{
    return m_inputType->hasBadInput();
}

bool HTMLInputElement::patternMismatch() const
{
    return m_inputType->patternMismatch(value());
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

    return value.length() < static_cast<unsigned>(min);
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
    return value.length() > max;
}

bool HTMLInputElement::rangeUnderflow() const
{
    return m_inputType->rangeUnderflow(value());
}

bool HTMLInputElement::rangeOverflow() const
{
    return m_inputType->rangeOverflow(value());
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
    return m_inputType->stepMismatch(value());
}

bool HTMLInputElement::computeValidity() const
{
    String value = this->value();
    bool someError = m_inputType->isInvalid(value) || tooShort(value, CheckDirtyFlag) || tooLong(value, CheckDirtyFlag) || customError();
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
std::optional<Decimal> HTMLInputElement::findClosestTickMarkValue(const Decimal& value)
{
    return m_inputType->findClosestTickMarkValue(value);
}

std::optional<double> HTMLInputElement::listOptionValueAsDouble(const HTMLOptionElement& optionElement)
{
    auto optionValue = optionElement.value();
    if (!isValidValue(optionValue))
        return std::nullopt;

    return parseToDoubleForNumberType(sanitizeValue(optionValue));
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
        if (restorationMode == SelectionRestorationMode::RestoreOrSelectAll && hasCachedSelection())
            restoreCachedSelection(revealMode);
        else
            setDefaultSelectionAfterFocus(restorationMode, revealMode);
    } else
        HTMLTextFormControlElement::updateFocusAppearance(restorationMode, revealMode);
}

void HTMLInputElement::setDefaultSelectionAfterFocus(SelectionRestorationMode restorationMode, SelectionRevealMode revealMode)
{
    ASSERT(isTextField());
    unsigned start = 0;
    auto direction = SelectionHasNoDirection;
    auto* frame = document().frame();
    if (frame && frame->editor().behavior().shouldMoveSelectionToEndWhenFocusingTextInput()) {
        start = std::numeric_limits<unsigned>::max();
        direction = SelectionHasForwardDirection;
    }
    unsigned end = restorationMode == SelectionRestorationMode::PlaceCaretAtStart ? start : std::numeric_limits<unsigned>::max();
    setSelectionRange(start, end, direction, revealMode, Element::defaultFocusTextStateChangeIntent());
}

void HTMLInputElement::endEditing()
{
    if (!isTextField())
        return;

    if (RefPtr frame = document().frame())
        frame->editor().textFieldDidEndEditing(*this);
}

bool HTMLInputElement::shouldUseInputMethod()
{
    return m_inputType->shouldUseInputMethod();
}

void HTMLInputElement::handleFocusEvent(Node* oldFocusedNode, FocusDirection direction)
{
    m_inputType->handleFocusEvent(oldFocusedNode, direction);

    invalidateStyleOnFocusChangeIfNeeded();
}

void HTMLInputElement::handleBlurEvent()
{
    m_inputType->handleBlurEvent();

    invalidateStyleOnFocusChangeIfNeeded();
}

void HTMLInputElement::setType(const AtomString& type)
{
    setAttributeWithoutSynchronization(typeAttr, type);
}

void HTMLInputElement::resignStrongPasswordAppearance()
{
    if (!hasAutofillStrongPasswordButton())
        return;
    setAutofilled(false);
    setAutofilledAndViewable(false);
    setAutofillButtonType(AutoFillButtonType::None);
    if (auto* page = document().page())
        page->chrome().client().inputElementDidResignStrongPasswordAppearance(*this);
}

void HTMLInputElement::updateType(const AtomString& typeAttributeValue)
{
    ASSERT(m_inputType);
    auto newType = InputType::createIfDifferent(*this, typeAttributeValue, m_inputType.get());
    m_hasType = true;
    if (!newType)
        return;
    ASSERT(m_inputType->type() != newType->type());

    Style::PseudoClassChangeInvalidation defaultInvalidation(*this, CSSSelector::PseudoClass::Default, Style::PseudoClassChangeInvalidation::AnyValue);

    removeFromRadioButtonGroup();
    resignStrongPasswordAppearance();

    bool didSupportReadOnly = m_inputType->supportsReadOnly();
    bool willSupportReadOnly = newType->supportsReadOnly();
    std::optional<Style::PseudoClassChangeInvalidation> readWriteInvalidation;

    bool didStoreValue = m_inputType->storesValueSeparateFromAttribute();
    bool willStoreValue = newType->storesValueSeparateFromAttribute();
    bool neededSuspensionCallback = needsSuspensionCallback();
    bool didRespectHeightAndWidth = m_inputType->shouldRespectHeightAndWidthAttributes();
    bool wasSuccessfulSubmitButtonCandidate = m_inputType->canBeSuccessfulSubmitButton();

    if (didStoreValue && !willStoreValue) {
        if (auto dirtyValue = std::exchange(m_valueIfDirty, { }); !dirtyValue.isEmpty())
            setAttributeWithoutSynchronization(valueAttr, AtomString { WTFMove(dirtyValue) });
    }

    m_inputType->removeShadowSubtree();
    m_inputType->detachFromElement();
    auto oldType = m_inputType->type();

    bool didDirAutoUseValue = m_inputType->dirAutoUsesValue();
    bool previouslySelectable = m_inputType->supportsSelectionAPI();

    m_inputType = WTFMove(newType);
    if (!didStoreValue && willStoreValue)
        m_valueIfDirty = sanitizeValue(attributeWithoutSynchronization(valueAttr));
    else
        updateValueIfNeeded();
    m_inputType->createShadowSubtreeIfNeeded();

    // https://html.spec.whatwg.org/multipage/dom.html#auto-directionality
    if (oldType == InputType::Type::Telephone || m_inputType->type() == InputType::Type::Telephone || (hasAutoTextDirectionState() && didDirAutoUseValue != m_inputType->dirAutoUsesValue()))
        updateEffectiveTextDirection();

    if (UNLIKELY(didSupportReadOnly != willSupportReadOnly && hasAttributeWithoutSynchronization(readonlyAttr))) {
        emplace(readWriteInvalidation, *this, { { CSSSelector::PseudoClass::ReadWrite, !willSupportReadOnly }, { CSSSelector::PseudoClass::ReadOnly, willSupportReadOnly } });
        readOnlyStateChanged();
    }

    updateWillValidateAndValidity();

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

    if (auto* form = this->form(); form && wasSuccessfulSubmitButtonCandidate != m_inputType->canBeSuccessfulSubmitButton())
        form->resetDefaultButton();

    runPostTypeUpdateTasks();

    // https://html.spec.whatwg.org/multipage/input.html#input-type-change
    // 8. Let nowSelectable be true if setRangeText() now applies to the element, and false otherwise.
    bool nowSelectable = m_inputType->supportsSelectionAPI();
    // 9. If previouslySelectable is false and nowSelectable is true, set the element's text entry cursor position to the beginning of the text control, and set its selection direction to "none".
    if (!previouslySelectable && nowSelectable) {
        TextFieldSelectionDirection direction = SelectionHasNoDirection;
        // https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#set-the-selection-direction
        RefPtr frame = document().frame();
        if (isTextField() && frame && frame->editor().behavior().shouldConsiderSelectionAsDirectional())
            direction = SelectionHasForwardDirection;
        cacheSelection(0, 0, direction);
    }

    updateValidity();
}

inline void HTMLInputElement::runPostTypeUpdateTasks()
{
    ASSERT(m_inputType);
#if ENABLE(TOUCH_EVENTS)
    updateTouchEventHandler();
#endif

    if (isPasswordField())
        m_hasEverBeenPasswordField = true;

    if (renderer())
        invalidateStyleAndRenderersForSubtree();

    if (document().focusedElement() == this)
        updateFocusAppearance(SelectionRestorationMode::RestoreOrSelectAll, SelectionRevealMode::Reveal);

    setChangedSinceLastFormControlChangeEvent(false);

    addToRadioButtonGroup();
}

#if ENABLE(TOUCH_EVENTS)
inline void HTMLInputElement::updateTouchEventHandler()
{
    bool hasTouchEventHandler = m_inputType->hasTouchEventHandler();
    if (hasTouchEventHandler != m_hasTouchEventHandler) {
        if (hasTouchEventHandler) {
#if ENABLE(IOS_TOUCH_EVENTS)
            document().addTouchEventHandler(*this);
#else
            document().didAddTouchEventHandler(*this);
#endif
        } else {
#if ENABLE(IOS_TOUCH_EVENTS)
            document().removeTouchEventHandler(*this);
#else
            document().didRemoveTouchEventHandler(*this);
#endif
        }
        m_hasTouchEventHandler = hasTouchEventHandler;
    }
}
#endif

void HTMLInputElement::subtreeHasChanged()
{
    m_inputType->subtreeHasChanged();
    // When typing in an input field, childrenChanged is not called, so we need to force the directionality check.
    if (selfOrPrecedingNodesAffectDirAuto())
        updateEffectiveTextDirection();
}

const AtomString& HTMLInputElement::formControlType() const
{
    return m_inputType->formControlType();
}

bool HTMLInputElement::shouldSaveAndRestoreFormControlState() const
{
    return m_inputType->shouldSaveAndRestoreFormControlState();
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
    return Ref { *m_inputType }->accessKeyAction(sendMouseEvents);
}

bool HTMLInputElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    switch (name.nodeName()) {
    case AttributeNames::vspaceAttr:
    case AttributeNames::hspaceAttr:
    case AttributeNames::widthAttr:
    case AttributeNames::heightAttr:
        return true;
    case AttributeNames::borderAttr:
        return isImageButton();
    default:
        return HTMLTextFormControlElement::hasPresentationalHintsForAttribute(name);
        break;
    }
}

void HTMLInputElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    switch (name.nodeName()) {
    case AttributeNames::vspaceAttr:
        if (isImageButton()) {
            addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
            addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
        }
        break;
    case AttributeNames::hspaceAttr:
        if (isImageButton()) {
            addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
            addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
        }
        break;
    case AttributeNames::alignAttr:
        if (m_inputType->shouldRespectAlignAttribute())
            applyAlignmentAttributeToStyle(value, style);
        break;
    case AttributeNames::widthAttr:
        if (m_inputType->shouldRespectHeightAndWidthAttributes())
            addHTMLLengthToStyle(style, CSSPropertyWidth, value);
        if (isImageButton())
            applyAspectRatioFromWidthAndHeightAttributesToStyle(value, attributeWithoutSynchronization(heightAttr), style);
        break;
    case AttributeNames::heightAttr:
        if (m_inputType->shouldRespectHeightAndWidthAttributes())
            addHTMLLengthToStyle(style, CSSPropertyHeight, value);
        if (isImageButton())
            applyAspectRatioFromWidthAndHeightAttributesToStyle(attributeWithoutSynchronization(widthAttr), value, style);
        break;
    case AttributeNames::borderAttr:
        if (isImageButton())
            applyBorderAttributeToStyle(value, style);
        break;
    default:
        HTMLTextFormControlElement::collectPresentationalHintsForAttribute(name, value, style);
        break;
    }
}

void HTMLInputElement::initializeInputTypeAfterParsingOrCloning()
{
    ASSERT(!m_inputType);

    auto& type = attributeWithoutSynchronization(typeAttr);
    if (type.isNull()) {
        m_inputType = TextInputType::create(*this);
        updateWillValidateAndValidity();
        return;
    }

    m_hasType = true;
    m_inputType = InputType::createIfDifferent(*this, type);
    updateWillValidateAndValidity();
    registerForSuspensionCallbackIfNeeded();
    runPostTypeUpdateTasks();
    updateValidity();
}

void HTMLInputElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (oldValue == newValue)
        return;

    ASSERT(m_inputType);
    Ref protectedInputType { *m_inputType };

    HTMLTextFormControlElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);

    switch (name.nodeName()) {
    case AttributeNames::typeAttr:
        if (attributeModificationReason != AttributeModificationReason::Directly)
            return; // initializeInputTypeAfterParsingOrCloning has taken care of this.
        updateType(newValue);
        break;
    case AttributeNames::valueAttr:
        if (attributeModificationReason != AttributeModificationReason::Directly)
            return; // initializeInputTypeAfterParsingOrCloning has taken care of this.
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
            setFormControlValueMatchesRenderer(false);
        }
        updateValidity();
        if (selfOrPrecedingNodesAffectDirAuto())
            updateEffectiveTextDirection();
        m_valueAttributeWasUpdatedAfterParsing = !m_parsingInProgress;
        break;
    case AttributeNames::nameAttr:
        removeFromRadioButtonGroup();
        m_name = newValue;
        addToRadioButtonGroup();
        HTMLTextFormControlElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
        break;
    case AttributeNames::checkedAttr:
        setDefaultCheckedState(!newValue.isNull());
        // Another radio button in the same group might be checked by state
        // restore. We shouldn't call setChecked() even if this has the checked
        // attribute. So, delay the setChecked() call until
        // finishParsingChildren() is called if parsing is in progress.
        if ((!m_parsingInProgress || !document().formController().hasFormStateToRestore()) && !m_dirtyCheckednessFlag) {
            setChecked(!newValue.isNull());
            // setChecked() above sets the dirty checkedness flag so we need to reset it.
            m_dirtyCheckednessFlag = false;
        }
        break;
    case AttributeNames::autocompleteAttr:
        if (equalLettersIgnoringASCIICase(newValue, "off"_s)) {
            m_autocomplete = Off;
            registerForSuspensionCallbackIfNeeded();
        } else {
            bool needsToUnregister = m_autocomplete == Off;

            if (newValue.isEmpty())
                m_autocomplete = Uninitialized;
            else
                m_autocomplete = On;

            if (needsToUnregister)
                unregisterForSuspensionCallbackIfNeeded();
        }
        break;
    case AttributeNames::maxlengthAttr:
        maxLengthAttributeChanged(newValue);
        break;
    case AttributeNames::minlengthAttr:
        minLengthAttributeChanged(newValue);
        break;
    case AttributeNames::sizeAttr: {
        unsigned oldSize = m_size;
        m_size = limitToOnlyHTMLNonNegativeNumbersGreaterThanZero(newValue, defaultSize);
        if (m_size != oldSize && renderer())
            renderer()->setNeedsLayoutAndPrefWidthsRecalc();
        break;
    }
    case AttributeNames::resultsAttr:
        m_maxResults = newValue.isNull() ? -1 : std::min(parseHTMLInteger(newValue).value_or(0), maxSavedResults);
        break;
    case AttributeNames::autosaveAttr:
        invalidateStyleForSubtree();
        break;
    case AttributeNames::maxAttr:
    case AttributeNames::minAttr:
    case AttributeNames::multipleAttr:
    case AttributeNames::patternAttr:
    case AttributeNames::stepAttr:
        updateValidity();
        break;
#if ENABLE(DATALIST_ELEMENT)
    case AttributeNames::listAttr:
        m_hasNonEmptyList = !newValue.isEmpty();
        if (m_hasNonEmptyList) {
            resetListAttributeTargetObserver();
            dataListMayHaveChanged();
        }
        break;
#endif
    case AttributeNames::switchAttr:
        if (document().settings().switchControlEnabled()) {
            auto hasSwitchAttribute = !newValue.isNull();
            m_hasSwitchAttribute = hasSwitchAttribute;
            if (isSwitch())
                m_inputType->createShadowSubtreeIfNeeded();
            else if (isCheckbox())
                m_inputType->removeShadowSubtree();
            if (renderer())
                invalidateStyleAndRenderersForSubtree();
#if ENABLE(TOUCH_EVENTS)
            updateTouchEventHandler();
#endif
        }
        break;
#if ENABLE(INPUT_TYPE_COLOR)
    case AttributeNames::alphaAttr:
    case AttributeNames::colorspaceAttr:
        if (isColorControl() && document().settings().inputTypeColorEnhancementsEnabled()) {
            updateValueIfNeeded();
            updateValidity();
        }
        break;
#endif
    default:
        break;
    }

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

bool HTMLInputElement::supportsReadOnly() const
{
    return m_inputType->supportsReadOnly();
}

void HTMLInputElement::finishParsingChildren()
{
    m_parsingInProgress = false;
    ASSERT(m_inputType);
    HTMLTextFormControlElement::finishParsingChildren();
    if (!m_stateRestored) {
        if (hasAttributeWithoutSynchronization(checkedAttr))
            setChecked(true);
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
        updateType(attributeWithoutSynchronization(typeAttr));
}

void HTMLInputElement::didAttachRenderers()
{
    HTMLTextFormControlElement::didAttachRenderers();

    m_inputType->attach();

    if (document().focusedElement() == this) {
        document().eventLoop().queueTask(TaskSource::UserInteraction, [weakThis = WeakPtr { *this }]() {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || protectedThis->document().focusedElement() != protectedThis)
                return;
            protectedThis->updateFocusAppearance(SelectionRestorationMode::RestoreOrSelectAll, SelectionRevealMode::Reveal);
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
    if (alt.isNull())
        alt = inputElementAltText();
    return alt;
}

bool HTMLInputElement::isSuccessfulSubmitButton() const
{
    // HTML spec says that buttons must have names to be considered successful.
    // However, other browsers do not impose this constraint. So we do not.
    return m_inputType->canBeSuccessfulSubmitButton();
}

bool HTMLInputElement::matchesDefaultPseudoClass() const
{
    ASSERT(m_inputType);
    if (m_inputType->canBeSuccessfulSubmitButton())
        return !isDisabledFormControl() && form() && form()->defaultButton() == this;
    return m_inputType->isCheckable() && m_isDefaultChecked;
}

bool HTMLInputElement::isActivatedSubmit() const
{
    return m_isActivatedSubmit;
}

void HTMLInputElement::setActivatedSubmit(bool flag)
{
    m_isActivatedSubmit = flag;
}

bool HTMLInputElement::appendFormData(DOMFormData& formData)
{
    Ref protectedInputType { *m_inputType };
    return m_inputType->isFormDataAppendable() && m_inputType->appendFormData(formData);
}

void HTMLInputElement::reset()
{
    if (m_inputType->storesValueSeparateFromAttribute()) {
        setValue({ });
        updateValidity();
    }

    setInteractedWithSinceLastFormSubmitEvent(false);
    setAutofilled(false);
    setAutofilledAndViewable(false);
    setAutofilledAndObscured(false);
    setAutofillButtonType(AutoFillButtonType::None);
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

bool HTMLInputElement::supportsWritingSuggestions() const
{
    static constexpr OptionSet<InputType::Type> allowedTypes = {
        InputType::Type::Text,
        InputType::Type::Search,
        InputType::Type::URL,
        InputType::Type::Email,
    };

    return allowedTypes.contains(m_inputType->type());
}

void HTMLInputElement::setDefaultCheckedState(bool isDefaultChecked)
{
    if (m_isDefaultChecked == isDefaultChecked)
        return;

    std::optional<Style::PseudoClassChangeInvalidation> defaultInvalidation;
    if (m_inputType->isCheckable())
        defaultInvalidation.emplace(*this, CSSSelector::PseudoClass::Default, isDefaultChecked);
    m_isDefaultChecked = isDefaultChecked;
}

void HTMLInputElement::setChecked(bool isChecked, WasSetByJavaScript wasCheckedByJavaScript)
{
    m_dirtyCheckednessFlag = true;
    if (checked() == isChecked)
        return;

    m_inputType->willUpdateCheckedness(isChecked, wasCheckedByJavaScript);

    Style::PseudoClassChangeInvalidation checkedInvalidation(*this, CSSSelector::PseudoClass::Checked, isChecked);

    m_isChecked = isChecked;

    if (auto* buttons = radioButtonGroups())
        buttons->updateCheckedState(*this);
    if (auto* renderer = this->renderer(); renderer && renderer->style().hasUsedAppearance())
        renderer->repaint();
    updateValidity();

    // Ideally we'd do this from the render tree (matching
    // RenderTextView), but it's not possible to do it at the moment
    // because of the way the code is structured.
    if (auto* renderer = this->renderer()) {
        if (CheckedPtr cache = renderer->document().existingAXObjectCache())
            cache->checkedStateChanged(*this);
    }
}

void HTMLInputElement::setIndeterminate(bool newValue)
{
    if (indeterminate() == newValue)
        return;

    Style::PseudoClassChangeInvalidation indeterminateInvalidation(*this, CSSSelector::PseudoClass::Indeterminate, newValue);
    m_isIndeterminate = newValue;

    if (auto* renderer = this->renderer(); renderer && renderer->style().hasUsedAppearance())
        renderer->repaint();

    if (CheckedPtr cache = document().existingAXObjectCache())
        cache->valueChanged(*this);
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
    m_isDefaultChecked = sourceElement.m_isDefaultChecked;
    m_dirtyCheckednessFlag = sourceElement.m_dirtyCheckednessFlag;
    m_isIndeterminate = sourceElement.m_isIndeterminate;

    HTMLTextFormControlElement::copyNonAttributePropertiesFromElement(source);

    updateValidity();
    setFormControlValueMatchesRenderer(false);
    if (m_inputType->hasCreatedShadowSubtree())
        m_inputType->updateInnerTextValue();
}

String HTMLInputElement::value() const
{
    if (document().requiresScriptExecutionTelemetry(ScriptTelemetryCategory::FormControls))
        return m_inputType->defaultValue();
    if (auto* fileInput = dynamicDowncast<FileInputType>(*m_inputType))
        return fileInput->firstElementPathForInputValue();

    if (!m_valueIfDirty.isNull())
        return m_valueIfDirty;

    if (auto& valueString = attributeWithoutSynchronization(valueAttr); !valueString.isNull()) {
        if (auto sanitizedValue = sanitizeValue(valueString); !sanitizedValue.isNull())
            return sanitizedValue;
    }

    return m_inputType->fallbackValue();
}

String HTMLInputElement::valueWithDefault() const
{
    if (auto value = this->value(); !value.isNull())
        return value;

    return m_inputType->defaultValue();
}

ExceptionOr<void> HTMLInputElement::setValue(const String& value, TextFieldEventBehavior eventBehavior, TextControlSetValueSelection selection)
{
    if (isFileUpload() && !value.isEmpty())
        return Exception { ExceptionCode::InvalidStateError };

    Ref protectedThis { *this };
    EventQueueScope scope;
    auto sanitizedValue = sanitizeValue(value);
    bool valueChanged = sanitizedValue != this->value();

    setLastChangeWasNotUserEdit();
    setFormControlValueMatchesRenderer(false);
    m_inputType->setValue(WTFMove(sanitizedValue), valueChanged, eventBehavior, selection);
    if (selfOrPrecedingNodesAffectDirAuto())
        updateEffectiveTextDirection();

    if (valueChanged && eventBehavior == DispatchNoEvent)
        setTextAsOfLastFormControlChangeEvent(sanitizedValue);

    bool wasModifiedProgrammatically = eventBehavior == DispatchNoEvent;
    if (wasModifiedProgrammatically) {
        resignStrongPasswordAppearance();

        if (m_isAutoFilledAndObscured)
            setAutofilledAndObscured(false);
    }

    return { };
}

void HTMLInputElement::setValueInternal(const String& sanitizedValue, TextFieldEventBehavior eventBehavior)
{
    m_valueIfDirty = sanitizedValue;
    m_wasModifiedByUser = eventBehavior != DispatchNoEvent;
    updateValidity();
}

WallTime HTMLInputElement::valueAsDate() const
{
    return m_inputType->valueAsDate();
}

ExceptionOr<void> HTMLInputElement::setValueAsDate(WallTime value)
{
    return m_inputType->setValueAsDate(value);
}

WallTime HTMLInputElement::accessibilityValueAsDate() const
{
    return m_inputType->accessibilityValueAsDate();
}

double HTMLInputElement::valueAsNumber() const
{
    return m_inputType->valueAsDouble();
}

ExceptionOr<void> HTMLInputElement::setValueAsNumber(double newValue, TextFieldEventBehavior eventBehavior)
{
    if (!std::isfinite(newValue))
        return Exception { ExceptionCode::NotSupportedError };
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
    m_valueIfDirty = value == "\n"_s ? emptyString() : value;

    setFormControlValueMatchesRenderer(true);
    m_wasModifiedByUser = true;

    // Input event is fired by the Node::defaultEventHandler for editable controls.
    if (!isTextField())
        dispatchInputEvent();

    updateValidity();

    // We clear certain AutoFill flags here because this catches user edits.
    setAutofilled(false);

    if (!value.isEmpty())
        return;

    if (m_isAutoFilledAndViewable)
        setAutofilledAndViewable(false);

    if (m_isAutoFilledAndObscured)
        setAutofilledAndObscured(false);
}

void HTMLInputElement::willDispatchEvent(Event& event, InputElementClickState& state)
{
    auto& eventNames = WebCore::eventNames();
    if (event.type() == eventNames.textInputEvent && m_inputType->shouldSubmitImplicitly(event))
        event.stopPropagation();
    if (event.type() == eventNames.clickEvent) {
        auto* mouseEvent = dynamicDowncast<MouseEvent>(event);
        if (mouseEvent && mouseEvent->button() == MouseButton::Left) {
            m_inputType->willDispatchClick(state);
            state.stateful = true;
        }
    }

    if (event.type() == eventNames.auxclickEvent) {
        auto* mouseEvent = dynamicDowncast<MouseEvent>(event);
        if (mouseEvent && mouseEvent->button() != MouseButton::Left) {
            m_inputType->willDispatchClick(state);
            state.stateful = true;
        }
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
    if (auto* mouseEvent = dynamicDowncast<MouseEvent>(event); mouseEvent && mouseEvent->button() == MouseButton::Left) {
        auto eventType = mouseEvent->type();
        if (isAnyClick(*mouseEvent))
            m_inputType->handleClickEvent(*mouseEvent);
        else if (eventType == eventNames().mousedownEvent)
            m_inputType->handleMouseDownEvent(*mouseEvent);
        else if (eventType == eventNames().mousemoveEvent)
            m_inputType->handleMouseMoveEvent(*mouseEvent);

        if (mouseEvent->defaultHandled())
            return;
    }

#if ENABLE(TOUCH_EVENTS)
    if (auto* touchEvent = dynamicDowncast<TouchEvent>(event)) {
        m_inputType->handleTouchEvent(*touchEvent);
        if (event.defaultHandled())
            return;
    }
#endif

    if (auto* keyboardEvent = dynamicDowncast<KeyboardEvent>(event); keyboardEvent && keyboardEvent->type() == eventNames().keydownEvent) {
        auto shouldCallBaseEventHandler = m_inputType->handleKeydownEvent(*keyboardEvent);
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
        if (commandForElement())
            handleCommand();
        else
            handlePopoverTargetAction();
        if (event.defaultHandled())
            return;
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (auto* keyboardEvent = dynamicDowncast<KeyboardEvent>(event)) {
        if (keyboardEvent->type() == eventNames().keypressEvent) {
            m_inputType->handleKeypressEvent(*keyboardEvent);
            if (keyboardEvent->defaultHandled())
                return;
        } else if (keyboardEvent->type() == eventNames().keyupEvent) {
            m_inputType->handleKeyupEvent(*keyboardEvent);
            if (keyboardEvent->defaultHandled())
                return;
        }
    }

    if (m_inputType->shouldSubmitImplicitly(event)) {
        if (isSearchField())
            addSearchResult();
        // Form submission finishes editing, just as loss of focus does.
        // If there was a change, send the event now.
        if (wasChangedSinceLastFormControlChangeEvent())
            dispatchFormControlChangeEvent();

        // Form may never have been present, or may have been destroyed by code responding to the change event.
        if (RefPtr formElement = form())
            formElement->submitImplicitly(event, canTriggerImplicitSubmission());

        event.setDefaultHandled();
        return;
    }

    if (auto* beforeTextInsertedEvent = dynamicDowncast<BeforeTextInsertedEvent>(event); beforeTextInsertedEvent)
        m_inputType->handleBeforeTextInsertedEvent(*beforeTextInsertedEvent);

    m_inputType->forwardEvent(event);

    if (!callBaseClassEarly && !event.defaultHandled())
        HTMLTextFormControlElement::defaultEventHandler(event);
}

bool HTMLInputElement::willRespondToMouseClickEventsWithEditability(Editability editability) const
{
    if (!isDisabledFormControl())
        return true;

    return HTMLTextFormControlElement::willRespondToMouseClickEventsWithEditability(editability);
}

bool HTMLInputElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || attribute.name() == formactionAttr || HTMLTextFormControlElement::isURLAttribute(attribute);
}

ExceptionOr<void> HTMLInputElement::showPicker()
{
    auto* frame = document().frame();
    if (!frame)
        return { };

    if (!isMutable())
        return Exception { ExceptionCode::InvalidStateError, "Input showPicker() cannot be used on immutable controls."_s };

    // In cross-origin iframes it should throw a "SecurityError" DOMException except on file and color. In same-origin iframes it should work fine.
    // https://github.com/whatwg/html/issues/6909#issuecomment-917138991
    if (!m_inputType->allowsShowPickerAcrossFrames()) {
        auto* localTopFrame = dynamicDowncast<LocalFrame>(frame->tree().top());
        if (!localTopFrame || !frame->document()->protectedSecurityOrigin()->isSameOriginAs(localTopFrame->document()->protectedSecurityOrigin()))
            return Exception { ExceptionCode::SecurityError, "Input showPicker() called from cross-origin iframe."_s };
    }

    auto* window = frame->window();
    if (!window || !window->hasTransientActivation())
        return Exception { ExceptionCode::NotAllowedError, "Input showPicker() requires a user gesture."_s };

    m_inputType->showPicker();
    return { };
}

const AtomString& HTMLInputElement::defaultValue() const
{
    return attributeWithoutSynchronization(valueAttr);
}

void HTMLInputElement::setDefaultValue(const AtomString& value)
{
    setAttributeWithoutSynchronization(valueAttr, value);
}

static inline bool isRFC2616TokenCharacter(UChar ch)
{
    return isASCII(ch) && ch > ' ' && ch != '"' && ch != '(' && ch != ')' && ch != ',' && ch != '/' && (ch < ':' || ch > '@') && (ch < '[' || ch > ']') && ch != '{' && ch != '}' && ch != 0x7f;
}

static bool isValidMIMEType(StringView type)
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

static bool isValidFileExtension(StringView type)
{
    if (type.length() < 2)
        return false;
    return type[0] == '.';
}

static Vector<String> parseAcceptAttribute(StringView acceptString, bool (*predicate)(StringView))
{
    Vector<String> types;
    if (acceptString.isEmpty())
        return types;

    for (auto splitType : acceptString.split(',')) {
        auto trimmedType = splitType.trim(isASCIIWhitespace<UChar>);
        if (trimmedType.isEmpty())
            continue;
        if (!predicate(trimmedType))
            continue;
        types.append(trimmedType.convertToASCIILowercase());
    }

    return types;
}

Vector<String> HTMLInputElement::acceptMIMETypes() const
{
    return parseAcceptAttribute(attributeWithoutSynchronization(acceptAttr), isValidMIMEType);
}

Vector<String> HTMLInputElement::acceptFileExtensions() const
{
    return parseAcceptAttribute(attributeWithoutSynchronization(acceptAttr), isValidFileExtension);
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
        return Exception { ExceptionCode::IndexSizeError };
    setUnsignedIntegralAttribute(sizeAttr, limitToOnlyHTMLNonNegativeNumbersGreaterThanZero(size, defaultSize));
    return { };
}

URL HTMLInputElement::src() const
{
    return document().completeURL(attributeWithoutSynchronization(srcAttr));
}

void HTMLInputElement::setAutofilled(bool autoFilled)
{
    if (autoFilled == m_isAutoFilled)
        return;

    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::Autofill, autoFilled);
    m_isAutoFilled = autoFilled;
}

void HTMLInputElement::setAutofilledAndViewable(bool autoFilledAndViewable)
{
    if (autoFilledAndViewable == m_isAutoFilledAndViewable)
        return;

    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::WebKitAutofillStrongPasswordViewable, autoFilledAndViewable);
    m_isAutoFilledAndViewable = autoFilledAndViewable;
}

void HTMLInputElement::setAutofilledAndObscured(bool autoFilledAndObscured)
{
    if (autoFilledAndObscured == m_isAutoFilledAndObscured)
        return;

    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::WebKitAutofillAndObscured, autoFilledAndObscured);
    m_isAutoFilledAndObscured = autoFilledAndObscured;

    if (CheckedPtr cache = document().existingAXObjectCache())
        cache->onTextSecurityChanged(*this);
}

void HTMLInputElement::setAutofillButtonType(AutoFillButtonType autoFillButtonType)
{
    if (autoFillButtonType == this->autofillButtonType())
        return;

    m_lastAutoFillButtonType = m_autoFillButtonType;
    m_autoFillButtonType = enumToUnderlyingType(autoFillButtonType);
    m_inputType->updateAutoFillButton();
    updateInnerTextElementEditability();
    invalidateStyleForSubtree();

    if (CheckedPtr cache = document().existingAXObjectCache())
        cache->autofillTypeChanged(*this);
}

auto HTMLInputElement::autofillVisibility() const -> AutofillVisibility
{
    ASSERT(!autofilledAndObscured() || !autofilledAndViewable());
    if (autofilledAndObscured())
        return AutofillVisibility::Hidden;
    if (autofilledAndViewable())
        return AutofillVisibility::Visible;
    return AutofillVisibility::Normal;
}

void HTMLInputElement::setAutofillVisibility(AutofillVisibility state)
{
    switch (state) {
    case AutofillVisibility::Normal:
        setAutofilledAndViewable(false);
        setAutofilledAndObscured(false);
        break;
    case AutofillVisibility::Visible:
        setAutofilledAndViewable(true);
        setAutofilledAndObscured(false);
        break;
    case AutofillVisibility::Hidden:
        setAutofilledAndViewable(false);
        setAutofilledAndObscured(true);
        break;
    }
}

#if ENABLE(INPUT_TYPE_COLOR)
bool HTMLInputElement::alpha()
{
    return document().settings().inputTypeColorEnhancementsEnabled() && hasAttributeWithoutSynchronization(alphaAttr);
}

String HTMLInputElement::colorSpace()
{
    if (!document().settings().inputTypeColorEnhancementsEnabled())
        return nullString();

    if (equalLettersIgnoringASCIICase(attributeWithoutSynchronization(colorspaceAttr), "display-p3"_s))
        return "display-p3"_s;

    return "limited-srgb"_s;
}

void HTMLInputElement::setColorSpace(const AtomString& value)
{
    ASSERT(document().settings().inputTypeColorEnhancementsEnabled());
    setAttributeWithoutSynchronization(colorspaceAttr, value);
}
#endif // ENABLE(INPUT_TYPE_COLOR)

FileList* HTMLInputElement::files()
{
    if (auto* fileInputType = dynamicDowncast<FileInputType>(*m_inputType))
        return &fileInputType->files();
    return nullptr;
}

void HTMLInputElement::setFiles(RefPtr<FileList>&& files, WasSetByJavaScript wasSetByJavaScript)
{
    if (auto* fileInputType = dynamicDowncast<FileInputType>(*m_inputType))
        fileInputType->setFiles(WTFMove(files), wasSetByJavaScript);
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
    if (!m_inputType)
        return false;

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
    return supportsReadOnly() && isMutable();
}

void HTMLInputElement::addSearchResult()
{
    m_inputType->addSearchResult();
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
    document().postTask([inputElement = Ref { *this }] (ScriptExecutionContext&) {
        inputElement->reset();
    });
}

#if ENABLE(INPUT_TYPE_COLOR)
void HTMLInputElement::prepareForDocumentSuspension()
{
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
    auto result = HTMLTextFormControlElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
#if ENABLE(DATALIST_ELEMENT)
    resetListAttributeTargetObserver();
#endif
    if (isRadioButton())
        updateValidity();
    if (insertionType.connectedToDocument && m_inputType->needsShadowSubtree() && !m_inputType->hasCreatedShadowSubtree() && !m_hasPendingUserAgentShadowTreeUpdate) {
        document().addElementWithPendingUserAgentShadowTreeUpdate(*this);
        m_hasPendingUserAgentShadowTreeUpdate = true;
    }
    if (!insertionType.connectedToDocument) {
        addToRadioButtonGroup();
        return result;
    }
    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void HTMLInputElement::updateUserAgentShadowTree()
{
    ASSERT(m_hasPendingUserAgentShadowTreeUpdate);
    document().removeElementWithPendingUserAgentShadowTreeUpdate(*this);
    m_hasPendingUserAgentShadowTreeUpdate = false;
    m_inputType->createShadowSubtreeIfNeeded();
}

void HTMLInputElement::didFinishInsertingNode()
{
    HTMLTextFormControlElement::didFinishInsertingNode();
    if (isInTreeScope() && !form())
        addToRadioButtonGroup();
}

void HTMLInputElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLTextFormControlElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (removalType.treeScopeChanged && isRadioButton())
        oldParentOfRemovedTree.treeScope().radioButtonGroups().removeButton(*this);
    if (removalType.disconnectedFromDocument && !form())
        removeFromRadioButtonGroup();
    if (removalType.disconnectedFromDocument && m_hasPendingUserAgentShadowTreeUpdate) {
        document().removeElementWithPendingUserAgentShadowTreeUpdate(*this);
        m_hasPendingUserAgentShadowTreeUpdate = false;
    }
    ASSERT(!isConnected());
    if (removalType.disconnectedFromDocument && !form() && isRadioButton())
        updateValidity();
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
#if ENABLE(IOS_TOUCH_EVENTS)
        oldDocument.removeTouchEventHandler(*this);
        newDocument.addTouchEventHandler(*this);
#else
        oldDocument.didRemoveEventTargetNode(*this);
        newDocument.didAddTouchEventHandler(*this);
#endif
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
#if ENABLE(INPUT_TYPE_COLOR)
    if (auto* colorInputType = dynamicDowncast<ColorInputType>(*m_inputType))
        return colorInputType->valueAsColor();
#endif
    return Color::black;
}

void HTMLInputElement::selectColor(StringView color)
{
#if ENABLE(INPUT_TYPE_COLOR)
    if (auto* colorInputType = dynamicDowncast<ColorInputType>(*m_inputType))
        colorInputType->selectColor(color);
#else
    UNUSED_PARAM(color);
#endif
}

Vector<Color> HTMLInputElement::suggestedColors() const
{
#if ENABLE(INPUT_TYPE_COLOR)
    if (auto* colorInputType = dynamicDowncast<ColorInputType>(*m_inputType))
        return colorInputType->suggestedColors();
#endif
    return { };
}

#if ENABLE(DATALIST_ELEMENT)

RefPtr<HTMLElement> HTMLInputElement::list() const
{
    return dataList();
}

RefPtr<HTMLDataListElement> HTMLInputElement::dataList() const
{
    if (!m_hasNonEmptyList || !m_inputType->shouldRespectListAttribute())
        return nullptr;

    return dynamicDowncast<HTMLDataListElement>(treeScope().getElementById(attributeWithoutSynchronization(listAttr)));
}

void HTMLInputElement::resetListAttributeTargetObserver()
{
    if (isConnected()) {
        if (auto& listAttrValue = attributeWithoutSynchronization(listAttr); !listAttrValue.isNull()) {
            m_listAttributeTargetObserver = makeUnique<ListAttributeTargetObserver>(listAttrValue, *this);
            return;
        }
    }

    m_listAttributeTargetObserver = nullptr;
}

void HTMLInputElement::dataListMayHaveChanged()
{
    auto protectedInputType = m_inputType;
    protectedInputType->dataListMayHaveChanged();
}

bool HTMLInputElement::isFocusingWithDataListDropdown() const
{
    return m_inputType->isFocusingWithDataListDropdown();
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

DateComponentsType HTMLInputElement::dateType() const
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

bool HTMLInputElement::isSwitch() const
{
    return m_inputType->isSwitch();
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

bool HTMLInputElement::isLabelable() const
{
    return m_inputType->isLabelable();
}

bool HTMLInputElement::matchesCheckedPseudoClass() const
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

    updateValidity();
}

void HTMLInputElement::minLengthAttributeChanged(const AtomString& newValue)
{
    int oldMinLength = minLength();
    internalSetMinLength(parseHTMLNonNegativeInteger(newValue).value_or(-1));
    if (oldMinLength != minLength())
        updateValueIfNeeded();

    updateValidity();
}

void HTMLInputElement::updateValueIfNeeded()
{
    auto newValue = sanitizeValue(m_valueIfDirty);
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
    return m_inputType->matchesIndeterminatePseudoClass();
}

#if ENABLE(MEDIA_CAPTURE)
MediaCaptureType HTMLInputElement::mediaCaptureType() const
{
    if (!isFileUpload())
        return MediaCaptureType::MediaCaptureTypeNone;
    
    auto& captureAttribute = attributeWithoutSynchronization(captureAttr);
    if (captureAttribute.isNull())
        return MediaCaptureType::MediaCaptureTypeNone;
    
    if (equalLettersIgnoringASCIICase(captureAttribute, "user"_s))
        return MediaCaptureType::MediaCaptureTypeUser;
    
    return MediaCaptureType::MediaCaptureTypeEnvironment;
}
#endif

Vector<Ref<HTMLInputElement>> HTMLInputElement::radioButtonGroup() const
{
    if (auto* buttons = radioButtonGroups())
        return buttons->groupMembers(*this);
    return { };
}

RefPtr<HTMLInputElement> HTMLInputElement::checkedRadioButtonForGroup() const
{
    ASSERT(isRadioButton());

    if (checked())
        return const_cast<HTMLInputElement*>(this);

    auto& name = this->name();
    if (auto* buttons = radioButtonGroups())
        return buttons->checkedButtonForGroup(name);

    if (name.isEmpty())
        return nullptr;

    ASSERT(!isConnected());
    ASSERT(!form());

    // The input is not managed by a RadioButtonGroups, we'll need to traverse the tree.
    RefPtr<HTMLInputElement> checkedRadio;
    RadioInputType::forEachButtonInDetachedGroup(rootNode(), name, [&](auto& input) {
        if (input.checked()) {
            checkedRadio = &input;
            return false;
        }
        return true;
    });
    return checkedRadio;
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
ListAttributeTargetObserver::ListAttributeTargetObserver(const AtomString& id, HTMLInputElement& element)
    : IdTargetObserver(element.treeScope().idTargetObserverRegistry(), id)
    , m_element(element)
{
}

void ListAttributeTargetObserver::idTargetChanged()
{
    m_element->document().eventLoop().queueTask(TaskSource::DOMManipulation, [element = m_element] {
        if (element)
            element->dataListMayHaveChanged();
    });
}
#endif

ExceptionOr<void> HTMLInputElement::setRangeText(StringView replacement, unsigned start, unsigned end, const String& selectionMode)
{
    if (!m_inputType->supportsSelectionAPI())
        return Exception { ExceptionCode::InvalidStateError };

    return HTMLTextFormControlElement::setRangeText(replacement, start, end, selectionMode);
}

bool HTMLInputElement::shouldTruncateText(const RenderStyle& style) const
{
    if (!isTextField())
        return false;
    return document().focusedElement() != this && style.textOverflow() == TextOverflow::Ellipsis;
}

void HTMLInputElement::invalidateStyleOnFocusChangeIfNeeded()
{
    if (!isTextField())
        return;
    // Focus change may affect the result of shouldTruncateText().
    if (auto* style = renderStyle(); style && style->textOverflow() == TextOverflow::Ellipsis)
        invalidateStyleForSubtreeInternal();
}

std::optional<unsigned> HTMLInputElement::selectionStartForBindings() const
{
    if (!canHaveSelection() || !m_inputType->supportsSelectionAPI())
        return std::nullopt;

    return selectionStart();
}

ExceptionOr<void> HTMLInputElement::setSelectionStartForBindings(std::optional<unsigned> start)
{
    if (!canHaveSelection() || !m_inputType->supportsSelectionAPI())
        return Exception { ExceptionCode::InvalidStateError, makeString("The input element's type ('"_s, m_inputType->formControlType(), "') does not support selection."_s) };

    setSelectionStart(start.value_or(0));
    return { };
}

std::optional<unsigned> HTMLInputElement::selectionEndForBindings() const
{
    if (!canHaveSelection() || !m_inputType->supportsSelectionAPI())
        return std::nullopt;

    return selectionEnd();
}

ExceptionOr<void> HTMLInputElement::setSelectionEndForBindings(std::optional<unsigned> end)
{
    if (!canHaveSelection() || !m_inputType->supportsSelectionAPI())
        return Exception { ExceptionCode::InvalidStateError, makeString("The input element's type ('"_s, m_inputType->formControlType(), "') does not support selection."_s) };

    setSelectionEnd(end.value_or(0));
    return { };
}

ExceptionOr<String> HTMLInputElement::selectionDirectionForBindings() const
{
    if (!canHaveSelection() || !m_inputType->supportsSelectionAPI())
        return String();

    return String { selectionDirection() };
}

ExceptionOr<void> HTMLInputElement::setSelectionDirectionForBindings(const String& direction)
{
    if (!canHaveSelection() || !m_inputType->supportsSelectionAPI())
        return Exception { ExceptionCode::InvalidStateError, makeString("The input element's type ('"_s, m_inputType->formControlType(), "') does not support selection."_s) };

    setSelectionDirection(direction);
    return { };
}

ExceptionOr<void> HTMLInputElement::setSelectionRangeForBindings(unsigned start, unsigned end, const String& direction)
{
    if (!canHaveSelection() || !m_inputType->supportsSelectionAPI())
        return Exception { ExceptionCode::InvalidStateError, makeString("The input element's type ('"_s, m_inputType->formControlType(), "') does not support selection."_s) };
    
    setSelectionRange(start, end, direction, AXTextStateChangeIntent(), ForBindings::Yes);
    return { };
}

static Ref<StyleGradientImage> autoFillStrongPasswordMaskImage()
{
    return StyleGradientImage::create(
        Style::FunctionNotation<CSSValueLinearGradient, Style::LinearGradient> {
            .parameters = {
                .colorInterpolationMethod = Style::GradientColorInterpolationMethod::legacyMethod(AlphaPremultiplication::Unpremultiplied),
                .gradientLine = { Style::Angle<> { 90 } },
                .stops = {
                    { Color::black,            Style::LengthPercentage<> { Style::Percentage<> { 50 } } },
                    { Color::transparentBlack, Style::LengthPercentage<> { Style::Percentage<> { 100 } } }
                }
            }
        }
    );
}

RenderStyle HTMLInputElement::createInnerTextStyle(const RenderStyle& style)
{
    auto textBlockStyle = RenderStyle::create();
    textBlockStyle.inheritFrom(style);
    adjustInnerTextStyle(style, textBlockStyle);

    textBlockStyle.setWhiteSpaceCollapse(WhiteSpaceCollapse::Preserve);
    textBlockStyle.setTextWrapMode(TextWrapMode::NoWrap);
    textBlockStyle.setOverflowWrap(OverflowWrap::Normal);
    textBlockStyle.setOverflowX(Overflow::Hidden);
    textBlockStyle.setOverflowY(Overflow::Hidden);
    textBlockStyle.setTextOverflow(shouldTruncateText(style) ? TextOverflow::Ellipsis : TextOverflow::Clip);

    textBlockStyle.setDisplay(DisplayType::Block);

    if (hasAutofillStrongPasswordButton() && isMutable()) {
        textBlockStyle.setDisplay(DisplayType::InlineBlock);
        textBlockStyle.setLogicalMaxWidth(Length { 100, LengthType::Percent });
        textBlockStyle.setColor(Color::black.colorWithAlphaByte(153));
        textBlockStyle.setTextOverflow(TextOverflow::Clip);
        textBlockStyle.setMaskImage(autoFillStrongPasswordMaskImage());
        // A stacking context is needed for the mask.
        if (textBlockStyle.hasAutoUsedZIndex())
            textBlockStyle.setUsedZIndex(0);
    }

    auto shouldUseInitialLineHeight = [&] {
        // Do not allow line-height to be smaller than our default.
        if (textBlockStyle.metricsOfPrimaryFont().intLineSpacing() > style.computedLineHeight())
            return true;
        return isText() && !style.logicalHeight().isAuto() && !hasAutofillStrongPasswordButton();
    };
    if (shouldUseInitialLineHeight())
        textBlockStyle.setLineHeight(RenderStyle::initialLineHeight());

    return textBlockStyle;
}

void HTMLInputElement::capsLockStateMayHaveChanged()
{
    m_inputType->capsLockStateMayHaveChanged();
}

String HTMLInputElement::resultForDialogSubmit() const
{
    return m_inputType->resultForDialogSubmit();
}

String HTMLInputElement::placeholder() const
{
    // According to the HTML5 specification, we need to remove CR and LF from
    // the attribute value.
    String attributeValue = attributeWithoutSynchronization(placeholderAttr);
    if (LIKELY(!containsHTMLLineBreak(attributeValue)))
        return attributeValue;

    return attributeValue.removeCharacters([](UChar character) {
        return isHTMLLineBreak(character);
    });
}

bool HTMLInputElement::dirAutoUsesValue() const
{
    return m_inputType->dirAutoUsesValue();
}

float HTMLInputElement::switchAnimationVisuallyOnProgress() const
{
    ASSERT(isSwitch());
    return downcast<CheckboxInputType>(*m_inputType).switchAnimationVisuallyOnProgress();
}

bool HTMLInputElement::isSwitchVisuallyOn() const
{
    ASSERT(isSwitch());
    return downcast<CheckboxInputType>(*m_inputType).isSwitchVisuallyOn();
}

float HTMLInputElement::switchAnimationHeldProgress() const
{
    ASSERT(isSwitch());
    return downcast<CheckboxInputType>(*m_inputType).switchAnimationHeldProgress();
}

bool HTMLInputElement::isSwitchHeld() const
{
    ASSERT(isSwitch());
    return downcast<CheckboxInputType>(*m_inputType).isSwitchHeld();
}

} // namespace
