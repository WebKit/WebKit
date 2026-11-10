/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012-2024 Apple Inc. All rights reserved.
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

#include <WebCore/ActiveDOMObject.h>
#include <WebCore/BlobPropertyBag.h>
#include <WebCore/BlobURL.h>
#include <WebCore/FileReaderLoader.h>
#include <WebCore/ScriptExecutionContext.h>
#include <WebCore/ScriptWrappable.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/URLKeepingBlobAlive.h>
#include <WebCore/URLRegistry.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/URL.h>

namespace JSC {
class ArrayBufferView;
class ArrayBuffer;
}

namespace WebCore {

class Blob;
class BlobLoader;
class DeferredPromise;
class FragmentedSharedBuffer;
class ReadableStream;
class ScriptExecutionContext;
class FragmentedSharedBuffer;
class WebCoreOpaqueRoot;

struct IDLArrayBuffer;

template<typename> class DOMPromiseDeferred;
template<typename> class ExceptionOr;

using BlobPartVariant = Variant<Ref<JSC::ArrayBufferView>, Ref<JSC::ArrayBuffer>, Ref<Blob>, RefPtr<StringImpl>>;

class Blob : public ScriptWrappable, public URLRegistrable, public RefCounted<Blob>, public ActiveDOMObject {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(Blob, WEBCORE_EXPORT);
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    static Ref<Blob> create(ScriptExecutionContext* context)
    {
        Ref blob = adoptRef(*new Blob(context));
        blob->suspendIfNeeded();
        return blob;
    }

    static Ref<Blob> create(ScriptExecutionContext& context, std::optional<Vector<BlobPartVariant>>&& blobPartVariants, const BlobPropertyBag& propertyBag)
    {
        Ref blob = adoptRef(*new Blob(context, WTF::move(blobPartVariants), propertyBag));
        blob->suspendIfNeeded();
        return blob;
    }

    static Ref<Blob> create(ScriptExecutionContext* context, Vector<uint8_t>&& data, StringImpl* contentType)
    {
        Ref blob = adoptRef(*new Blob(context, WTF::move(data), contentType));
        blob->suspendIfNeeded();
        return blob;
    }

    static Ref<Blob> create(ScriptExecutionContext* context, Ref<FragmentedSharedBuffer>&& buffer, StringImpl* contentType)
    {
        Ref blob = adoptRef(*new Blob(context, WTF::move(buffer), contentType));
        blob->suspendIfNeeded();
        return blob;
    }

    static Ref<Blob> deserialize(ScriptExecutionContext* context, const URL& srcURL, StringImpl* type, long long size, long long memoryCost, StringImpl* fileBackedPath)
    {
        ASSERT(Blob::isNormalizedContentType(type));
        Ref blob = adoptRef(*new Blob(deserializationContructor, context, srcURL, type, size, memoryCost, fileBackedPath));
        blob->suspendIfNeeded();
        return blob;
    }

    virtual ~Blob();

    const URL& url() const LIFETIME_BOUND { return m_internalURL; }
    const RefPtr<StringImpl>& type() const LIFETIME_BOUND { return m_type; }

    WEBCORE_EXPORT unsigned long long size() const;
    virtual bool isFile() const { return false; }

    // The checks described in the File API spec.
    NODELETE static bool isValidContentType(StringImpl*);
    // The normalization procedure described in the File API spec.
    static RefPtr<StringImpl> normalizedContentType(StringImpl*);
#if ASSERT_ENABLED
    static bool isNormalizedContentType(StringImpl*);
    static bool isNormalizedContentType(const ASCIICString&);
#endif

    // URLRegistrable
    URLRegistry& registry() const final;
    RegistrableType registrableType() const final { return RegistrableType::Blob; }

    Ref<Blob> slice(long long start, long long end, StringImpl* contentType) const;

    void text(Ref<DeferredPromise>&&);
    void arrayBuffer(DOMPromiseDeferred<IDLArrayBuffer>&&);
    void getArrayBuffer(CompletionHandler<void(ExceptionOr<Ref<JSC::ArrayBuffer>>)>&&);
    void bytes(Ref<DeferredPromise>&&);
    ExceptionOr<Ref<ReadableStream>> stream();

    size_t memoryCost() const { return m_memoryCost; }

    // Keeping the handle alive will keep the Blob data alive (but not the Blob object).
    URLKeepingBlobAlive handle() const;

protected:
    WEBCORE_EXPORT explicit Blob(ScriptExecutionContext*);
    Blob(ScriptExecutionContext&, std::optional<Vector<BlobPartVariant>>&&, const BlobPropertyBag&);
    Blob(ScriptExecutionContext*, Vector<uint8_t>&&, StringImpl* contentType);
    Blob(ScriptExecutionContext*, Ref<FragmentedSharedBuffer>&&, StringImpl* contentType);

    enum ReferencingExistingBlobConstructor { referencingExistingBlobConstructor };
    Blob(ReferencingExistingBlobConstructor, ScriptExecutionContext*, const Blob&);

    enum UninitializedContructor { uninitializedContructor };
    Blob(UninitializedContructor, ScriptExecutionContext*, URL&&, RefPtr<StringImpl>&& type);

    enum DeserializationContructor { deserializationContructor };
    Blob(DeserializationContructor, ScriptExecutionContext*, const URL& srcURL, StringImpl* type, std::optional<unsigned long long> size, unsigned long long memoryCost, StringImpl* fileBackedPath);

    // For slicing.
    Blob(ScriptExecutionContext*, const URL& srcURL, long long start, long long end, unsigned long long memoryCost, StringImpl* contentType);

private:
    void loadBlob(FileReaderLoader::ReadType, Function<void(BlobLoader&)>&&);

    RefPtr<StringImpl> m_type;
    mutable std::optional<unsigned long long> m_size;
    size_t m_memoryCost { 0 };

    // This is an internal URL referring to the blob data associated with this object. It serves
    // as an identifier for this blob. The internal URL is never used to source the blob's content
    // into an HTML or for FileRead'ing, public blob URLs must be used for those purposes.
    URL m_internalURL;

    HashSet<Ref<BlobLoader>> m_blobLoaders;
};

NODELETE WebCoreOpaqueRoot root(Blob*);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Blob)
    static bool isType(const WebCore::URLRegistrable& registrable) { return registrable.registrableType() == WebCore::URLRegistrable::RegistrableType::Blob; }
SPECIALIZE_TYPE_TRAITS_END()
