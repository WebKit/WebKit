/*
 * Copyright (c) 2010 Google Inc. All rights reserved.
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

#ifndef ReadableDataObject_h
#define ReadableDataObject_h

#include "PlatformString.h"
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

// Used for one way communication of drag/drop and copy/paste data from the
// browser to the renderer.
class ReadableDataObject : public RefCounted<ReadableDataObject> {
public:
    static PassRefPtr<ReadableDataObject> create(bool isForDragging);

    virtual bool hasData() const;
    virtual HashSet<String> types() const;
    virtual String getData(const String& type, bool& succeeded) const;

    virtual String getURL(String* title) const;
    virtual String getHTML(String* baseURL) const;

    virtual bool hasFilenames() const;
    virtual Vector<String> filenames() const;

private:
    explicit ReadableDataObject(bool isForDragging);

    // This isn't always const... but most of the time it is.
    void ensureTypeCacheInitialized() const;


    bool m_isForDragging;

    // To avoid making a lot of IPC calls for each drag event, we cache some
    // values in the renderer.
    mutable HashSet<String> m_types;
    mutable bool m_containsFilenames;
    mutable bool m_isTypeCacheInitialized;
};

} // namespace WebCore

#endif
