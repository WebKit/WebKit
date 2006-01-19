/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
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
#include "config.h"
#include "HTMLBaseFontElementImpl.h"
#include "htmlnames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLBaseFontElementImpl::HTMLBaseFontElementImpl(DocumentImpl *doc)
    : HTMLElementImpl(basefontTag, doc)
{
}

DOMString HTMLBaseFontElementImpl::color() const
{
    return getAttribute(colorAttr);
}

void HTMLBaseFontElementImpl::setColor(const DOMString &value)
{
    setAttribute(colorAttr, value);
}

DOMString HTMLBaseFontElementImpl::face() const
{
    return getAttribute(faceAttr);
}

void HTMLBaseFontElementImpl::setFace(const DOMString &value)
{
    setAttribute(faceAttr, value);
}

DOMString HTMLBaseFontElementImpl::size() const
{
    return getAttribute(sizeAttr);
}

void HTMLBaseFontElementImpl::setSize(const DOMString &value)
{
    setAttribute(sizeAttr, value);
}

}
