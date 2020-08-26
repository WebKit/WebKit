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

#include "config.h"
#include "Blob.h"

#include "BlobBuilder.h"
#include "BlobLoader.h"
#include "BlobPart.h"
#include "BlobURL.h"
#include "File.h"
#include "JSDOMPromiseDeferred.h"
#include "ScriptExecutionContext.h"
#include "SharedBuffer.h"
#include "ThreadableBlobRegistry.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Blob);

class BlobURLRegistry final : public URLRegistry {
public:
    void registerURL(ScriptExecutionContext&, const URL&, URLRegistrable&) final;
    void unregisterURL(const URL&) final;

    static URLRegistry& registry();
};


void BlobURLRegistry::registerURL(ScriptExecutionContext& context, const URL& publicURL, URLRegistrable& blob)
{
    ASSERT(&blob.registry() == this);
    ThreadableBlobRegistry::registerBlobURL(context.securityOrigin(), publicURL, static_cast<Blob&>(blob).url());
}

void BlobURLRegistry::unregisterURL(const URL& url)
{
    ThreadableBlobRegistry::unregisterBlobURL(url);
}

URLRegistry& BlobURLRegistry::registry()
{
    static NeverDestroyed<BlobURLRegistry> instance;
    return instance;
}

Blob::Blob(UninitializedContructor, ScriptExecutionContext* context, URL&& url, String&& type)
    : ActiveDOMObject(context)
    , m_internalURL(WTFMove(url))
    , m_type(WTFMove(type))
{
}

Blob::Blob(ScriptExecutionContext* context)
    : ActiveDOMObject(context)
    , m_size(0)
{
    m_internalURL = BlobURL::createInternalURL();
    ThreadableBlobRegistry::registerBlobURL(m_internalURL, { },  { });
}

Blob::Blob(ScriptExecutionContext& context, Vector<BlobPartVariant>&& blobPartVariants, const BlobPropertyBag& propertyBag)
    : ActiveDOMObject(&context)
    , m_internalURL(BlobURL::createInternalURL())
    , m_type(normalizedContentType(propertyBag.type))
{
    BlobBuilder builder(propertyBag.endings);
    for (auto& blobPartVariant : blobPartVariants) {
        WTF::switchOn(blobPartVariant,
            [&] (auto& part) {
                builder.append(WTFMove(part));
            }
        );
    }

    ThreadableBlobRegistry::registerBlobURL(m_internalURL, builder.finalize(), m_type);
}

Blob::Blob(ScriptExecutionContext* context, const SharedBuffer& buffer, const String& contentType)
    : ActiveDOMObject(context)
    , m_type(contentType)
    , m_size(buffer.size())
{
    Vector<uint8_t> data;
    data.append(buffer.data(), buffer.size());

    Vector<BlobPart> blobParts;
    blobParts.append(BlobPart(WTFMove(data)));
    m_internalURL = BlobURL::createInternalURL();
    ThreadableBlobRegistry::registerBlobURL(m_internalURL, WTFMove(blobParts), contentType);
}

Blob::Blob(ScriptExecutionContext* context, Vector<uint8_t>&& data, const String& contentType)
    : ActiveDOMObject(context)
    , m_type(contentType)
    , m_size(data.size())
{
    Vector<BlobPart> blobParts;
    blobParts.append(BlobPart(WTFMove(data)));
    m_internalURL = BlobURL::createInternalURL();
    ThreadableBlobRegistry::registerBlobURL(m_internalURL, WTFMove(blobParts), contentType);
}

Blob::Blob(ReferencingExistingBlobConstructor, ScriptExecutionContext* context, const Blob& blob)
    : ActiveDOMObject(context)
    , m_internalURL(BlobURL::createInternalURL())
    , m_type(blob.type())
    , m_size(blob.size())
{
    ThreadableBlobRegistry::registerBlobURL(m_internalURL, { BlobPart(blob.url()) } , m_type);
}

Blob::Blob(DeserializationContructor, ScriptExecutionContext* context, const URL& srcURL, const String& type, Optional<unsigned long long> size, const String& fileBackedPath)
    : ActiveDOMObject(context)
    , m_type(normalizedContentType(type))
    , m_size(size)
{
    m_internalURL = BlobURL::createInternalURL();
    if (fileBackedPath.isEmpty())
        ThreadableBlobRegistry::registerBlobURL(nullptr, m_internalURL, srcURL);
    else
        ThreadableBlobRegistry::registerBlobURLOptionallyFileBacked(m_internalURL, srcURL, fileBackedPath, m_type);
}

