/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2013 Apple Inc. All rights reserved.
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

#ifndef Clipboard_h
#define Clipboard_h

#include "CachedResourceHandle.h"
#include "ClipboardAccessPolicy.h"
#include "DragActions.h"
#include "DragImage.h"
#include "IntPoint.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

    class CachedImage;
    class DataTransferItemList;
    class DragData;
    class DragImageLoader;
    class Element;
    class FileList;
    class Pasteboard;

    class Clipboard : public RefCounted<Clipboard> {
    public:
        static PassRefPtr<Clipboard> createForCopyAndPaste(ClipboardAccessPolicy);

        ~Clipboard();

        String dropEffect() const;
        void setDropEffect(const String&);

        String effectAllowed() const;
        void setEffectAllowed(const String&);

        Vector<String> types() const;

        PassRefPtr<FileList> files() const;

        void clearData(const String& type);
        void clearData();

        String getData(const String& type) const;

        bool setData(const String& type, const String& data);

        void setDragImage(Element*, int x, int y);

#if ENABLE(DATA_TRANSFER_ITEMS)
        PassRefPtr<DataTransferItemList> items() = 0;
#endif

        void setAccessPolicy(ClipboardAccessPolicy);
        bool canReadTypes() const;
        bool canReadData() const;
        bool canWriteData() const;

        Pasteboard& pasteboard() { return *m_pasteboard; }

#if ENABLE(DRAG_SUPPORT)
        static PassRefPtr<Clipboard> createForDragAndDrop();
        static PassRefPtr<Clipboard> createForDragAndDrop(ClipboardAccessPolicy, const DragData&);

        bool dropEffectIsUninitialized() const { return m_dropEffect == "uninitialized"; }

        DragOperation sourceOperation() const;
        DragOperation destinationOperation() const;
        void setSourceOperation(DragOperation);
        void setDestinationOperation(DragOperation);

        void setDragHasStarted() { m_shouldUpdateDragImage = true; }
        DragImageRef createDragImage(IntPoint& dragLocation) const;
        void updateDragImage();
#endif

    private:
        enum ClipboardType { CopyAndPaste, DragAndDrop };
        Clipboard(ClipboardAccessPolicy, PassOwnPtr<Pasteboard>, ClipboardType = CopyAndPaste, bool forFileDrag = false);

#if ENABLE(DRAG_SUPPORT)
        bool canSetDragImage() const;
#endif

        ClipboardAccessPolicy m_policy;
        OwnPtr<Pasteboard> m_pasteboard;

#if ENABLE(DRAG_SUPPORT)
        bool m_forDrag;
        bool m_forFileDrag;
        String m_dropEffect;
        String m_effectAllowed;
        bool m_shouldUpdateDragImage;
        IntPoint m_dragLocation;
        CachedResourceHandle<CachedImage> m_dragImage;
        RefPtr<Element> m_dragImageElement;
        std::unique_ptr<DragImageLoader> m_dragImageLoader;
#endif
    };

} // namespace WebCore

#endif // Clipboard_h
