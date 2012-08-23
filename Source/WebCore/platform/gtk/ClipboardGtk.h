/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007, Holger Hans Peter Freyther
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

#ifndef ClipboardGtk_h
#define ClipboardGtk_h

#include "CachedImageClient.h"
#include "Clipboard.h"
#include "DataObjectGtk.h"

namespace WebCore {
    class CachedImage;
    class Frame;
    class PasteboardHelper;

    // State available during IE's events for drag and drop and copy/paste
    // Created from the EventHandlerGtk to be used by the dom
    class ClipboardGtk : public Clipboard, public CachedImageClient {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        static PassRefPtr<ClipboardGtk> create(ClipboardAccessPolicy policy, GtkClipboard* clipboard, Frame* frame)
        {
            return adoptRef(new ClipboardGtk(policy, clipboard, frame));
        }

        static PassRefPtr<ClipboardGtk> create(ClipboardAccessPolicy policy, PassRefPtr<DataObjectGtk> dataObject, ClipboardType clipboardType, Frame* frame)
        {
            return adoptRef(new ClipboardGtk(policy, dataObject, clipboardType, frame));
        }
        virtual ~ClipboardGtk();

        void clearData(const String&);
        void clearAllData();
        String getData(const String&) const;
        bool setData(const String&, const String&);

        virtual Vector<String> types() const;
        virtual PassRefPtr<FileList> files() const;

        void setDragImage(CachedImage*, const IntPoint&);
        void setDragImageElement(Node*, const IntPoint&);
        void setDragImage(CachedImage*, Node*, const IntPoint&);

        virtual DragImageRef createDragImage(IntPoint&) const;
        virtual void declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*);
        virtual void writeURL(const KURL&, const String&, Frame*);
        virtual void writeRange(Range*, Frame*);
        virtual void writePlainText(const String&);

        virtual bool hasData();

        PassRefPtr<DataObjectGtk> dataObject() { return m_dataObject; }
        GtkClipboard* clipboard() { return m_clipboard; }

    private:
        ClipboardGtk(ClipboardAccessPolicy, GtkClipboard*, Frame*);
        ClipboardGtk(ClipboardAccessPolicy, PassRefPtr<DataObjectGtk>, ClipboardType, Frame*);

        RefPtr<DataObjectGtk> m_dataObject;
        GtkClipboard* m_clipboard;
        Frame* m_frame;
    };
}

#endif
