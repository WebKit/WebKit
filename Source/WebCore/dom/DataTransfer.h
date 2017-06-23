/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003-2016 Apple Inc. All rights reserved.
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

#pragma once

#include "CachedResourceHandle.h"
#include "DataTransferAccessPolicy.h"
#include "DragActions.h"
#include "DragImage.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class CachedImage;
class DragData;
class DragImageLoader;
class Element;
class FileList;
class Pasteboard;

class DataTransfer : public RefCounted<DataTransfer> {
public:
    static Ref<DataTransfer> createForCopyAndPaste(DataTransferAccessPolicy);
    static Ref<DataTransfer> createForInputEvent(const String& plainText, const String& htmlText);

    WEBCORE_EXPORT ~DataTransfer();

    String dropEffect() const;
    void setDropEffect(const String&);

    String effectAllowed() const;
    void setEffectAllowed(const String&);

    Vector<String> types() const;

    FileList& files() const;

    void clearData(const String& type = String());

    String getData(const String& type) const;

    void setData(const String& type, const String& data);

    void setDragImage(Element*, int x, int y);

    void setAccessPolicy(DataTransferAccessPolicy);
    bool canReadTypes() const;
    bool canReadData() const;
    bool canWriteData() const;

    bool hasFileOfType(const String&);
    bool hasStringOfType(const String&);

    Pasteboard& pasteboard() { return *m_pasteboard; }

#if ENABLE(DRAG_SUPPORT)
    static Ref<DataTransfer> createForDrag();
    static Ref<DataTransfer> createForDrop(DataTransferAccessPolicy, const DragData&);

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
    enum Type { CopyAndPaste, DragAndDrop, InputEvent };
    DataTransfer(DataTransferAccessPolicy, std::unique_ptr<Pasteboard>, Type = CopyAndPaste, bool forFileDrag = false);

#if ENABLE(DRAG_SUPPORT)
    bool canSetDragImage() const;
#endif

    DataTransferAccessPolicy m_policy;
    std::unique_ptr<Pasteboard> m_pasteboard;

    mutable RefPtr<FileList> m_fileList;

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
