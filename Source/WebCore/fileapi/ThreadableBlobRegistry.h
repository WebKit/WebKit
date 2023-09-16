/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013, 2014, 2016 Apple Inc. All rights reserved.
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

#pragma once

#include <optional>
#include <wtf/Forward.h>

namespace WebCore {

class BlobPart;
class SecurityOrigin;
class SecurityOriginData;
class URLKeepingBlobAlive;

struct PolicyContainer;

class ThreadableBlobRegistry {
public:
    static void registerBlobURL(SecurityOrigin*, PolicyContainer&&, const URL&, const URL& srcURL, const std::optional<SecurityOriginData>& topOrigin);
    static void registerBlobURL(SecurityOrigin*, PolicyContainer&&, const URLKeepingBlobAlive&, const URL& srcURL);
    static void registerInternalFileBlobURL(const URL&, const String& path, const String& replacementPath, const String& contentType);
    static void registerInternalBlobURL(const URL&, Vector<BlobPart>&& blobParts, const String& contentType);
    static void registerInternalBlobURLOptionallyFileBacked(const URL&, const URL& srcURL, const String& fileBackedPath, const String& contentType);
    static void registerInternalBlobURLForSlice(const URL& newURL, const URL& srcURL, long long start, long long end, const String& contentType);
    static void unregisterBlobURL(const URL&, const std::optional<SecurityOriginData>& topOrigin);
    static void unregisterBlobURL(const URLKeepingBlobAlive&);

    static void registerBlobURLHandle(const URL&, const std::optional<SecurityOriginData>& topOrigin);
    static void unregisterBlobURLHandle(const URL&, const std::optional<SecurityOriginData>& topOrigin);

    WEBCORE_EXPORT static unsigned long long blobSize(const URL&);

    // Returns the origin for the given blob URL. This is because we are not able to embed the unique security origin or the origin of file URL
    // in the blob URL.
    static RefPtr<SecurityOrigin> getCachedOrigin(const URL&);
};

} // namespace WebCore
