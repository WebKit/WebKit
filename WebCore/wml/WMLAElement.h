/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef WMLAElement_h
#define WMLAElement_h

#if ENABLE(WML)
#include "WMLElement.h"

namespace WebCore {

class WMLAElement : public WMLElement {
public:
    WMLAElement(const QualifiedName& tagName, Document*);

    virtual bool supportsFocus() const;
    virtual bool isFocusable() const;
    virtual bool isMouseFocusable() const;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const;

    virtual void parseMappedAttribute(MappedAttribute*);
    virtual void defaultEventHandler(Event*);

    virtual void accessKeyAction(bool fullAction);
    virtual bool isURLAttribute(Attribute*) const;

    virtual String target() const;
};

}

#endif
#endif
