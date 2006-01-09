/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#include "RenderTextFragment.h"

using namespace DOM;

namespace khtml {

RenderTextFragment::RenderTextFragment(DOM::NodeImpl* node, DOM::DOMStringImpl* str, int startOffset, int length)
    : RenderText(node, str ? str->substring(startOffset, length) : 0), m_start(startOffset), m_end(length)
{
}

RenderTextFragment::RenderTextFragment(DOM::NodeImpl* node, DOM::DOMStringImpl* str)
    : RenderText(node, str), m_start(0), m_end(str ? str->l : 0), m_generatedContentStr(str)
{
}

bool RenderTextFragment::isTextFragment() const
{
    return true;
}

PassRefPtr<DOMStringImpl> RenderTextFragment::originalString() const
{
    DOM::DOMStringImpl* result = 0;
    if (element())
        result = element()->string();
    else
        result = contentString();
    if (result && (start() > 0 || start() < result->l))
        result = result->substring(start(), end());
    return result;
}

}
