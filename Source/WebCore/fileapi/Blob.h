/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#include "BlobPropertyBag.h"
#include "FileReaderLoader.h"
#include "ScriptExecutionContext.h"
#include "ScriptWrappable.h"
#include <wtf/IsoMalloc.h>
#include <wtf/URL.h>
#include "URLRegistry.h"
#include <wtf/Variant.h>

namespace JSC {
class ArrayBufferView;
class ArrayBuffer;
}

namespace WebCore {

class Blob;
class BlobLoader;
class DeferredPromise;
class ScriptExecutionContext;
class SharedBuffer;

using BlobPartVariant = Variant<RefPtr<JSC::ArrayBufferView>, RefPtr<JSC::ArrayBuffer>, RefPtr<Blob>, String>;

class Blob : public ScriptWrappable, public URLRegistrable, public RefCounted<Blob>, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED_EXPORT(Blob, WEBCORE_EXPORT);
public:
    static Ref<Blob> create(ScriptExecutionContext* context)
    {
        auto blob = adoptRef(*new Blob(context));
        blob->suspendIfNeeded();
        return blob;
    }

    static Ref<Blob> create(ScriptExecutionContext& context, Vector<BlobPartVariant>&& blobPartVariants, const BlobPropertyBag& propertyBag)
    {
        auto blob = adoptRef(*new Blob(context, WTFMove(blobPartVariants), propertyBag));
        blob->suspendIfNeeded();
        return blob;
    }

    static Ref<Blob> create(ScriptExecutionContext* context, const SharedBuffer& buffer, const String& contentType)
    {
        auto blob = adoptRef(*new Blob(context, buffer, contentType));
        blob->suspendIfNeeded();
        return blob;
    }

    static Ref<Blob> create(ScriptExecutionContext* context, Vector<uint8_t>&& data, const String& contentType)
    {
        auto blob = adoptRef(*new Blob(context, WTFMove(data), contentType));
        blob->suspendIfNeeded();
        return blob;
    }

    static Ref<Blob> deserialize(ScriptExecutionContext* context, const URL& srcURL, const String& type, long long size, const String& fileBackedPath)
    {
        ASSERT(Blob::isNormalizedContentType(type));
        auto blob = adoptRef(*new Blob(deserializationContructor, context, srcURL, type, size, fileBackedPath));
        blob->suspendIfNeeded();
        return blob;
    }

    virtual ~Blob();

    const URL& url() const { return m_internalURL; }
    const String& type() const { return m_type; }

    WEBCORE_EXPORT unsigned long long size() const;
    virtual bool isFile() const { return false; }

    // The checks described in the File API spec.
    static bool isValidContentType(const String&);
    // The normalization procedure described in the File API spec.
    static String normalizedContentType(const String&);
#if ASSERT_ENABLED
    static bool isNormalizedContentType(const String&);
    static bool isNormalizedContentType(const CString&);
#endif

    // URLRegistrable
    URLRegistry& registry() const override;

    Ref<Blob> slice(ScriptExecutionContext& context, long long start, long long end, const String& contentType) const
    {
        auto blob = adoptRef(*new Blob(&context, m_internalURL, start, end, contentType));
        blob->suspendIfNeeded();
        return blob;
    }

    void text(ScriptExecutionContext&, Ref<DeferredPromise>&&);
    void arrayBuffer(ScriptExecutionContext&, Ref<DeferredPromise>&&);

protected:
    WEBCORE_EXPORT explicit Blob(ScriptExecutionContext*);
    Blob(ScriptExecutionContext&, Vector<BlobPartVariant>&&, const BlobPropertyBag&);
    Blob(ScriptExecutionContext*, const SharedBuffer&, const String& contentType);
    Blob(ScriptExecutionContext*, Vector<uint8_t>&&, const String& contentType);

    enum ReferencingExistingBlobConstructor { referencingExistingBlobConstructor };
    Blob(ReferencingExistingBlobConstructor, ScriptExecutionContext*, const Blob&);

    enum UninitializedContructor { uninitializedContructor };
    Blob(UninitializedContructor, ScriptExecutionContext*, URL&&, String&& type);

    enum DeserializationContructor { deserializationContructor };
    Blob(DeserializationContructor, ScriptExecutionContext*, const URL& srcURL, const String& type, Optional<unsigned long long> size, const String& fileBackedPath);

    // For slicing.
    Blob(ScriptExecutionContext*, const URL& srcURL, long long start, long long end, const String& contentType);

private:
    void loadBlob(ScriptExecutionContext&, FileReaderLoader::ReadType, CompletionHandler<void(std::unique_ptr<BlobLoader>)>&&);

    // ActiveDOMObject.
    const char* activeDOMObjectName() const override;

    // This is an internal URL referring to the blob data associated with this object. It serves
    // as an identifier for this blob. The internal URL is never used to source the blob's content
    // into an HTML or for FileRead'ing, public blob URLs must be used for those purposes.
    URL m_internalURL;

    String m_type;

    mutable Optional<unsigned long long> m_size;
    HashSet<std::unique_ptr<BlobLoader>> m_blobLoaders;
};

} // namespace WebCore
