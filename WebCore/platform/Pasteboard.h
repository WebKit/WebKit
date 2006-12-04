/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef PASTEBOARD_H_
#define PASTEBOARD_H_

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>

// FIXME: This class is too high-level to be in the platform directory, since it
// uses the DOM and makes calls to Editor. It should either be divested of its
// knowledge of the frame and editor or moved into the editing directory.

#if PLATFORM(MAC)
class NSPasteboard;
class NSArray;
#endif

#if PLATFORM(WIN)
#include <windows.h>
typedef struct HWND__* HWND;
#endif

#if PLATFORM(MAC)
// FIXME: These should be moved into the WebCore namespace.
extern NSString *WebArchivePboardType;
extern NSString *WebSmartPastePboardType;
extern NSString *WebURLNamePboardType;
extern NSString *WebURLPboardType;
extern NSString *WebURLsWithTitlesPboardType;
#endif

namespace WebCore {

class CString;
class DeprecatedCString;
class DocumentFragment;
class Frame;
class Range;
class String;

class Pasteboard : Noncopyable {
public:
    static Pasteboard* generalPasteboard();
    void writeSelection(PassRefPtr<Range>, bool canSmartCopyOrDelete, Frame*);
    void clearTypes();
    bool canSmartReplace();
    PassRefPtr<DocumentFragment> documentFragment(Frame*, PassRefPtr<Range>, bool allowPlainText, bool& chosePlainText);
    String plainText(Frame* = 0);

private:
    Pasteboard();
    ~Pasteboard();

#if PLATFORM(MAC)
    Pasteboard(NSPasteboard *);
    NSArray *selectionPasteboardTypes(bool canSmartCopyOrDelete, bool selectionContainsAttachments);
    NSPasteboard *m_pasteboard;
#endif

#if PLATFORM(WIN)
    HashSet<int> registerSelectionPasteboardTypes();
    void replaceNBSP(String&);
    HGLOBAL createHandle(String);
    HGLOBAL createHandle(CString);
    DeprecatedCString createCF_HTMLFromRange(PassRefPtr<Range>);
    HWND m_owner;
#endif
};

} // namespace WebCore

#endif // PASTEBOARD_H_
