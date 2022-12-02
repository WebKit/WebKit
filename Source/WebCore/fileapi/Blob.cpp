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
#include "PolicyContainer.h"
#include "ReadableStream.h"
#include "ReadableStreamSource.h"
#include "ScriptExecutionContext.h"
#include "SharedBuffer.h"
#include "ThreadableBlobRegistry.h"
#include "WebCoreOpaqueRoot.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/CString.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Blob);

class BlobURLRegistry final : public URLRegistry {
public:
    void registerURL(const ScriptExecutionContext&, const URL&, URLRegistrable&) final;
    void unregisterURL(const URL&) final;
    void unregisterURLsForContext(const ScriptExecutionContext&) final;

    static URLRegistry& registry();

    Lock m_urlsPerContextLock;
    HashMap<ScriptExecutionContextIdentifier, HashSet<URL>> m_urlsPerContext WTF_GUARDED_BY_LOCK(m_urlsPerContextLock);
};

void BlobURLRegistry::registerURL(const ScriptExecutionContext& context, const URL& publicURL, URLRegistrable& blob)
{
    ASSERT(&blob.registry() == this);
    {
        Locker locker { m_urlsPerContextLock };
        m_urlsPerContext.add(context.identifier(), HashSet<URL>()).iterator->value.add(publicURL.isolatedCopy());
    }
    ThreadableBlobRegistry::registerBlobURL(context.securityOrigin(), context.policyContainer(), publicURL, static_cast<Blob&>(blob).url());
}

void BlobURLRegistry::unregisterURL(const URL& url)
{
    bool isURLRegistered = false;
    {
        Locker locker { m_urlsPerContextLock };
        for (auto& [contextIdentifier, urls] : m_urlsPerContext) {
            if (!urls.remove(url))
                continue;
            if (urls.isEmpty())
                m_urlsPerContext.remove(contextIdentifier);
            isURLRegistered = true;
            break;
        }
    }
    if (!isURLRegistered)
        return;

    ThreadableBlobRegistry::unregisterBlobURL(url);
}

void BlobURLRegistry::unregisterURLsForContext(const ScriptExecutionContext& context)
{
    HashSet<URL> urlsForContext;
    {
        Locker locker { m_urlsPerContextLock };
        urlsForContext = m_urlsPerContext.take(context.identifier());
    }
    for (auto& url : urlsForContext)
        ThreadableBlobRegistry::unregisterBlobURL(url);
}

URLRegistry& BlobURLRegistry::registry()
{
    static NeverDestroyed<BlobURLRegistry> instance;
    return instance;
}

Blob::Blob(UninitializedContructor, ScriptExecutionContext* context, URL&& url, String&& type)
    : ActiveDOMObject(context)
    , m_type(WTFMove(type))
    , m_internalURL(WTFMove(url))
{
}

Blob::Blob(ScriptExecutionContext* context)
    : ActiveDOMObject(context)
    , m_size(0)
    , m_internalURL(BlobURL::createInternalURL())
{
    ThreadableBlobRegistry::registerBlobURL(m_internalURL, { }, { });
}

static size_t computeMemoryCost(const Vector<BlobPartVariant>& blobPartVariants)
{
    size_t memoryCost = 0;
    for (auto& blobPartVariant : blobPartVariants) {
        WTF::switchOn(blobPartVariant, [&](const RefPtr<Blob>& blob) {
            memoryCost += blob->memoryCost();
        }, [&](const RefPtr<JSC::ArrayBufferView>& view) {
            memoryCost += view->byteLength();
        }, [&](const RefPtr<JSC::ArrayBuffer>& array) {
            memoryCost += array->byteLength();
        }, [&](const String& string) {
            memoryCost += string.sizeInBytes();
        });
    }
    return memoryCost;
}

