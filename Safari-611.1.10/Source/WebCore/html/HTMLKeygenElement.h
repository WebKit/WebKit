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

#pragma once

#include "HTMLFormControlElementWithState.h"

namespace WebCore {

class HTMLSelectElement;

class HTMLKeygenElement final : public HTMLFormControlElementWithState {
    WTF_MAKE_ISO_ALLOCATED(HTMLKeygenElement);
public:
    static Ref<HTMLKeygenElement> create(const QualifiedName&, Document&, HTMLFormElement*);

    void setKeytype(const AtomString&);
    String keytype() const;

private:
    HTMLKeygenElement(const QualifiedName&, Document&, HTMLFormElement*);

    bool computeWillValidate() const final { return false; }
    bool canStartSelection() const final { return false; }

    void parseAttribute(const QualifiedName&, const AtomString&) final;

    bool appendFormData(DOMFormData&, bool) final;
    const AtomString& formControlType() const final;
    bool isOptionalFormControl() const final { return false; }

    bool isEnumeratable() const final { return true; }
    bool supportLabels() const final { return true; }

    int defaultTabIndex() const final;

    void reset() final;
    bool shouldSaveAndRestoreFormControlState() const final;

    bool isKeytypeRSA() const;

    HTMLSelectElement* shadowSelect() const;
};

} // namespace WebCore
