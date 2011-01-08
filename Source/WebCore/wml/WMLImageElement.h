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

#ifndef WMLImageElement_h
#define WMLImageElement_h

#if ENABLE(WML)
#include "WMLElement.h"
#include "WMLImageLoader.h"

namespace WebCore {

class WMLImageElement : public WMLElement {
public:
    static PassRefPtr<WMLImageElement> create(const QualifiedName&, Document*);

    WMLImageElement(const QualifiedName& tagName, Document*);
    virtual ~WMLImageElement();

    virtual bool mapToEntry(const QualifiedName&, MappedAttributeEntry&) const;
    virtual void parseMappedAttribute(Attribute*);

    virtual void attach();
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

    virtual void insertedIntoDocument();
    virtual bool isURLAttribute(Attribute*) const;
    virtual const QualifiedName& imageSourceAttributeName() const;

    String altText() const;

    bool useFallbackAttribute() { return m_useFallbackAttribute; }
    void setUseFallbackAttribute(bool value) { m_useFallbackAttribute = value; }

private:
    WMLImageLoader m_imageLoader;
    bool m_useFallbackAttribute;
};

}

#endif
#endif
