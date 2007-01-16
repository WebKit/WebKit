/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2006 Rob Buis
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

    StyleSheet* sheet() const;

    virtual bool isLoading() const { return false; }

    virtual const AtomicString& type() const = 0;
    virtual const AtomicString& media() const = 0;

    void insertedIntoDocument(Document*);
    void removedFromDocument(Document*);
    void childrenChanged(Element*);

protected:
    RefPtr<CSSStyleSheet> m_sheet;
    bool m_loading;
};

} //namespace

#endif
