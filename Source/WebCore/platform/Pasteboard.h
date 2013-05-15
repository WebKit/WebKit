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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include <wtf/ListHashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(GTK)
// FIXME: Why does this include need to be in this header?
#include <PasteboardHelper.h>
#endif

#if PLATFORM(WIN)
#include <windows.h>
typedef struct HWND__* HWND;
#endif

// FIXME: This class is too high-level to be in the platform directory, since it
// uses the DOM and makes calls to Editor. It should either be divested of its
// knowledge of the frame and editor or moved into the editing directory.

namespace WebCore {

#if PLATFORM(MAC)
#if PLATFORM(IOS)
// FIXME: This is only temporary until Pasteboard is refactored for iOS.
extern NSString *WebArchivePboardType;
#else
extern const char* WebArchivePboardType;
#endif
extern const char* WebSmartPastePboardType;
extern const char* WebURLNamePboardType;
extern const char* WebURLPboardType;
extern const char* WebURLsWithTitlesPboardType;
#endif

class Clipboard;
class DocumentFragment;
class Frame;
class HitTestResult;
class KURL;
class Node;
class Range;
class SharedBuffer;

enum ShouldSerializeSelectedTextForClipboard { DefaultSelectedTextType, IncludeImageAltTextForClipboard };
    
class Pasteboard {
    WTF_MAKE_NONCOPYABLE(Pasteboard); WTF_MAKE_FAST_ALLOCATED;
public:
    enum SmartReplaceOption {
        CanSmartReplace,
        CannotSmartReplace
    };

#if PLATFORM(MAC) && !PLATFORM(IOS)
    static PassOwnPtr<Pasteboard> create(const String& pasteboardName);
    String name() const { return m_pasteboardName; }

    // This is required to support OS X services.
    void writeSelectionForTypes(const Vector<String>& pasteboardTypes, bool canSmartCopyOrDelete, Frame*, ShouldSerializeSelectedTextForClipboard);
    explicit Pasteboard(const String& pasteboardName);
    static PassRefPtr<SharedBuffer> getDataSelection(Frame*, const String& pasteboardType);
#endif

#if PLATFORM(MAC)
    static String getStringSelection(Frame*, ShouldSerializeSelectedTextForClipboard);
#endif

    static Pasteboard* generalPasteboard();

    bool hasData();
    ListHashSet<String> types();

    String readString(const String& type);
    Vector<String> readFilenames();

    bool writeString(const String& type, const String& data);
    void writeSelection(Range*, bool canSmartCopyOrDelete, Frame*, ShouldSerializeSelectedTextForClipboard = DefaultSelectedTextType);
    void writePlainText(const String&, SmartReplaceOption);
#if !PLATFORM(IOS)
    void writeURL(const KURL&, const String&, Frame* = 0);
    void writeImage(Node*, const KURL&, const String& title);
#else
    void writeImage(Node*, Frame*);
    void writePlainText(const String&, Frame*);
    static NSArray* supportedPasteboardTypes();
#endif
    void writePasteboard(const Pasteboard& sourcePasteboard);
    void writeClipboard(Clipboard*);

    void clear();
    void clear(const String& type);

    bool canSmartReplace();

#if ENABLE(DRAG_SUPPORT)
    void setDragImage(DragImageRef, const IntPoint& hotSpot);
#endif

    // FIXME: Having these functions here is a layering violation.
    // These functions need to move to the editing directory even if they have platform-specific aspects.
    PassRefPtr<DocumentFragment> documentFragment(Frame*, PassRefPtr<Range>, bool allowPlainText, bool& chosePlainText);
    String plainText(Frame* = 0);
    
#if PLATFORM(QT) || PLATFORM(GTK)
    bool isSelectionMode() const;
    void setSelectionMode(bool);
#else
    bool isSelectionMode() const { return false; }
    void setSelectionMode(bool) { }
#endif

#if PLATFORM(GTK)
    ~Pasteboard();
#endif

private:
    Pasteboard();

#if PLATFORM(MAC) && !PLATFORM(IOS)
    String m_pasteboardName;
    long m_changeCount;
#endif

#if PLATFORM(IOS)
    PassRefPtr<DocumentFragment> documentFragmentForPasteboardItemAtIndex(Frame*, int index, bool allowPlainText, bool& chosePlainText);
#endif

#if PLATFORM(WIN)
    HWND m_owner;
#endif

#if PLATFORM(QT)
    bool m_selectionMode;
#endif

};

} // namespace WebCore

#endif // Pasteboard_h
