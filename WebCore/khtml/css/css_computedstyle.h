/**
 * css_computedstyle.h
 *
 * Copyright (C)  2004  Zack Rusin <zack@kde.org>
 * Copyright (C)  2004  Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307  USA
 */
#ifndef CSS_COMPUTEDSTYLE_H
#define CSS_COMPUTEDSTYLE_H

#include "css/css_valueimpl.h"
#include "dom/dom_string.h"

namespace khtml {
    class RenderObject;
}

namespace DOM {

class CSSProperty;
class CSSStyleDeclarationImpl;
class CSSValueImpl;
class DOMString;
class NodeImpl;

class CSSComputedStyleDeclarationImpl : public CSSStyleDeclarationImpl
{
public:
    CSSComputedStyleDeclarationImpl(NodeImpl *node);
    virtual ~CSSComputedStyleDeclarationImpl();

    virtual DOMString cssText() const;
    virtual void setCssText(const DOMString &);

    virtual CSSValueImpl *getPropertyCSSValue(int propertyID) const;
    virtual DOMString getPropertyValue(int propertyID) const;
    virtual bool getPropertyPriority(int propertyID) const;

    virtual DOMString removeProperty(int propertyID);
    virtual bool setProperty(int propertyId, const DOMString &value, bool important = false);
    virtual void setProperty(int propertyId, int value, bool important = false);
    virtual void setLengthProperty(int id, const DOMString &value, bool important, bool multiLength = false);

    virtual void setProperty(const DOMString &propertyString);
    virtual DOMString item(unsigned long index) const;

private:
    CSSProperty property(int id) const;

    khtml::RenderObject *m_renderer;
};


}

#endif
