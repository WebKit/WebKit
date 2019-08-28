/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010, 2016 Apple Inc. All rights reserved.
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

#include "HTMLFormControlElement.h"

namespace WebCore {

class RenderButton;

class HTMLButtonElement final : public HTMLFormControlElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLButtonElement);
public:
    static Ref<HTMLButtonElement> create(const QualifiedName&, Document&, HTMLFormElement*);

    WEBCORE_EXPORT void setType(const AtomString&);
    
    const AtomString& value() const;

    bool willRespondToMouseClickEvents() final;

    RenderButton* renderer() const;

private:
    HTMLButtonElement(const QualifiedName& tagName, Document&, HTMLFormElement*);

    enum Type { SUBMIT, RESET, BUTTON };

    const AtomString& formControlType() const final;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

    int defaultTabIndex() const final;

    void parseAttribute(const QualifiedName&, const AtomString&) final;
    bool isPresentationAttribute(const QualifiedName&) const final;
    void defaultEventHandler(Event&) final;

    bool appendFormData(DOMFormData&, bool) final;

    bool isEnumeratable() const final { return true; }
    bool supportLabels() const final { return true; }
    bool isInteractiveContent() const final { return true; }

    bool isSuccessfulSubmitButton() const final;
    bool matchesDefaultPseudoClass() const final;
    bool isActivatedSubmit() const final;
    void setActivatedSubmit(bool flag) final;

    void accessKeyAction(bool sendMouseEvents) final;
    bool isURLAttribute(const Attribute&) const final;

    bool canStartSelection() const final { return false; }

    bool isOptionalFormControl() const final { return true; }
    bool computeWillValidate() const final;

    Type m_type;
    bool m_isActivatedSubmit;
};

} // namespace
