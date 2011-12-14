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

#ifndef WebStorageArea_h
#define WebStorageArea_h

#include "WebCommon.h"
#include "WebString.h"

namespace WebKit {

class WebFrame;
class WebURL;

// In WebCore, there's one distinct StorageArea per origin per StorageNamespace. This
// class wraps a StorageArea.  All the methods have obvious connections to the spec:
// http://dev.w3.org/html5/webstorage/
class WebStorageArea {
public:
    virtual ~WebStorageArea() { }

    enum Result {
        ResultOK = 0,
        ResultBlockedByQuota
    };

    // The number of key/value pairs in the storage area.
    virtual unsigned length() = 0;

    // Get a value for a specific key. Valid key indices are 0 through length() - 1.
    // Indexes may change on any set/removeItem call. Will return null if the index
    // provided is out of range.
    virtual WebString key(unsigned index) = 0;

    // Get the value that corresponds to a specific key. This returns null if there is
    // no entry for that key.
    virtual WebString getItem(const WebString& key) = 0;

    // Set the value that corresponds to a specific key. Result will either be ResultOK
    // or some particular error. The value is NOT set when there's an error.  url is the
    // url that should be used if a storage event fires.
    virtual void setItem(const WebString& key, const WebString& newValue, const WebURL& url, Result& result, WebString& oldValue, WebFrame*)
    {
        setItem(key, newValue, url, result, oldValue);
    }
    // FIXME: Remove soon (once Chrome has rolled past this revision).
    virtual void setItem(const WebString& key, const WebString& newValue, const WebURL& url, Result& result, WebString& oldValue)
    {
        setItem(key, newValue, url, result, oldValue, 0);
    }

    // Remove the value associated with a particular key.  url is the url that should be used
    // if a storage event fires.
    virtual void removeItem(const WebString& key, const WebURL& url, WebString& oldValue) = 0;

    // Clear all key/value pairs.  url is the url that should be used if a storage event fires.
    virtual void clear(const WebURL& url, bool& somethingCleared) = 0;
};

} // namespace WebKit

#endif // WebStorageArea_h
