/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef HTMLFrameOwnerElement_h
#define HTMLFrameOwnerElement_h

#include "HTMLElement.h"

namespace WebCore {

class DOMWindow;
class Frame;
class KeyboardEvent;

#if ENABLE(SVG)
class SVGDocument;
#endif

class HTMLFrameOwnerElement : public HTMLElement {
protected:
    HTMLFrameOwnerElement(const QualifiedName& tagName, Document*);

public:
    virtual ~HTMLFrameOwnerElement();

    virtual void willRemove();

    Frame* contentFrame() const { return m_contentFrame; }
    DOMWindow* contentWindow() const;
    Document* contentDocument() const;

    virtual bool isFrameOwnerElement() const { return true; }
    virtual bool isKeyboardFocusable(KeyboardEvent*) const { return m_contentFrame; }
    
    virtual ScrollbarMode scrollingMode() const { return ScrollbarAuto; }

#if ENABLE(SVG)
    SVGDocument* getSVGDocument(ExceptionCode&) const;
#endif

private:
    friend class Frame;
    Frame* m_contentFrame;
};

} // namespace WebCore

#endif // HTMLFrameOwnerElement_h
