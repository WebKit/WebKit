/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#ifndef WMLTableElement_h
#define WMLTableElement_h

#if ENABLE(WML)
#include "WMLElement.h"

namespace WebCore {

class HTMLCollection;

class WMLTableElement : public WMLElement {
public:
    static PassRefPtr<WMLTableElement> create(const QualifiedName&, Document*);

    WMLTableElement(const QualifiedName& tagName, Document*);
    virtual ~WMLTableElement();

    virtual bool mapToEntry(const QualifiedName&, MappedAttributeEntry&) const;
    virtual void parseMappedAttribute(Attribute*);

    virtual void finishParsingChildren();

private:
    Vector<WMLElement*> scanTableChildElements(WMLElement* startElement, const QualifiedName& tagName) const;

    bool tryMergeAdjacentTextCells(Node* item, Node* nextItem) const;
    void transferAllChildrenOfElementToTargetElement(WMLElement* sourceElement, WMLElement* targetElement, unsigned startOffset) const;
    void joinSuperflousColumns(Vector<WMLElement*>& columnElements, WMLElement* rowElement) const;
    void padWithEmptyColumns(Vector<WMLElement*>& columnElements, WMLElement* rowElement) const;
    void alignCells(Vector<WMLElement*>& columnElements, WMLElement* rowElement) const;

    unsigned m_columns;
    String m_alignment;
};

}

#endif
#endif
