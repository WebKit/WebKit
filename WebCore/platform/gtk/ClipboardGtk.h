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

#ifndef ClipboardGdk_h
#define ClipboardGdk_h

#include "Clipboard.h"

namespace WebCore {
    class CachedImage;

    // State available during IE's events for drag and drop and copy/paste
    // Created from the EventHandlerGtk to be used by the dom
    class ClipboardGtk : public Clipboard {
    public:
        static PassRefPtr<ClipboardGtk> create(ClipboardAccessPolicy policy, bool isForDragging)
        {
            return adoptRef(new ClipboardGtk(policy, isForDragging));
        }
        virtual ~ClipboardGtk();

        void clearData(const String&);
        void clearAllData();
        String getData(const String&, bool&) const;
        bool setData(const String&, const String&);

        HashSet<String> types() const;
        IntPoint dragLocation() const;
        CachedImage* dragImage() const;
        void setDragImage(CachedImage*, const IntPoint&);
        Node* dragImageElement();
        void setDragImageElement(Node*, const IntPoint&);

        virtual DragImageRef createDragImage(IntPoint&) const;
        virtual void declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*);
        virtual void writeURL(const KURL&, const String&, Frame*);
        virtual void writeRange(Range*, Frame*);

        virtual bool hasData();

    private:
        ClipboardGtk(ClipboardAccessPolicy, bool);
    };
}

#endif
