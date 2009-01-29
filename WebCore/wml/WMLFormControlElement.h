/**
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#ifndef WMLFormControlElement_h
#define WMLFormControlElement_h

#if ENABLE(WML)
#include "FormControlElementWithState.h"
#include "FormControlElement.h"
#include "WMLElement.h"

namespace WebCore {

class WMLFormControlElement : public WMLElement, public FormControlElement {
public:
    WMLFormControlElement(const QualifiedName& tagName, Document* document);
    virtual ~WMLFormControlElement();

    virtual bool isEnabled() const { return true; }
    virtual bool isFormControlElement() const { return true; }
    virtual bool isReadOnlyControl() const { return false; }
    virtual bool isTextControl() const { return false; }

    virtual bool valueMatchesRenderer() const { return m_valueMatchesRenderer; }
    virtual void setValueMatchesRenderer(bool b = true) { m_valueMatchesRenderer = b; }

    virtual const AtomicString& name() const { return emptyAtom; }

    virtual bool isFocusable() const;

private:
    bool m_valueMatchesRenderer;
};

class WMLFormControlElementWithState : public WMLFormControlElement, public FormControlElementWithState {
public:
    WMLFormControlElementWithState(const QualifiedName& tagName, Document*);
    virtual ~WMLFormControlElementWithState();

    virtual bool isFormControlElementWithState() const { return true; }
    virtual FormControlElement* toFormControlElement() { return this; }
};

}

#endif
#endif
