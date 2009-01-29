/*
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#ifndef FormControlElement_h
#define FormControlElement_h

namespace WebCore {

class AtomicString;
class Element;

class FormControlElement {
public:
    virtual ~FormControlElement() { }

    virtual bool isEnabled() const = 0;
    virtual bool isReadOnlyControl() const = 0;
    virtual bool isTextControl() const = 0;

    virtual bool valueMatchesRenderer() const = 0;
    virtual void setValueMatchesRenderer(bool value = true) = 0;

    virtual const AtomicString& name() const = 0;
    virtual const AtomicString& type() const = 0;

protected:
    FormControlElement() { }
};

FormControlElement* toFormControlElement(Element*);

}

#endif
