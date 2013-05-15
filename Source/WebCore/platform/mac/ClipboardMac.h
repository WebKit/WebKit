/*
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef ClipboardMac_h
#define ClipboardMac_h

#include "CachedImageClient.h"
#include "Clipboard.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS NSImage;

namespace WebCore {

class Frame;
class FileList;

class ClipboardMac : public Clipboard {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum ClipboardContents {
        DragAndDropData,
        DragAndDropFiles,
        CopyAndPasteGeneric
    };

    static PassRefPtr<ClipboardMac> create(ClipboardType clipboardType, const String& pasteboardName, ClipboardAccessPolicy policy, ClipboardContents clipboardContents, Frame*)
    {
        return adoptRef(new ClipboardMac(clipboardType, pasteboardName, policy, clipboardContents));
    }

    virtual ~ClipboardMac();

#if ENABLE(DRAG_SUPPORT)
    virtual void declareAndWriteDragImage(Element*, const KURL&, const String& title, Frame*);
#endif
    
    // Methods for getting info in Cocoa's type system
    const String& pasteboardName() { return m_pasteboardName; }

private:
    ClipboardMac(ClipboardType, const String& pasteboardName, ClipboardAccessPolicy, ClipboardContents);

    String m_pasteboardName;
    int m_changeCount;
};

}

#endif