static Vector<BlobPart> buildBlobData(Vector<BlobPartVariant>&& blobPartVariants, const BlobPropertyBag& propertyBag)
{
    BlobBuilder builder(propertyBag.endings);
    for (auto& blobPartVariant : blobPartVariants) {
        WTF::switchOn(blobPartVariant,
            [&] (auto& part) {
                builder.append(WTFMove(part));
            }
        );
    }
    return builder.finalize();
}

Blob::Blob(ScriptExecutionContext& context, Vector<BlobPartVariant>&& blobPartVariants, const BlobPropertyBag& propertyBag)
    : ActiveDOMObject(&context)
    , m_type(normalizedContentType(propertyBag.type))
    , m_memoryCost(computeMemoryCost(blobPartVariants))
    , m_internalURL(BlobURL::createInternalURL())
{
    ThreadableBlobRegistry::registerBlobURL(m_internalURL, buildBlobData(WTFMove(blobPartVariants), propertyBag), m_type);
}

Blob::Blob(ScriptExecutionContext* context, Vector<uint8_t>&& data, const String& contentType)
    : ActiveDOMObject(context)
    , m_type(contentType)
    , m_size(data.size())
    , m_memoryCost(data.size())
    , m_internalURL(BlobURL::createInternalURL())
{
    ThreadableBlobRegistry::registerBlobURL(m_internalURL, { BlobPart(WTFMove(data)) }, contentType);
}

Blob::Blob(ReferencingExistingBlobConstructor, ScriptExecutionContext* context, const Blob& blob)
    : ActiveDOMObject(context)
    , m_type(blob.type())
    , m_size(blob.size())
    , m_memoryCost(blob.memoryCost())
    , m_internalURL(BlobURL::createInternalURL())
{
    ThreadableBlobRegistry::registerBlobURL(m_internalURL, { BlobPart(blob.url()) } , m_type);
}

Blob::Blob(DeserializationContructor, ScriptExecutionContext* context, const URL& srcURL, const String& type, std::optional<unsigned long long> size, unsigned long long memoryCost, const String& fileBackedPath)
    : ActiveDOMObject(context)
    , m_type(normalizedContentType(type))
    , m_size(size)
    , m_memoryCost(memoryCost)
    , m_internalURL(BlobURL::createInternalURL())
{
    if (fileBackedPath.isEmpty())
        ThreadableBlobRegistry::registerBlobURL(nullptr, { }, m_internalURL, srcURL);
    else
        ThreadableBlobRegistry::registerBlobURLOptionallyFileBacked(m_internalURL, srcURL, fileBackedPath, m_type);
}

Blob::Blob(ScriptExecutionContext* context, const URL& srcURL, long long start, long long end, unsigned long long memoryCost, const String& type)
    : ActiveDOMObject(context)
    , m_type(normalizedContentType(type))
    , m_memoryCost(memoryCost)
    , m_internalURL(BlobURL::createInternalURL())
    // m_size is not necessarily equal to end - start so we do not initialize it here.
{
    ThreadableBlobRegistry::registerBlobURLForSlice(m_internalURL, srcURL, start, end, m_type);
}

Blob::~Blob()
{
    ThreadableBlobRegistry::unregisterBlobURL(m_internalURL);
    while (!m_blobLoaders.isEmpty())
        (*m_blobLoaders.begin())->cancel();
}

