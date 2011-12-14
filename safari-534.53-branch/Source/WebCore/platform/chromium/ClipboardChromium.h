// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef ClipboardChromium_h
#define ClipboardChromium_h

#include "Clipboard.h"

#include "CachedResourceClient.h"

namespace WebCore {

    class CachedImage;
    class ChromiumDataObject;
    class Frame;
    class IntPoint;

    class ClipboardChromium : public Clipboard, public CachedResourceClient {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        ~ClipboardChromium() {}

        static PassRefPtr<ClipboardChromium> create(
            ClipboardType, PassRefPtr<ChromiumDataObject>, ClipboardAccessPolicy, Frame*);

        // Returns the file name (not including the extension). This removes any
        // invalid file system characters as well as making sure the
        // path + extension is not bigger than allowed by the file system.
        // This may change the file extension in dataObject.
        static String validateFileName(const String& title, ChromiumDataObject* dataObject);

        virtual void clearData(const String& type);
        void clearAllData();
        String getData(const String& type, bool& success) const;
        bool setData(const String& type, const String& data);

        // extensions beyond IE's API
        virtual HashSet<String> types() const;
        virtual PassRefPtr<FileList> files() const;

        void setDragImage(CachedImage*, const IntPoint&);
        void setDragImageElement(Node*, const IntPoint&);

        PassRefPtr<ChromiumDataObject> dataObject()
        {
            return m_dataObject;
        }

        virtual DragImageRef createDragImage(IntPoint& dragLoc) const;
        virtual void declareAndWriteDragImage(Element*, const KURL&, const String& title, Frame*);
        virtual void writeURL(const KURL&, const String&, Frame*);
        virtual void writeRange(Range*, Frame*);
        virtual void writePlainText(const String&);

        virtual bool hasData();

#if ENABLE(DATA_TRANSFER_ITEMS)
        virtual PassRefPtr<DataTransferItems> items();
#endif

    private:
        ClipboardChromium(ClipboardType, PassRefPtr<ChromiumDataObject>, ClipboardAccessPolicy, Frame*);

        void resetFromClipboard();
        void setDragImage(CachedImage*, Node*, const IntPoint&);
        RefPtr<ChromiumDataObject> m_dataObject;
        Frame* m_frame;
    };

} // namespace WebCore

#endif // ClipboardChromium_h
