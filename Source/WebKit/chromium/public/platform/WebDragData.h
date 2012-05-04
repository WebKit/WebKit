/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebDragData_h
#define WebDragData_h

#include "WebCommon.h"
#include "WebData.h"
#include "WebString.h"
#include "WebURL.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class ChromiumDataObject; }
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebKit {

class WebDragDataPrivate;
template <typename T> class WebVector;

// Holds data that may be exchanged through a drag-n-drop operation.  It is
// inexpensive to copy a WebDragData object.
class WebDragData {
public:
    struct Item {
        enum StorageType {
            // String data with an associated MIME type. Depending on the MIME type, there may be
            // optional metadata attributes as well.
            StorageTypeString,
            // Stores the name of one file being dragged into the renderer.
            StorageTypeFilename,
            // An image being dragged out of the renderer. Contains a buffer holding the image data
            // as well as the suggested name for saving the image to.
            StorageTypeBinaryData,
        };

        StorageType storageType;

        // Only valid when storageType == StorageTypeString.
        WebString stringType;
        WebString stringData;

        // Only valid when storageType == StorageTypeFilename.
        WebString filenameData;
        WebString displayNameData;

        // Only valid when storageType == StorageTypeBinaryData.
        WebData binaryData;

        // Title associated with a link when stringType == "text/uri-list".
        // Filename when storageType == StorageTypeBinaryData.
        WebString title;

        // Only valid when stringType == "text/html".
        WebURL baseURL;
    };

    ~WebDragData() { reset(); }

    WebDragData() : m_private(0) { }
    WebDragData(const WebDragData& d) : m_private(0) { assign(d); }
    WebDragData& operator=(const WebDragData& d)
    {
        assign(d);
        return *this;
    }

    WEBKIT_EXPORT void initialize();
    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebDragData&);

    bool isNull() const { return !m_private; }

    WEBKIT_EXPORT WebVector<Item> items() const;
    WEBKIT_EXPORT void setItems(const WebVector<Item>&);
    WEBKIT_EXPORT void addItem(const Item&);

    WEBKIT_EXPORT WebString filesystemId() const;
    WEBKIT_EXPORT void setFilesystemId(const WebString&);

#if WEBKIT_IMPLEMENTATION
    WebDragData(const WTF::PassRefPtr<WebCore::ChromiumDataObject>&);
    WebDragData& operator=(const WTF::PassRefPtr<WebCore::ChromiumDataObject>&);
    operator WTF::PassRefPtr<WebCore::ChromiumDataObject>() const;
#endif

private:
    void assign(WebDragDataPrivate*);
    void ensureMutable();
    WebDragDataPrivate* m_private;
};

} // namespace WebKit

#endif