Blob::Blob(ScriptExecutionContext* context, const URL& srcURL, long long start, long long end, const String& type)
    : ActiveDOMObject(context)
    , m_type(normalizedContentType(type))
    // m_size is not necessarily equal to end - start so we do not initialize it here.
{
    m_internalURL = BlobURL::createInternalURL();
    ThreadableBlobRegistry::registerBlobURLForSlice(m_internalURL, srcURL, start, end);
}

Blob::~Blob()
{
    while (!m_blobLoaders.isEmpty())
        (*m_blobLoaders.begin())->cancel();

    ThreadableBlobRegistry::unregisterBlobURL(m_internalURL);
}

unsigned long long Blob::size() const
{
    if (!m_size) {
        // FIXME: JavaScript cannot represent sizes as large as unsigned long long, we need to
        // come up with an exception to throw if file size is not representable.
        unsigned long long actualSize = ThreadableBlobRegistry::blobSize(m_internalURL);
        m_size = isInBounds<long long>(actualSize) ? actualSize : 0;
    }

    return *m_size;
}

bool Blob::isValidContentType(const String& contentType)
{
    // FIXME: Do we really want to treat the empty string and null string as valid content types?
    unsigned length = contentType.length();
    for (unsigned i = 0; i < length; ++i) {
        if (contentType[i] < 0x20 || contentType[i] > 0x7e)
            return false;
    }
    return true;
}

String Blob::normalizedContentType(const String& contentType)
{
    if (!isValidContentType(contentType))
        return emptyString();
    return contentType.convertToASCIILowercase();
}

void Blob::loadBlob(ScriptExecutionContext& context, FileReaderLoader::ReadType readType, CompletionHandler<void(std::unique_ptr<BlobLoader>)>&& completionHandler)
{
    auto blobLoader = makeUnique<BlobLoader>([this, pendingActivity = makePendingActivity(*this), completionHandler = WTFMove(completionHandler)](BlobLoader& blobLoader) mutable {
        completionHandler(m_blobLoaders.take(&blobLoader));
    });
    auto* blobLoaderPtr = blobLoader.get();
    m_blobLoaders.add(WTFMove(blobLoader));
    blobLoaderPtr->start(*this, &context, readType);
}

void Blob::text(ScriptExecutionContext& context, Ref<DeferredPromise>&& promise)
{
    loadBlob(context, FileReaderLoader::ReadAsText, [promise = WTFMove(promise)](std::unique_ptr<BlobLoader> blobLoader) mutable {
        if (auto optionalErrorCode = blobLoader->errorCode()) {
            promise->reject(Exception { *optionalErrorCode });
            return;
        }
        promise->resolve<IDLDOMString>(blobLoader->stringResult());
    });
}

void Blob::arrayBuffer(ScriptExecutionContext& context, Ref<DeferredPromise>&& promise)
{
    loadBlob(context, FileReaderLoader::ReadAsArrayBuffer, [promise = WTFMove(promise)](std::unique_ptr<BlobLoader> blobLoader) mutable {
        if (auto optionalErrorCode = blobLoader->errorCode()) {
            promise->reject(Exception { *optionalErrorCode });
            return;
        }
        auto arrayBuffer = blobLoader->arrayBufferResult();
        if (!arrayBuffer) {
            promise->reject(Exception { InvalidStateError });
            return;
        }
        promise->resolve<IDLArrayBuffer>(*arrayBuffer);
    });
}

#if ASSERT_ENABLED
bool Blob::isNormalizedContentType(const String& contentType)
{
    // FIXME: Do we really want to treat the empty string and null string as valid content types?
    unsigned length = contentType.length();
    for (size_t i = 0; i < length; ++i) {
        if (contentType[i] < 0x20 || contentType[i] > 0x7e)
            return false;
        if (isASCIIUpper(contentType[i]))
            return false;
    }
    return true;
}

bool Blob::isNormalizedContentType(const CString& contentType)
{
    // FIXME: Do we really want to treat the empty string and null string as valid content types?
    size_t length = contentType.length();
    const char* characters = contentType.data();
    for (size_t i = 0; i < length; ++i) {
        if (characters[i] < 0x20 || characters[i] > 0x7e)
            return false;
        if (isASCIIUpper(characters[i]))
            return false;
    }
    return true;
}
#endif // ASSERT_ENABLED

URLRegistry& Blob::registry() const
{
    return BlobURLRegistry::registry();
}

const char* Blob::activeDOMObjectName() const
{
    return "Blob";
}


} // namespace WebCore
