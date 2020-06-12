/*
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Color.h"
#include "DragActions.h"
#include "IntPoint.h"
#include <wtf/EnumTraits.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(MAC)

#ifdef __OBJC__ 
#import <Foundation/Foundation.h>
#import <AppKit/NSDragging.h>
typedef id <NSDraggingInfo> DragDataRef;
#else
typedef void* DragDataRef;
#endif

#elif PLATFORM(WIN)
typedef struct IDataObject* DragDataRef;
#elif PLATFORM(GTK)
namespace WebCore {
class SelectionData;
}
typedef WebCore::SelectionData* DragDataRef;
#else
typedef void* DragDataRef;
#endif

namespace WebCore {

enum DragApplicationFlags {
    DragApplicationNone = 0,
    DragApplicationIsModal = 1,
    DragApplicationIsSource = 2,
    DragApplicationHasAttachedSheet = 4,
    DragApplicationIsCopyKeyDown = 8
};

#if PLATFORM(WIN)
typedef HashMap<unsigned, Vector<String>> DragDataMap;
#endif

class DragData {
public:
    enum FilenameConversionPolicy { DoNotConvertFilenames, ConvertFilenames };
    enum class DraggingPurpose { ForEditing, ForFileUpload, ForColorControl };

    // clientPosition is taken to be the position of the drag event within the target window, with (0,0) at the top left
    WEBCORE_EXPORT DragData(DragDataRef, const IntPoint& clientPosition, const IntPoint& globalPosition, OptionSet<DragOperation>, DragApplicationFlags = DragApplicationNone, OptionSet<DragDestinationAction> = anyDragDestinationAction());
    WEBCORE_EXPORT DragData(const String& dragStorageName, const IntPoint& clientPosition, const IntPoint& globalPosition, OptionSet<DragOperation>, DragApplicationFlags = DragApplicationNone, OptionSet<DragDestinationAction> = anyDragDestinationAction());
    // This constructor should used only by WebKit2 IPC because DragData
    // is initialized by the decoder and not in the constructor.
    DragData() = default;
#if PLATFORM(WIN)
    WEBCORE_EXPORT DragData(const DragDataMap&, const IntPoint& clientPosition, const IntPoint& globalPosition, OptionSet<DragOperation> sourceOperationMask, DragApplicationFlags = DragApplicationNone);
    const DragDataMap& dragDataMap();
    void getDragFileDescriptorData(int& size, String& pathname);
    void getDragFileContentData(int size, void* dataBlob);
#endif
    const IntPoint& clientPosition() const { return m_clientPosition; }
    const IntPoint& globalPosition() const { return m_globalPosition; }
    DragApplicationFlags flags() const { return m_applicationFlags; }
    DragDataRef platformData() const { return m_platformDragData; }
    OptionSet<DragOperation> draggingSourceOperationMask() const { return m_draggingSourceOperationMask; }
    bool containsURL(FilenameConversionPolicy = ConvertFilenames) const;
    bool containsPlainText() const;
    bool containsCompatibleContent(DraggingPurpose = DraggingPurpose::ForEditing) const;
    String asURL(FilenameConversionPolicy = ConvertFilenames, String* title = nullptr) const;
    String asPlainText() const;
    Vector<String> asFilenames() const;
    Color asColor() const;
    bool canSmartReplace() const;
    bool containsColor() const;
    bool containsFiles() const;
    unsigned numberOfFiles() const;
    OptionSet<DragDestinationAction> dragDestinationActionMask() const { return m_dragDestinationActionMask; }
    void setFileNames(Vector<String>& fileNames) { m_fileNames = WTFMove(fileNames); }
    const Vector<String>& fileNames() const { return m_fileNames; }
#if PLATFORM(COCOA)
    const String& pasteboardName() const { return m_pasteboardName; }
    bool containsURLTypeIdentifier() const;
    bool containsPromise() const;
#endif

#if PLATFORM(GTK)

    DragData& operator =(const DragData& data)
    {
        m_clientPosition = data.m_clientPosition;
        m_globalPosition = data.m_globalPosition;
        m_platformDragData = data.m_platformDragData;
        m_draggingSourceOperationMask = data.m_draggingSourceOperationMask;
        m_applicationFlags = data.m_applicationFlags;
        m_dragDestinationActionMask = data.m_dragDestinationActionMask;
        return *this;
    }
#endif

private:
    IntPoint m_clientPosition;
    IntPoint m_globalPosition;
    DragDataRef m_platformDragData;
    OptionSet<DragOperation> m_draggingSourceOperationMask;
    DragApplicationFlags m_applicationFlags;
    Vector<String> m_fileNames;
    OptionSet<DragDestinationAction> m_dragDestinationActionMask;
#if PLATFORM(COCOA)
    String m_pasteboardName;
#endif
#if PLATFORM(WIN)
    DragDataMap m_dragDataMap;
#endif
};
    
} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::DragApplicationFlags> {
    using values = EnumValues<
        WebCore::DragApplicationFlags,
        WebCore::DragApplicationFlags::DragApplicationNone,
        WebCore::DragApplicationFlags::DragApplicationIsModal,
        WebCore::DragApplicationFlags::DragApplicationIsSource,
        WebCore::DragApplicationFlags::DragApplicationHasAttachedSheet,
        WebCore::DragApplicationFlags::DragApplicationIsCopyKeyDown
    >;
};

} // namespace WTF
