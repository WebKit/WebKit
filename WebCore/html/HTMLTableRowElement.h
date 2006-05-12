/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef HTMLTableRowElement_H
#define HTMLTableRowElement_H

#include "HTMLTablePartElement.h"

namespace WebCore {

class HTMLTableRowElement : public HTMLTablePartElement
{
public:
    HTMLTableRowElement(Document* doc);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusOptional; }
    virtual int tagPriority() const { return 7; }
    virtual bool checkDTD(const Node*);
    virtual ContainerNode* addChild(PassRefPtr<Node>);
    
    int rowIndex() const;
    int sectionRowIndex() const;

    HTMLElement* insertCell(int index, ExceptionCode&);
    void deleteCell(int index, ExceptionCode&);

    void setRowIndex(int);

    void setSectionRowIndex(int);

    RefPtr<HTMLCollection> cells();
    void setCells(HTMLCollection *, ExceptionCode&);

    String align() const;
    void setAlign(const String&);

    String bgColor() const;
    void setBgColor(const String&);

    String ch() const;
    void setCh(const String&);

    String chOff() const;
    void setChOff(const String&);

    String vAlign() const;
    void setVAlign(const String&);

protected:
    int ncols;
};

} //namespace

#endif
