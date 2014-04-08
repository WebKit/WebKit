/*
 * Copyright (C) 2006, 2013 Apple Inc. All rights reserved.
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

#ifndef Pasteboard_h
#define Pasteboard_h

#include "DragImage.h"
#include "URL.h"
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(GTK)
typedef struct _GtkClipboard GtkClipboard;
#endif

#if PLATFORM(IOS)
OBJC_CLASS NSArray;
OBJC_CLASS NSString;
#endif

#if PLATFORM(WIN)
#include "COMPtr.h"
#include "WCDataObject.h"
#include <objidl.h>
#include <windows.h>
typedef struct HWND__* HWND;
#endif

// FIXME: This class uses the DOM and makes calls to Editor.
// It should be divested of its knowledge of the frame and editor.

namespace WebCore {

class DataObjectGtk;
class DocumentFragment;
class DragData;
class Element;
class Frame;
class Range;
class SharedBuffer;

enum ShouldSerializeSelectedTextForDataTransfer { DefaultSelectedTextType, IncludeImageAltTextForDataTransfer };

// For writing to the pasteboard. Generally sorted with the richest formats on top.

struct PasteboardWebContent {
#if !(PLATFORM(EFL) || PLATFORM(GTK) || PLATFORM(WIN))
    PasteboardWebContent();
    ~PasteboardWebContent();
    bool canSmartCopyOrDelete;
    RefPtr<SharedBuffer> dataInWebArchiveFormat;
    RefPtr<SharedBuffer> dataInRTFDFormat;
    RefPtr<SharedBuffer> dataInRTFFormat;
    String dataInStringFormat;
    Vector<String> clientTypes;
    Vector<RefPtr<SharedBuffer>> clientData;
#endif
};

struct PasteboardURL {
    URL url;
    String title;
#if PLATFORM(MAC)
    String userVisibleForm;
#endif
};

struct PasteboardImage {
    PasteboardImage();
    ~PasteboardImage();
    RefPtr<Image> image;
#if !(PLATFORM(EFL) || PLATFORM(GTK) || PLATFORM(WIN))
    PasteboardURL url;
    RefPtr<SharedBuffer> resourceData;
    String resourceMIMEType;
#endif
};

// For reading from the pasteboard.

class PasteboardWebContentReader {
public:
    virtual ~PasteboardWebContentReader() { }

#if !(PLATFORM(EFL) || PLATFORM(GTK) || PLATFORM(WIN))
    virtual bool readWebArchive(PassRefPtr<SharedBuffer>) = 0;
    virtual bool readFilenames(const Vector<String>&) = 0;
    virtual bool readHTML(const String&) = 0;
    virtual bool readRTFD(PassRefPtr<SharedBuffer>) = 0;
    virtual bool readRTF(PassRefPtr<SharedBuffer>) = 0;
    virtual bool readImage(PassRefPtr<SharedBuffer>, const String& type) = 0;
    virtual bool readURL(const URL&, const String& title) = 0;
#endif
    virtual bool readPlainText(const String&) = 0;
};

struct PasteboardPlainText {
    String text;
#if PLATFORM(MAC)
    bool isURL;
#endif
};

class Pasteboard {
    WTF_MAKE_NONCOPYABLE(Pasteboard); WTF_MAKE_FAST_ALLOCATED;
public:
    ~Pasteboard();

    static PassOwnPtr<Pasteboard> createForCopyAndPaste();
    static PassOwnPtr<Pasteboard> createPrivate(); // Temporary pasteboard. Can put data on this and then write to another pasteboard with writePasteboard.

    bool hasData();
    Vector<String> types();
    String readString(const String& type);

    bool writeString(const String& type, const String& data);
    void clear();
    void clear(const String& type);

    void read(PasteboardPlainText&);
    void read(PasteboardWebContentReader&);

    void write(const PasteboardURL&);
    void write(const PasteboardImage&);
    void write(const PasteboardWebContent&);

    Vector<String> readFilenames();
    bool canSmartReplace();

    void writeMarkup(const String& markup);
    enum SmartReplaceOption { CanSmartReplace, CannotSmartReplace };
    void writePlainText(const String&, SmartReplaceOption); // FIXME: Two separate functions would be clearer than one function with an argument.
    void writePasteboard(const Pasteboard& sourcePasteboard);

#if ENABLE(DRAG_SUPPORT)
    static PassOwnPtr<Pasteboard> createForDragAndDrop();
    static PassOwnPtr<Pasteboard> createForDragAndDrop(const DragData&);

    void setDragImage(DragImageRef, const IntPoint& hotSpot);
#endif

#if PLATFORM(GTK) || PLATFORM(WIN)
    PassRefPtr<DocumentFragment> documentFragment(Frame&, Range&, bool allowPlainText, bool& chosePlainText); // FIXME: Layering violation.
    void writeImage(Element&, const URL&, const String& title); // FIXME: Layering violation.
    void writeSelection(Range&, bool canSmartCopyOrDelete, Frame&, ShouldSerializeSelectedTextForDataTransfer = DefaultSelectedTextType); // FIXME: Layering violation.
#endif

#if PLATFORM(GTK)
    static PassOwnPtr<Pasteboard> create(PassRefPtr<DataObjectGtk>);
    static PassOwnPtr<Pasteboard> create(GtkClipboard*);
    PassRefPtr<DataObjectGtk> dataObject() const;
    static PassOwnPtr<Pasteboard> createForGlobalSelection();
#endif

#if PLATFORM(IOS)
    static NSArray* supportedPasteboardTypes();
    static String resourceMIMEType(const NSString *mimeType);
#endif

#if PLATFORM(MAC)
    explicit Pasteboard(const String& pasteboardName);
    static PassOwnPtr<Pasteboard> create(const String& pasteboardName);

    const String& name() const { return m_pasteboardName; }
#endif

#if PLATFORM(WIN)
    COMPtr<IDataObject> dataObject() const { return m_dataObject; }
    void setExternalDataObject(IDataObject*);
    void writeURLToWritableDataObject(const URL&, const String&);
    COMPtr<WCDataObject> writableDataObject() const { return m_writableDataObject; }
    void writeImageToDataObject(Element&, const URL&); // FIXME: Layering violation.
#endif

private:
    Pasteboard();

#if PLATFORM(GTK)
    Pasteboard(PassRefPtr<DataObjectGtk>);
    Pasteboard(GtkClipboard*);
#endif

#if PLATFORM(WIN)
    explicit Pasteboard(IDataObject*);
    explicit Pasteboard(WCDataObject*);
    explicit Pasteboard(const DragDataMap&);

    void finishCreatingPasteboard();
    void writeRangeToDataObject(Range&, Frame&); // FIXME: Layering violation.
    void writeURLToDataObject(const URL&, const String&);
    void writePlainTextToDataObject(const String&, SmartReplaceOption);
#endif

#if PLATFORM(GTK)
    RefPtr<DataObjectGtk> m_dataObject;
    GtkClipboard* m_gtkClipboard;
#endif

#if PLATFORM(IOS)
    long m_changeCount;
#endif

#if PLATFORM(MAC)
    String m_pasteboardName;
    long m_changeCount;
#endif

#if PLATFORM(WIN)
    HWND m_owner;
    COMPtr<IDataObject> m_dataObject;
    COMPtr<WCDataObject> m_writableDataObject;
    DragDataMap m_dragDataMap;
#endif
};

#if PLATFORM(IOS)
extern NSString *WebArchivePboardType;
#endif

#if PLATFORM(MAC)
extern const char* const WebArchivePboardType;
extern const char* const WebURLNamePboardType;
#endif

#if !PLATFORM(GTK)

inline Pasteboard::~Pasteboard()
{
}

#endif

} // namespace WebCore

#endif // Pasteboard_h
