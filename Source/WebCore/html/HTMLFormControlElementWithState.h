/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

using FormControlState = Vector<String>;

class HTMLFormControlElementWithState : public HTMLFormControlElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLFormControlElementWithState);
public:
    virtual ~HTMLFormControlElementWithState();

    virtual bool shouldSaveAndRestoreFormControlState() const;
    virtual FormControlState saveFormControlState() const;
    virtual void restoreFormControlState(const FormControlState&) { } // Called only if state is not empty.

protected:
    HTMLFormControlElementWithState(const QualifiedName& tagName, Document&, HTMLFormElement*);

    virtual bool shouldAutocomplete() const;

    bool canContainRangeEndPoint() const override { return false; }
    void finishParsingChildren() override;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;

private:
    bool isFormControlElementWithState() const final;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLFormControlElementWithState)
    static bool isType(const WebCore::FormAssociatedElement& element) { return element.isFormControlElementWithState(); }
SPECIALIZE_TYPE_TRAITS_END()
