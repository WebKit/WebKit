/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FormAssociatedCustomElement.h"

#include "CustomElementReactionQueue.h"
#include "ElementAncestorIterator.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormElement.h"
#include "NodeRareData.h"
#include "ValidationMessage.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>

namespace WebCore {

using namespace HTMLNames;

FormAssociatedCustomElement::FormAssociatedCustomElement(HTMLMaybeFormAssociatedCustomElement& element)
    : ValidatedFormListedElement { nullptr }
    , m_element { element }
{
}

FormAssociatedCustomElement::~FormAssociatedCustomElement()
{
    clearForm();
}

Ref<FormAssociatedCustomElement> FormAssociatedCustomElement::create(HTMLMaybeFormAssociatedCustomElement& element)
{
    return adoptRef(*new FormAssociatedCustomElement(element));
}

ExceptionOr<void> FormAssociatedCustomElement::setValidity(ValidityStateFlags validityStateFlags, String&& message, HTMLElement* validationAnchor)
{
    ASSERT(m_element->isPrecustomizedOrDefinedCustomElement());

    if (!validityStateFlags.isValid() && message.isEmpty())
        return Exception { TypeError };

    m_validityStateFlags = validityStateFlags;
    setCustomValidity(validityStateFlags.isValid() ? emptyString() : WTFMove(message));

    if (validationAnchor && !validationAnchor->isDescendantOrShadowDescendantOf(*m_element))
        return Exception { NotFoundError };

    m_validationAnchor = validationAnchor;

    return { };
}

String FormAssociatedCustomElement::validationMessage() const
{
    ASSERT(m_element->isPrecustomizedOrDefinedCustomElement());
    return customValidationMessage();
}

ALWAYS_INLINE static CustomElementFormValue cloneIfIsFormData(CustomElementFormValue&& value)
{
    return WTF::switchOn(WTFMove(value), [](RefPtr<DOMFormData> value) -> CustomElementFormValue {
        return value->clone().ptr();
    }, [](const auto& value) -> CustomElementFormValue {
        return value;
    });
}

void FormAssociatedCustomElement::setFormValue(std::optional<CustomElementFormValue>&& submissionValue, std::optional<CustomElementFormValue>&& state)
{
    ASSERT(m_element->isPrecustomizedOrDefinedCustomElement());

    if (submissionValue.has_value())
        m_submissionValue = cloneIfIsFormData(WTFMove(submissionValue.value()));

    m_state = state.has_value() ? cloneIfIsFormData(WTFMove(state.value())) : m_submissionValue;
}

HTMLElement* FormAssociatedCustomElement::validationAnchorElement()
{
    ASSERT(m_element->isDefinedCustomElement());
    return m_validationAnchor.get();
}

bool FormAssociatedCustomElement::computeValidity() const
{
    ASSERT(m_element->isPrecustomizedOrDefinedCustomElement());
    return m_validityStateFlags.isValid();
}

bool FormAssociatedCustomElement::appendFormData(DOMFormData& formData)
{
    ASSERT(m_element->isDefinedCustomElement());

    if (!m_submissionValue.has_value())
        return false;

    WTF::switchOn(m_submissionValue.value(), [&](RefPtr<DOMFormData> value) {
        for (const auto& item : value->items()) {
            WTF::switchOn(item.data, [&](const String& value) {
                formData.append(item.name, value);
            }, [&](RefPtr<File> value) {
                formData.append(item.name, *value);
            });
        }
    }, [&](const String& value) {
        if (!name().isEmpty())
            formData.append(name(), value);
    }, [&](RefPtr<File> value) {
        if (!name().isEmpty())
            formData.append(name(), *value);
    });

    return true;
}

void FormAssociatedCustomElement::formWillBeDestroyed()
{
    m_formWillBeDestroyed = true;
    ValidatedFormListedElement::formWillBeDestroyed();
    m_formWillBeDestroyed = false;
}

bool FormAssociatedCustomElement::isEnumeratable() const
{
    ASSERT(m_element->isDefinedCustomElement());
    return true;
}

void FormAssociatedCustomElement::reset()
{
    ASSERT(m_element->isDefinedCustomElement());
    CustomElementReactionQueue::enqueueFormResetCallbackIfNeeded(*m_element);
}

void FormAssociatedCustomElement::disabledStateChanged()
{
    ASSERT(m_element->isDefinedCustomElement());
    ValidatedFormListedElement::disabledStateChanged();
    CustomElementReactionQueue::enqueueFormDisabledCallbackIfNeeded(*m_element, isDisabled());
}

void FormAssociatedCustomElement::didChangeForm()
{
    ASSERT(m_element->isDefinedCustomElement());
    ValidatedFormListedElement::didChangeForm();
    if (!m_formWillBeDestroyed)
        CustomElementReactionQueue::enqueueFormAssociatedCallbackIfNeeded(*m_element, form());
}

void FormAssociatedCustomElement::willUpgrade()
{
    setDataListAncestorState(TriState::False);
}

void FormAssociatedCustomElement::didUpgrade()
{
    ASSERT(!form());

    HTMLElement& element = asHTMLElement();

    parseFormAttribute(element.attributeWithoutSynchronization(formAttr));
    parseDisabledAttribute(element.attributeWithoutSynchronization(disabledAttr));
    parseReadOnlyAttribute(element.attributeWithoutSynchronization(readonlyAttr));

    setDataListAncestorState(TriState::Indeterminate);
    updateWillValidateAndValidity();
    syncWithFieldsetAncestors(element.parentNode());
    invalidateElementsCollectionCachesInAncestors();
    restoreFormControlStateIfNecessary();
}

void FormAssociatedCustomElement::finishParsingChildren()
{
    if (!FormController::ownerForm(*this))
        restoreFormControlStateIfNecessary();
}

void FormAssociatedCustomElement::invalidateElementsCollectionCachesInAncestors()
{
    auto invalidateElementsCache = [](HTMLElement& element) {
        if (auto* nodeLists = element.nodeLists()) {
            // We kick the "form" attribute to invalidate only the FormControls, FieldSetElements,
            // and RadioNodeList collections, and do so without duplicating invalidation logic of Node.cpp.
            nodeLists->invalidateCachesForAttribute(HTMLNames::formAttr);
        }
    };

    if (RefPtr form = this->form())
        invalidateElementsCache(*form);

    for (auto& ancestor : lineageOfType<HTMLFieldSetElement>(*m_element))
        invalidateElementsCache(ancestor);
}

const AtomString& FormAssociatedCustomElement::formControlType() const
{
    return asHTMLElement().localName();
}

bool FormAssociatedCustomElement::shouldSaveAndRestoreFormControlState() const
{
    const auto& element = asHTMLElement();
    ASSERT(element.reactionQueue());
    return element.isDefinedCustomElement() && element.reactionQueue()->hasFormStateRestoreCallback();
}

FormControlState FormAssociatedCustomElement::saveFormControlState() const
{
    ASSERT(m_element->isDefinedCustomElement());

    FormControlState savedState;

    if (m_state.has_value()) {
        // FIXME: Support File when saving / restoring state.
        // https://bugs.webkit.org/show_bug.cgi?id=249895
        bool didLogMessage = false;
        auto logUnsupportedFileWarning = [&](RefPtr<File>) {
            auto& document = asHTMLElement().document();
            if (document.frame() && !didLogMessage) {
                document.addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "File isn't currently supported when saving / restoring state."_s);
                didLogMessage = true;
            }
        };

        WTF::switchOn(m_state.value(), [&](RefPtr<DOMFormData> state) {
            savedState.reserveInitialCapacity(state->items().size() * 2);

            for (const auto& item : state->items()) {
                WTF::switchOn(item.data, [&](const String& value) {
                    savedState.append(item.name);
                    savedState.append(value);
                }, logUnsupportedFileWarning);
            }

            savedState.shrinkToFit();
        }, [&](const String& state) {
            savedState.append(state);
        }, logUnsupportedFileWarning);
    }

    return savedState;
}

void FormAssociatedCustomElement::restoreFormControlState(const FormControlState& savedState)
{
    ASSERT(m_element->isDefinedCustomElement());

    CustomElementFormValue restoredState;

    if (savedState.size() == 1)
        restoredState.emplace<String>(savedState[0]);
    else {
        auto formData = DOMFormData::create(PAL::UTF8Encoding());
        for (size_t i = 0; i < savedState.size(); i += 2)
            formData->append(savedState[i], savedState[i + 1]);
        restoredState.emplace<RefPtr<DOMFormData>>(formData.ptr());
    }

    CustomElementReactionQueue::enqueueFormStateRestoreCallbackIfNeeded(*m_element, WTFMove(restoredState));
}

} // namespace Webcore
