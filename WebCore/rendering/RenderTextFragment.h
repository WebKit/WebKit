/*
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

#ifndef KHTML_RenderTextFragment_H
#define KHTML_RenderTextFragment_H

#include "RenderText.h"
#include <kxmlcore/PassRefPtr.h>

namespace DOM {
    class DOMStringImpl;
};

namespace khtml
{
// Used to represent a text substring of an element, e.g., for text runs that are split because of
// first letter and that must therefore have different styles (and positions in the render tree).
// We cache offsets so that text transformations can be applied in such a way that we can recover
// the original unaltered string from our corresponding DOM node.
class RenderTextFragment : public RenderText
{
public:
    RenderTextFragment(DOM::NodeImpl*, DOM::DOMStringImpl*, int startOffset, int length, RenderObject* firstLetter = 0);
    RenderTextFragment(DOM::NodeImpl*, DOM::DOMStringImpl*);
    
    virtual bool isTextFragment() const;
    
    virtual void destroy();

    uint start() const { return m_start; }
    uint end() const { return m_end; }
    RenderObject* firstLetter() const { return m_firstLetter; }
    
    DOM::DOMStringImpl* contentString() const { return m_generatedContentStr.get(); }
    virtual PassRefPtr<DOM::DOMStringImpl> originalString() const;
    
private:
    uint m_start;
    uint m_end;
    RefPtr<DOM::DOMStringImpl> m_generatedContentStr;
    RenderObject* m_firstLetter;
};

}

#endif
