/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebBlobData_h
#define WebBlobData_h

#include "WebThreadSafeData.h"
#include "WebString.h"
#include "WebURL.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class BlobData; }
namespace WTF { template <typename T> class PassOwnPtr; }
#endif

namespace WebKit {

class WebBlobDataPrivate;

class WebBlobData {
public:
    struct Item {
        enum { TypeData, TypeFile, TypeBlob } type;
        WebThreadSafeData data;
        WebString filePath;
        WebURL blobURL;
        long long offset;
        long long length; // -1 means go to the end of the file/blob.
        double expectedModificationTime; // 0.0 means that the time is not set.
    };

    ~WebBlobData() { reset(); }

    WebBlobData() : m_private(0) { }

    WEBKIT_API void initialize();
    WEBKIT_API void reset();

    bool isNull() const { return !m_private; }

    // Returns the number of items.
    WEBKIT_API size_t itemCount() const;

    // Retrieves the values of the item at the given index.  Returns false if
    // index is out of bounds.
    WEBKIT_API bool itemAt(size_t index, Item& result) const;

    WEBKIT_API WebString contentType() const;

    WEBKIT_API WebString contentDisposition() const;

#if WEBKIT_IMPLEMENTATION
    WebBlobData(const WTF::PassOwnPtr<WebCore::BlobData>&);
    WebBlobData& operator=(const WTF::PassOwnPtr<WebCore::BlobData>&);
    operator WTF::PassOwnPtr<WebCore::BlobData>();
#endif

private:
#if WEBKIT_IMPLEMENTATION
    void assign(const WTF::PassOwnPtr<WebCore::BlobData>&);
#endif
    WebBlobDataPrivate* m_private;
};

} // namespace WebKit

#endif // WebBlobData_h
