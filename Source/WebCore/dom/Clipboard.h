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
#include "Node.h"

namespace WebCore {

    class CachedImage;
    class DataTransferItemList;
    class DragData;
    class DragImageLoader;
    class FileList;
    class Frame;
    class Pasteboard;
    class Range;

    // State available during IE's events for drag and drop and copy/paste
    class Clipboard : public RefCounted<Clipboard> {
    public:
        // Whether this clipboard is serving a drag-drop or copy-paste request.
        enum ClipboardType {
            CopyAndPaste,
            DragAndDrop,
        };
        
        static PassRefPtr<Clipboard> create(ClipboardAccessPolicy, DragData*, Frame*);

        ~Clipboard();

        bool isForCopyAndPaste() const { return m_clipboardType == CopyAndPaste; }
        bool isForDragAndDrop() const { return m_clipboardType == DragAndDrop; }

        String dropEffect() const { return dropEffectIsUninitialized() ? "none" : m_dropEffect; }
        void setDropEffect(const String&);
        bool dropEffectIsUninitialized() const { return m_dropEffect == "uninitialized"; }
        String effectAllowed() const { return m_effectAllowed; }
        void setEffectAllowed(const String&);
    
        void clearData(const String& type);
        void clearData();

        void setDragImage(Element*, int x, int y);

        String getData(const String& type) const;
        bool setData(const String& type, const String& data);
    
        ListHashSet<String> types() const;
        PassRefPtr<FileList> files() const;

        CachedImage* dragImage() const { return m_dragImage.get(); }
        Node* dragImageElement() const { return m_dragImageElement.get(); }
        
        DragImageRef createDragImage(IntPoint& dragLocation) const;

        bool hasData();

        void setAccessPolicy(ClipboardAccessPolicy);
        bool canReadTypes() const;
        bool canReadData() const;
        bool canWriteData() const;
        // Note that the spec doesn't actually allow drag image modification outside the dragstart
        // event. This capability is maintained for backwards compatiblity for ports that have
        // supported this in the past. On many ports, attempting to set a drag image outside the
        // dragstart operation is a no-op anyway.
        bool canSetDragImage() const;

        DragOperation sourceOperation() const;
        DragOperation destinationOperation() const;
        void setSourceOperation(DragOperation);
        void setDestinationOperation(DragOperation);
        
        void setDragHasStarted() { m_dragStarted = true; }

#if ENABLE(DATA_TRANSFER_ITEMS)
        PassRefPtr<DataTransferItemList> items() = 0;
#endif
        
        static PassRefPtr<Clipboard> createForCopyAndPaste(ClipboardAccessPolicy);

        Pasteboard& pasteboard() { return *m_pasteboard; }

#if ENABLE(DRAG_SUPPORT)
        static PassRefPtr<Clipboard> createForDragAndDrop();

        void updateDragImage();
#endif

    protected:
        Clipboard(ClipboardAccessPolicy, ClipboardType, PassOwnPtr<Pasteboard>, bool forFileDrag = false);

        bool dragStarted() const { return m_dragStarted; }
        
    private:
        // Instead of using this member directly, prefer to use the can*() methods above.
        ClipboardAccessPolicy m_policy;
        String m_dropEffect;
        String m_effectAllowed;
        bool m_dragStarted;
        ClipboardType m_clipboardType;
        
    protected:
        IntPoint m_dragLocation;
        CachedResourceHandle<CachedImage> m_dragImage;
        RefPtr<Node> m_dragImageElement;

    private:
        OwnPtr<Pasteboard> m_pasteboard;
        bool m_forFileDrag;
#if ENABLE(DRAG_SUPPORT)
        OwnPtr<DragImageLoader> m_dragImageLoader;
#endif
    };

} // namespace WebCore

#endif // Clipboard_h
