/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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

#include "HTMLTextFormControlElement.h"
#include "SelectionRestorationMode.h"

namespace WebCore {

class BeforeTextInsertedEvent;
class RenderTextControlMultiLine;
class VisibleSelection;

class HTMLTextAreaElement final : public HTMLTextFormControlElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLTextAreaElement);
public:
    WEBCORE_EXPORT static Ref<HTMLTextAreaElement> create(Document&);
    static Ref<HTMLTextAreaElement> create(const QualifiedName&, Document&, HTMLFormElement*);

    unsigned cols() const { return m_cols; }
    unsigned rows() const { return m_rows; }

    bool shouldWrapText() const { return m_wrap != NoWrap; }

    WEBCORE_EXPORT String value() const final;
    WEBCORE_EXPORT void setValue(const String&);
    WEBCORE_EXPORT String defaultValue() const;
    WEBCORE_EXPORT void setDefaultValue(const String&);
    int textLength() const { return value().length(); }
    int effectiveMaxLength() const { return maxLength(); }
    // For ValidityState
    String validationMessage() const final;
    bool valueMissing() const final;
    bool tooShort() const final;
    bool tooLong() const final;
    bool isValidValue(const String&) const;
    
    WEBCORE_EXPORT RefPtr<TextControlInnerTextElement> innerTextElement() const final;
    RenderStyle createInnerTextStyle(const RenderStyle&) final;
    void copyNonAttributePropertiesFromElement(const Element&) final;

    void rendererWillBeDestroyed();

    WEBCORE_EXPORT void setCols(unsigned);
    WEBCORE_EXPORT void setRows(unsigned);

    bool willRespondToMouseClickEvents() final;

    RenderTextControlMultiLine* renderer() const;

private:
    HTMLTextAreaElement(const QualifiedName&, Document&, HTMLFormElement*);

    enum WrapMethod { NoWrap, SoftWrap, HardWrap };

    void didAddUserAgentShadowRoot(ShadowRoot&) final;

    void maxLengthAttributeChanged(const AtomString& newValue);
    void minLengthAttributeChanged(const AtomString& newValue);

    void handleBeforeTextInsertedEvent(BeforeTextInsertedEvent&) const;
    static String sanitizeUserInputValue(const String&, unsigned maxLength);
    void updateValue() const;
    void setNonDirtyValue(const String&);
    void setValueCommon(const String&);

    bool supportsPlaceholder() const final { return true; }
    HTMLElement* placeholderElement() const final;
    void updatePlaceholderText() final;
    bool isEmptyValue() const final { return value().isEmpty(); }

    bool isOptionalFormControl() const final { return !isRequiredFormControl(); }
    bool isRequiredFormControl() const final { return isRequired(); }

    void defaultEventHandler(Event&) final;
    
    void subtreeHasChanged() final;

    bool isEnumeratable() const final { return true; }
    bool supportLabels() const final { return true; }

    bool isInteractiveContent() const final { return true; }

    const AtomString& formControlType() const final;

    FormControlState saveFormControlState() const final;
    void restoreFormControlState(const FormControlState&) final;

    bool isTextField() const final { return true; }

    void childrenChanged(const ChildChange&) final;
    void parseAttribute(const QualifiedName&, const AtomString&) final;
    bool hasPresentationalHintsForAttribute(const QualifiedName&) const final;
    void collectPresentationalHintsForAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) final;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    bool appendFormData(DOMFormData&, bool) final;
    void reset() final;
    bool hasCustomFocusLogic() const final;
    int defaultTabIndex() const final;
    bool isMouseFocusable() const final;
    bool isKeyboardFocusable(KeyboardEvent*) const final;
    void updateFocusAppearance(SelectionRestorationMode, SelectionRevealMode) final;

    bool accessKeyAction(bool sendMouseEvents) final;

    bool shouldUseInputMethod() final;
    bool matchesReadWritePseudoClass() const final;

    bool valueMissing(const String& value) const { return isRequiredFormControl() && !isDisabledOrReadOnly() && value.isEmpty(); }
    bool tooShort(StringView, NeedsToCheckDirtyFlag) const;
    bool tooLong(StringView, NeedsToCheckDirtyFlag) const;

    unsigned m_rows;
    unsigned m_cols;
    WrapMethod m_wrap { SoftWrap };
    RefPtr<HTMLElement> m_placeholder;
    mutable String m_value;
    mutable bool m_isDirty { false };
    mutable bool m_wasModifiedByUser { false };
};

} //namespace
