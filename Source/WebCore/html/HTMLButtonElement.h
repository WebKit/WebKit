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

#ifndef HTMLButtonElement_h
#define HTMLButtonElement_h

#include "HTMLFormControlElement.h"

namespace WebCore {

class HTMLButtonElement final : public HTMLFormControlElement {
public:
    static Ref<HTMLButtonElement> create(const QualifiedName&, Document&, HTMLFormElement*);

    void setType(const AtomicString&);
    
    const AtomicString& value() const;

    bool willRespondToMouseClickEvents() override;

private:
    HTMLButtonElement(const QualifiedName& tagName, Document&, HTMLFormElement*);

    enum Type { SUBMIT, RESET, BUTTON };

    const AtomicString& formControlType() const override;

    RenderPtr<RenderElement> createElementRenderer(Ref<RenderStyle>&&, const RenderTreePosition&) override;

    // HTMLFormControlElement always creates one, but buttons don't need it.
    bool alwaysCreateUserAgentShadowRoot() const override { return false; }
    bool canHaveUserAgentShadowRoot() const final { return true; }

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    bool isPresentationAttribute(const QualifiedName&) const override;
    void defaultEventHandler(Event*) override;

    bool appendFormData(FormDataList&, bool) override;

    bool isEnumeratable() const override { return true; }
    bool supportLabels() const override { return true; }

    bool isSuccessfulSubmitButton() const override;
    bool isActivatedSubmit() const override;
    void setActivatedSubmit(bool flag) override;

    void accessKeyAction(bool sendMouseEvents) override;
    bool isURLAttribute(const Attribute&) const override;

    bool canStartSelection() const override { return false; }

    bool isOptionalFormControl() const override { return true; }
    bool computeWillValidate() const override;

    Type m_type;
    bool m_isActivatedSubmit;
};

} // namespace

#endif
