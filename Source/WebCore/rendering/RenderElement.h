/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2009, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
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
 */

#ifndef RenderElement_h
#define RenderElement_h

#include "RenderObject.h"

namespace WebCore {

class RenderElement : public RenderObject {
public:
    virtual ~RenderElement();

    static RenderElement* createFor(Element&, RenderStyle&);

    // This is null for anonymous renderers.
    Element* element() const { return toElement(RenderObject::node()); }
    Element* nonPseudoElement() const { return toElement(RenderObject::nonPseudoNode()); }
    Element* generatingElement() const { return toElement(RenderObject::generatingNode()); }

protected:
    explicit RenderElement(Element*);

private:
    void node() const WTF_DELETED_FUNCTION;
    void nonPseudoNode() const WTF_DELETED_FUNCTION;
    void generatingNode() const WTF_DELETED_FUNCTION;
};

} // namespace WebCore

#endif // RenderElement_h
