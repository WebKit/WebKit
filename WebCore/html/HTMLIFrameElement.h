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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef HTMLIFrameElement_H
#define HTMLIFrameElement_H

#include "HTMLFrameElement.h"

namespace WebCore {

class HTMLIFrameElement : public HTMLFrameElement
{
public:
    HTMLIFrameElement(Document *doc);
    ~HTMLIFrameElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    virtual bool mapToEntry(const QualifiedName&, MappedAttributeEntry&) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    virtual void attach();
    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject *createRenderer(RenderArena*, RenderStyle*);
    virtual void recalcStyle(StyleChange);
    
    virtual bool isURLAttribute(Attribute*) const;

    String align() const;
    void setAlign(const String&);

    String height() const;
    void setHeight(const String&);

    String width() const;
    void setWidth(const String&);

    virtual String src() const;

protected:
    virtual void openURL();

    bool needWidgetUpdate;

 private:
    String oldNameAttr;
};

} //namespace

#endif
