/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2010, 2014 Apple Inc. All rights reserved.
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

#ifndef HTMLKeygenElement_h
#define HTMLKeygenElement_h

#include "HTMLFormControlElementWithState.h"

namespace WebCore {

class HTMLSelectElement;

class HTMLKeygenElement final : public HTMLFormControlElementWithState {
public:
    static Ref<HTMLKeygenElement> create(const QualifiedName&, Document&, HTMLFormElement*);

    void setKeytype(const AtomicString&);
    String keytype() const;

private:
    HTMLKeygenElement(const QualifiedName&, Document&, HTMLFormElement*);

    bool computeWillValidate() const override { return false; }
    bool canStartSelection() const override { return false; }

    void parseAttribute(const QualifiedName&, const AtomicString&) override;

    bool appendFormData(FormDataList&, bool) override;
    const AtomicString& formControlType() const override;
    bool isOptionalFormControl() const override { return false; }

    bool isEnumeratable() const override { return true; }
    bool supportLabels() const override { return true; }

    void reset() override;
    bool shouldSaveAndRestoreFormControlState() const override;

    bool canHaveUserAgentShadowRoot() const final { return true; }

    bool isKeytypeRSA() const;

    HTMLSelectElement* shadowSelect() const;
};

} //namespace

#endif
