/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef MimeTypeArray_h
#define MimeTypeArray_h

#include "MimeType.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

    class AtomicString;
    class Frame;
    class PluginData;

    class MimeTypeArray : public RefCounted<MimeTypeArray> {
    public:
        static PassRefPtr<MimeTypeArray> create(Frame* frame) { return adoptRef(new MimeTypeArray(frame)); }
        ~MimeTypeArray();

        void disconnectFrame() { m_frame = 0; }

        unsigned length() const;
        PassRefPtr<MimeType> item(unsigned index);
        bool canGetItemsForName(const AtomicString& propertyName);
        PassRefPtr<MimeType> namedItem(const AtomicString& propertyName);

    private:
        MimeTypeArray(Frame*);
        PluginData* getPluginData() const;

        Frame* m_frame;
    };

} // namespace WebCore

#endif // MimeTypeArray_h
