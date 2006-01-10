/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#include "HTMLFieldSetElementImpl.h"

#include "rendering/render_form.h"

using namespace khtml;

namespace DOM {

using namespace HTMLNames;

HTMLFieldSetElementImpl::HTMLFieldSetElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
   : HTMLGenericFormElementImpl(fieldsetTag, doc, f)
{
}

HTMLFieldSetElementImpl::~HTMLFieldSetElementImpl()
{
}

bool HTMLFieldSetElementImpl::checkDTD(const NodeImpl* newChild)
{
    return newChild->hasTagName(legendTag) || HTMLElementImpl::checkDTD(newChild);
}

bool HTMLFieldSetElementImpl::isFocusable() const
{
    return false;
}

DOMString HTMLFieldSetElementImpl::type() const
{
    return "fieldset";
}

RenderObject* HTMLFieldSetElementImpl::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return new (arena) RenderFieldset(this);
}

} // namespace
