/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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

#ifndef HTMLAreaElement_H
#define HTMLAreaElement_H

#include "HTMLAnchorElement.h"
#include "Path.h"
#include "RenderObject.h" // for RenderObject::NodeInfo

namespace WebCore {

class HTMLAreaElement : public HTMLAnchorElement {
public:
    enum Shape { Default, Poly, Rect, Circle, Unknown };

    HTMLAreaElement(Document*);
    ~HTMLAreaElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual void parseMappedAttribute(MappedAttribute*);

    bool isDefault() const { return m_shape == Default; }

    bool mapMouseEvent(int x, int y, const IntSize&, RenderObject::NodeInfo&);

    virtual IntRect getRect(RenderObject*) const;

    String accessKey() const;
    void setAccessKey(const String&);

    String alt() const;
    void setAlt(const String&);

    String coords() const;
    void setCoords(const String&);

    String href() const;
    void setHref(const String&);

    bool noHref() const;
    void setNoHref(bool);

    String shape() const;
    void setShape(const String&);

    int tabIndex() const;
    void setTabIndex(int);

    String target() const;
    void setTarget(const String&);

protected:
    Path getRegion(const IntSize&) const;
    Path region;
    Length* m_coords;
    int m_coordsLen;
    IntSize m_lastSize;
    Shape m_shape;
};

} //namespace

#endif
