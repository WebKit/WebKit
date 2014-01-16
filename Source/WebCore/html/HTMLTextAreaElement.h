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
    static PassRefPtr<HTMLTextAreaElement> create(const QualifiedName&, Document&, HTMLFormElement*);

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }

    bool shouldWrapText() const { return m_wrap != NoWrap; }

    virtual String value() const override;
    void setValue(const String&);
    String defaultValue() const;
    void setDefaultValue(const String&);
    int textLength() const { return value().length(); }
    virtual int maxLength() const override;
    void setMaxLength(int, ExceptionCode&);
    // For ValidityState
    virtual String validationMessage() const override;
    virtual bool valueMissing() const override;
    virtual bool tooLong() const override;
    bool isValidValue(const String&) const;
    
    virtual TextControlInnerTextElement* innerTextElement() const override;

    void rendererWillBeDestroyed();

    void setCols(int);
    void setRows(int);

    virtual bool willRespondToMouseClickEvents() override;

private:
    HTMLTextAreaElement(const QualifiedName&, Document&, HTMLFormElement*);

    enum WrapMethod { NoWrap, SoftWrap, HardWrap };

    virtual void didAddUserAgentShadowRoot(ShadowRoot*) override;
    virtual bool areAuthorShadowsAllowed() const override { return false; }

    void handleBeforeTextInsertedEvent(BeforeTextInsertedEvent*) const;
    static String sanitizeUserInputValue(const String&, unsigned maxLength);
    void updateValue() const;
    void setNonDirtyValue(const String&);
    void setValueCommon(const String&);

    virtual bool supportsPlaceholder() const override { return true; }
    virtual HTMLElement* placeholderElement() const override;
    virtual void updatePlaceholderText() override;
    virtual bool isEmptyValue() const override { return value().isEmpty(); }

    virtual bool isOptionalFormControl() const override { return !isRequiredFormControl(); }
    virtual bool isRequiredFormControl() const override { return isRequired(); }

    virtual void defaultEventHandler(Event*) override;
    
    virtual void subtreeHasChanged() override;

    virtual bool isEnumeratable() const override { return true; }
    virtual bool supportLabels() const override { return true; }

    virtual const AtomicString& formControlType() const override;

    virtual FormControlState saveFormControlState() const override;
    virtual void restoreFormControlState(const FormControlState&) override;

    virtual bool isTextFormControl() const override { return true; }

    virtual void childrenChanged(const ChildChange&) override;
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual bool isPresentationAttribute(const QualifiedName&) const override;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;
    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;
    virtual bool appendFormData(FormDataList&, bool) override;
    virtual void reset() override;
    virtual bool hasCustomFocusLogic() const override;
    virtual bool isMouseFocusable() const override;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const override;
    virtual void updateFocusAppearance(bool restorePreviousSelection) override;

    virtual void accessKeyAction(bool sendMouseEvents) override;

    virtual bool shouldUseInputMethod() override;
    virtual bool matchesReadOnlyPseudoClass() const override;
    virtual bool matchesReadWritePseudoClass() const override;

    bool valueMissing(const String& value) const { return isRequiredFormControl() && !isDisabledOrReadOnly() && value.isEmpty(); }
    bool tooLong(const String&, NeedsToCheckDirtyFlag) const;

    int m_rows;
    int m_cols;
    WrapMethod m_wrap;
    HTMLElement* m_placeholder;
    mutable String m_value;
    mutable bool m_isDirty;
    mutable bool m_wasModifiedByUser;
};

NODE_TYPE_CASTS(HTMLTextAreaElement)

} //namespace

#endif