Ref<Blob> Blob::slice(long long start, long long end, const String& contentType) const
{
    unsigned long long sliceMemoryCost = 0;
    if (auto totalMemoryCost = memoryCost()) {
        unsigned long long positiveStart = start > 0 ? std::min<unsigned long long>(start, totalMemoryCost) : totalMemoryCost - std::min<unsigned long long>(-start, totalMemoryCost);
        unsigned long long positiveEnd = end > 0 ? std::min<unsigned long long>(end, totalMemoryCost) : totalMemoryCost - std::min<unsigned long long>(-end, totalMemoryCost);
        if (positiveStart < positiveEnd)
            sliceMemoryCost = positiveEnd - positiveStart;
        ASSERT(sliceMemoryCost <= totalMemoryCost);
    }
    auto blob = adoptRef(*new Blob(scriptExecutionContext(), m_internalURL, start, end, sliceMemoryCost, contentType));
    blob->suspendIfNeeded();
    return blob;
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

void Blob::loadBlob(FileReaderLoader::ReadType readType, CompletionHandler<void(BlobLoader&)>&& completionHandler)
{
    auto blobLoader = makeUnique<BlobLoader>([this, pendingActivity = makePendingActivity(*this), completionHandler = WTFMove(completionHandler)](BlobLoader& blobLoader) mutable {
        completionHandler(blobLoader);
        m_blobLoaders.take(&blobLoader);
    });

    blobLoader->start(*this, scriptExecutionContext(), readType);

    if (blobLoader->isLoading())
        m_blobLoaders.add(WTFMove(blobLoader));
}

void Blob::text(Ref<DeferredPromise>&& promise)
{
    loadBlob(FileReaderLoader::ReadAsText, [promise = WTFMove(promise)](BlobLoader& blobLoader) mutable {
        if (auto optionalErrorCode = blobLoader.errorCode()) {
            promise->reject(Exception { *optionalErrorCode });
            return;
        }
        promise->resolve<IDLDOMString>(blobLoader.stringResult());
    });
}

void Blob::arrayBuffer(Ref<DeferredPromise>&& promise)
{
    loadBlob(FileReaderLoader::ReadAsArrayBuffer, [promise = WTFMove(promise)](BlobLoader& blobLoader) mutable {
        if (auto optionalErrorCode = blobLoader.errorCode()) {
            promise->reject(Exception { *optionalErrorCode });
            return;
        }
        auto arrayBuffer = blobLoader.arrayBufferResult();
        if (!arrayBuffer) {
            promise->reject(Exception { InvalidStateError });
            return;
        }
        promise->resolve<IDLArrayBuffer>(*arrayBuffer);
    });
}

ExceptionOr<Ref<ReadableStream>> Blob::stream()
{
    class BlobStreamSource : public FileReaderLoaderClient, public ReadableStreamSource {
    public:
        BlobStreamSource(ScriptExecutionContext& scriptExecutionContext, Blob& blob)
            : m_loader(makeUniqueRef<FileReaderLoader>(FileReaderLoader::ReadType::ReadAsBinaryChunks, this))
        {
            m_loader->start(&scriptExecutionContext, blob);
        }

    private:
        // ReadableStreamSource
        void setActive() final { }
        void setInactive() final { }
        void doStart() final
        {
            m_isStarted = true;
            if (m_exception)
                controller().error(*m_exception);
        }

        void doPull() final { }
        void doCancel() final
        {
            m_loader->cancel();
        }

        // FileReaderLoaderClient
        void didStartLoading() final { }
        void didReceiveData() final { }
        void didReceiveBinaryChunk(const SharedBuffer& buffer) final
        {
            if (!controller().enqueue(buffer.tryCreateArrayBuffer()))
                doCancel();
        }
        void didFinishLoading() final
        {
            controller().close();
        }
        void didFail(ExceptionCode code) final
        {
            Exception exception { code };
            if (!m_isStarted) {
                m_exception = WTFMove(exception);
                return;
            }
            controller().error(exception);
        }

        UniqueRef<FileReaderLoader> m_loader;
        bool m_isStarted { false };
        std::optional<Exception> m_exception;
    };

    auto* context = scriptExecutionContext();
    auto* globalObject = context ? context->globalObject() : nullptr;
    if (!globalObject)
        return Exception { InvalidStateError };
    return ReadableStream::create(*globalObject, adoptRef(*new BlobStreamSource(*context, *this)));
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

URLKeepingBlobAlive Blob::handle() const
{
    return { m_internalURL };
}

WebCoreOpaqueRoot root(Blob* blob)
{
    return WebCoreOpaqueRoot { blob };
}

} // namespace WebCore
