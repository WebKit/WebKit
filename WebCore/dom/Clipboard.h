/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
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

#ifndef Clipboard_h
#define Clipboard_h

#include "AtomicString.h"
#include "Node.h"
#include "Shared.h"

namespace WebCore {

    class CachedImage;
    class DeprecatedStringList;
    class IntPoint;

    // State available during IE's events for drag and drop and copy/paste
    class Clipboard : public Shared<Clipboard> {
    public:
        virtual ~Clipboard() { }

        // Is this operation a drag-drop or a copy-paste?
        virtual bool isForDragging() const = 0;

        virtual String dropEffect() const = 0;
        virtual void setDropEffect(const String&) = 0;
        virtual String effectAllowed() const = 0;
        virtual void setEffectAllowed(const String&) = 0;
    
        virtual void clearData(const String& type) = 0;
        virtual void clearAllData() = 0;
        virtual String getData(const String& type, bool& success) const = 0;
        virtual bool setData(const String& type, const String& data) = 0;
    
        // extensions beyond IE's API
        virtual DeprecatedStringList types() const = 0;
    
        virtual IntPoint dragLocation() const = 0;
        virtual CachedImage* dragImage() const = 0;
        virtual void setDragImage(CachedImage*, const IntPoint&) = 0;
        virtual Node* dragImageElement() = 0;
        virtual void setDragImageElement(Node*, const IntPoint&) = 0;
    };

} // namespace WebCore

#endif // Clipboard_h
