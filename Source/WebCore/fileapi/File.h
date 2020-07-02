/*
 * Copyright (C) 2008-2019 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "Blob.h"
#include <wtf/IsoMalloc.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class File final : public Blob {
    WTF_MAKE_ISO_ALLOCATED_EXPORT(File, WEBCORE_EXPORT);
public:
    struct PropertyBag : BlobPropertyBag {
        Optional<int64_t> lastModified;
    };

    // Create a file with an optional name exposed to the author (via File.name and associated DOM properties) that differs from the one provided in the path.
    WEBCORE_EXPORT static Ref<File> create(const String& path, const String& replacementPath = { }, const String& nameOverride = { });

    // Create a File using the 'new File' constructor.
    static Ref<File> create(Vector<BlobPartVariant>&& blobPartVariants, const String& filename, const PropertyBag& propertyBag)
    {
        return adoptRef(*new File(WTFMove(blobPartVariants), filename, propertyBag));
    }

    static Ref<File> deserialize(const String& path, const URL& srcURL, const String& type, const String& name, const Optional<int64_t>& lastModified = WTF::nullopt)
    {
        return adoptRef(*new File(deserializationContructor, path, srcURL, type, name, lastModified));
    }

    static Ref<File> create(const Blob& blob, const String& name)
    {
        return adoptRef(*new File(blob, name));
    }

    static Ref<File> create(const File& file, const String& name)
    {
        return adoptRef(*new File(file, name));
    }

    static Ref<File> createWithRelativePath(const String& path, const String& relativePath);

    bool isFile() const override { return true; }

    const String& path() const { return m_path; }
    const String& relativePath() const { return m_relativePath; }
    void setRelativePath(const String& relativePath) { m_relativePath = relativePath; }
    const String& name() const { return m_name; }
    WEBCORE_EXPORT int64_t lastModified() const; // Number of milliseconds since Epoch.
    const Optional<int64_t>& lastModifiedOverride() const { return m_lastModifiedDateOverride; } // Number of milliseconds since Epoch.

    static String contentTypeForFile(const String& path);

#if ENABLE(FILE_REPLACEMENT)
    static bool shouldReplaceFile(const String& path);
#endif

    bool isDirectory() const;

private:
    WEBCORE_EXPORT explicit File(const String& path);
    File(URL&&, String&& type, String&& path, String&& name);
    File(Vector<BlobPartVariant>&& blobPartVariants, const String& filename, const PropertyBag&);
    File(const Blob&, const String& name);
    File(const File&, const String& name);

    File(DeserializationContructor, const String& path, const URL& srcURL, const String& type, const String& name, const Optional<int64_t>& lastModified);

    static void computeNameAndContentType(const String& path, const String& nameOverride, String& effectiveName, String& effectiveContentType);
#if ENABLE(FILE_REPLACEMENT)
    static void computeNameAndContentTypeForReplacedFile(const String& path, const String& nameOverride, String& effectiveName, String& effectiveContentType);
#endif

    String m_path;
    String m_relativePath;
    String m_name;

    Optional<int64_t> m_lastModifiedDateOverride;
    mutable Optional<bool> m_isDirectory;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::File)
    static bool isType(const WebCore::Blob& blob) { return blob.isFile(); }
SPECIALIZE_TYPE_TRAITS_END()
