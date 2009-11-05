/*
 * Copyright (C) 2006, 2007 Rob Buis
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
#ifndef StyleElement_h
#define StyleElement_h

#include "CSSStyleSheet.h"

namespace WebCore {

class Element;

class StyleElement {
public:
    StyleElement();
    virtual ~StyleElement() {}

protected:
    StyleSheet* sheet(Element*);

    virtual void setLoading(bool) {}

    virtual const AtomicString& type() const = 0;
    virtual const AtomicString& media() const = 0;

    void insertedIntoDocument(Document*, Element*);
    void removedFromDocument(Document*);
    void process(Element*);

    void createSheet(Element* e, const String& text = String());

protected:
    RefPtr<CSSStyleSheet> m_sheet;
};

} //namespace

#endif
