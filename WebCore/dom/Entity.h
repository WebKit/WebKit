/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef Entity_h
#define Entity_h

#include "ContainerNode.h"

namespace WebCore {

class Entity : public ContainerNode
{
public:
    Entity(Document*);
    Entity(Document*, const String& name);
    Entity(Document*, const String& publicId, const String& systemId, const String& notationName);

    // DOM methods & attributes for Entity
    String publicId() const { return m_publicId; }
    String systemId() const { return m_systemId; }
    String notationName() const { return m_notationName; }

    virtual String nodeName() const;
    virtual NodeType nodeType() const;
    virtual PassRefPtr<Node> cloneNode(bool deep);
    virtual bool childTypeAllowed(NodeType);
    virtual String toString() const;

private:
    String m_publicId;
    String m_systemId;
    String m_notationName;
    String m_name;
};

} //namespace

#endif
