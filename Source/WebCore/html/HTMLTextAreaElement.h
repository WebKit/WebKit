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

#ifndef HTMLTextAreaElement_h
#define HTMLTextAreaElement_h

#include "HTMLTextFormControlElement.h"

namespace WebCore {

class BeforeTextInsertedEvent;
class VisibleSelection;

class HTMLTextAreaElement final : public HTMLTextFormControlElement {
public:
    static Ref<HTMLTextAreaElement> create(const QualifiedName&, Document&, HTMLFormElement*);

    unsigned cols() const { return m_cols; }
    unsigned rows() const { return m_rows; }

    bool shouldWrapText() const { return m_wrap != NoWrap; }

    WEBCORE_EXPORT String value() const override;
    WEBCORE_EXPORT void setValue(const String&);
    String defaultValue() const;
    void setDefaultValue(const String&);
    int textLength() const { return value().length(); }
    int maxLengthForBindings() const { return m_maxLength; }
    int effectiveMaxLength() const { return m_maxLength; }
    // For ValidityState
    String validationMessage() const override;
    bool valueMissing() const override;
    bool tooLong() const override;
    bool isValidValue(const String&) const;
    
    TextControlInnerTextElement* innerTextElement() const override;
    Ref<RenderStyle> createInnerTextStyle(const RenderStyle&) const override;

    void rendererWillBeDestroyed();

    void setCols(unsigned);
    void setRows(unsigned);

    bool willRespondToMouseClickEvents() override;

private:
    HTMLTextAreaElement(const QualifiedName&, Document&, HTMLFormElement*);

    enum WrapMethod { NoWrap, SoftWrap, HardWrap };

    void didAddUserAgentShadowRoot(ShadowRoot*) override;
    bool canHaveUserAgentShadowRoot() const override final { return true; }

    void maxLengthAttributeChanged(const AtomicString& newValue);

    void handleBeforeTextInsertedEvent(BeforeTextInsertedEvent*) const;
    static String sanitizeUserInputValue(const String&, unsigned maxLength);
    void updateValue() const;
    void setNonDirtyValue(const String&);
    void setValueCommon(const String&);

    bool supportsPlaceholder() const override { return true; }
    HTMLElement* placeholderElement() const override;
    void updatePlaceholderText() override;
    bool isEmptyValue() const override { return value().isEmpty(); }

    bool isOptionalFormControl() const override { return !isRequiredFormControl(); }
    bool isRequiredFormControl() const override { return isRequired(); }

    void defaultEventHandler(Event*) override;
    
    void subtreeHasChanged() override;

    bool isEnumeratable() const override { return true; }
    bool supportLabels() const override { return true; }

    const AtomicString& formControlType() const override;

    FormControlState saveFormControlState() const override;
    void restoreFormControlState(const FormControlState&) override;

    bool isTextFormControl() const override { return true; }

    void childrenChanged(const ChildChange&) override;
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    bool isPresentationAttribute(const QualifiedName&) const override;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;
    RenderPtr<RenderElement> createElementRenderer(Ref<RenderStyle>&&, const RenderTreePosition&) override;
    bool appendFormData(FormDataList&, bool) override;
    void reset() override;
    bool hasCustomFocusLogic() const override;
    bool isMouseFocusable() const override;
    bool isKeyboardFocusable(KeyboardEvent*) const override;
    void updateFocusAppearance(SelectionRestorationMode, SelectionRevealMode) override;

    void accessKeyAction(bool sendMouseEvents) override;

    bool shouldUseInputMethod() override;
    bool matchesReadWritePseudoClass() const override;

    bool valueMissing(const String& value) const { return isRequiredFormControl() && !isDisabledOrReadOnly() && value.isEmpty(); }
    bool tooLong(const String&, NeedsToCheckDirtyFlag) const;

    unsigned m_rows;
    unsigned m_cols;
    int m_maxLength { -1 };
    WrapMethod m_wrap;
    HTMLElement* m_placeholder;
    mutable String m_value;
    mutable bool m_isDirty;
    mutable bool m_wasModifiedByUser;
};

} //namespace

#endif
