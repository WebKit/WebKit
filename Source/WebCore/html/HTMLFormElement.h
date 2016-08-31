/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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

#pragma once

#include "FormState.h"
#include "FormSubmission.h"
#include "HTMLElement.h"
#include "RadioButtonGroups.h"
#include <memory>

#if ENABLE(IOS_AUTOCORRECT_AND_AUTOCAPITALIZE)
#include "Autocapitalize.h"
#endif

namespace WebCore {

class Event;
class FormAssociatedElement;
class FormData;
class HTMLFormControlElement;
class HTMLFormControlsCollection;
class HTMLImageElement;
class HTMLInputElement;
class TextEncoding;

class HTMLFormElement final : public HTMLElement {
public:
    static Ref<HTMLFormElement> create(Document&);
    static Ref<HTMLFormElement> create(const QualifiedName&, Document&);
    virtual ~HTMLFormElement();

    Ref<HTMLFormControlsCollection> elements();
    WEBCORE_EXPORT Ref<HTMLCollection> elementsForNativeBindings();
    Vector<Ref<Element>> namedElements(const AtomicString&);

    WEBCORE_EXPORT unsigned length() const;
    HTMLElement* item(unsigned index);

    String enctype() const { return m_attributes.encodingType(); }
    WEBCORE_EXPORT void setEnctype(const String&);

    bool shouldAutocomplete() const;

    WEBCORE_EXPORT void setAutocomplete(const AtomicString&);
    WEBCORE_EXPORT const AtomicString& autocomplete() const;

#if ENABLE(IOS_AUTOCORRECT_AND_AUTOCAPITALIZE)
    WEBCORE_EXPORT bool autocorrect() const;
    WEBCORE_EXPORT void setAutocorrect(bool);

    WEBCORE_EXPORT WebAutocapitalizeType autocapitalizeType() const;
    WEBCORE_EXPORT const AtomicString& autocapitalize() const;
    WEBCORE_EXPORT void setAutocapitalize(const AtomicString&);
#endif

    // FIXME: Should rename these two functions to say "form control" or "form-associated element" instead of "form element".
    void registerFormElement(FormAssociatedElement*);
    void removeFormElement(FormAssociatedElement*);

    void registerInvalidAssociatedFormControl(const HTMLFormControlElement&);
    void removeInvalidAssociatedFormControlIfNeeded(const HTMLFormControlElement&);

    void registerImgElement(HTMLImageElement*);
    void removeImgElement(HTMLImageElement*);

    void prepareForSubmission(Event&); // FIXME: This function doesn't only prepare, it sometimes calls submit() itself.
    WEBCORE_EXPORT void submit();
    void submitFromJavaScript();
    WEBCORE_EXPORT void reset();

    void setDemoted(bool demoted) { m_wasDemoted = demoted; }

    void submitImplicitly(Event&, bool fromImplicitSubmissionTrigger);
    bool formWouldHaveSecureSubmission(const String& url);

    String name() const;

    bool noValidate() const;

    String acceptCharset() const { return m_attributes.acceptCharset(); }
    void setAcceptCharset(const String&);

    String action() const;
    void setAction(const String&);

    WEBCORE_EXPORT String method() const;
    WEBCORE_EXPORT void setMethod(const String&);

    String target() const final;

    bool wasUserSubmitted() const;

    HTMLFormControlElement* defaultButton() const;
    void resetDefaultButton();

    WEBCORE_EXPORT bool checkValidity();

#if ENABLE(REQUEST_AUTOCOMPLETE)
    enum class AutocompleteResult {
        Success,
        ErrorDisabled,
        ErrorCancel,
        ErrorInvalid,
    };

    void requestAutocomplete();
    void finishRequestAutocomplete(AutocompleteResult);
#endif

    RadioButtonGroups& radioButtonGroups() { return m_radioButtonGroups; }

    const Vector<FormAssociatedElement*>& associatedElements() const { return m_associatedElements; }
    const Vector<HTMLImageElement*>& imageElements() const { return m_imageElements; }

    void getTextFieldValues(StringPairVector& fieldNamesAndValues) const;

    static HTMLFormElement* findClosestFormAncestor(const Element&);

private:
    HTMLFormElement(const QualifiedName&, Document&);

    bool rendererIsNeeded(const RenderStyle&) final;
    InsertionNotificationRequest insertedInto(ContainerNode&) final;
    void removedFrom(ContainerNode&) final;
    void finishParsingChildren() final;

    void handleLocalEvents(Event&) final;

    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    bool isURLAttribute(const Attribute&) const final;

    void resumeFromDocumentSuspension() final;

    void didMoveToNewDocument(Document* oldDocument) final;

    void copyNonAttributePropertiesFromElement(const Element&) final;

    void submit(Event*, bool activateSubmitButton, bool processingUserGesture, FormSubmissionTrigger);

    unsigned formElementIndexWithFormAttribute(Element*, unsigned rangeStart, unsigned rangeEnd);
    unsigned formElementIndex(FormAssociatedElement*);

    // Returns true if the submission should proceed.
    bool validateInteractively(Event&);

    // Validates each of the controls, and stores controls of which 'invalid'
    // event was not canceled to the specified vector. Returns true if there
    // are any invalid controls in this form.
    bool checkInvalidControlsAndCollectUnhandled(Vector<RefPtr<FormAssociatedElement>>&);

    HTMLElement* elementFromPastNamesMap(const AtomicString&) const;
    void addToPastNamesMap(FormNamedItem*, const AtomicString& pastName);
    void assertItemCanBeInPastNamesMap(FormNamedItem*) const;
    void removeFromPastNamesMap(FormNamedItem*);

    bool matchesValidPseudoClass() const final;
    bool matchesInvalidPseudoClass() const final;

    typedef HashMap<RefPtr<AtomicStringImpl>, FormNamedItem*> PastNamesMap;

    FormSubmission::Attributes m_attributes;
    std::unique_ptr<PastNamesMap> m_pastNamesMap;

    RadioButtonGroups m_radioButtonGroups;
    mutable HTMLFormControlElement* m_defaultButton { nullptr };

    unsigned m_associatedElementsBeforeIndex;
    unsigned m_associatedElementsAfterIndex;
    Vector<FormAssociatedElement*> m_associatedElements;
    Vector<HTMLImageElement*> m_imageElements;
    HashSet<const HTMLFormControlElement*> m_invalidAssociatedFormControls;

    bool m_wasUserSubmitted;
    bool m_isSubmittingOrPreparingForSubmission;
    bool m_shouldSubmit;

    bool m_isInResetFunction;

    bool m_wasDemoted;

#if ENABLE(REQUEST_AUTOCOMPLETE)
    void requestAutocompleteTimerFired();

    Vector<RefPtr<Event>> m_pendingAutocompleteEvents;
    Timer m_requestAutocompleteTimer;
#endif
};

} // namespace WebCore
