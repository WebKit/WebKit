/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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

#ifndef HTMLBodyElement_h
#define HTMLBodyElement_h

#include "HTMLElement.h"

namespace WebCore {

class HTMLBodyElement : public HTMLElement
{
public:
    HTMLBodyElement(const QualifiedName&, Document*);
    ~HTMLBodyElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 10; }
    
    virtual bool mapToEntry(const QualifiedName&, MappedAttributeEntry&) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual void insertedIntoDocument();

    void createLinkDecl();
    
    virtual bool isURLAttribute(Attribute*) const;

    String aLink() const;
    void setALink(const String&);
    String background() const;
    void setBackground(const String&);
    String bgColor() const;
    void setBgColor(const String&);
    String link() const;
    void setLink(const String&);
    String text() const;
    void setText(const String&);
    String vLink() const;
    void setVLink(const String&);

    virtual int scrollLeft() const;
    virtual void setScrollLeft(int scrollLeft);
    
    virtual int scrollTop() const;
    virtual void setScrollTop(int scrollTop);
    
    virtual int scrollHeight() const;
    virtual int scrollWidth() const;
    
    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;
    
protected:
    RefPtr<CSSMutableStyleDeclaration> m_linkDecl;

private:
    virtual void didMoveToNewOwnerDocument();
};

} //namespace

#endif
