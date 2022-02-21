/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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
#include "BlobRegistryImpl.h"

#include "BlobData.h"
#include "BlobPart.h"
#include "BlobResourceHandle.h"
#include "PolicyContainer.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <wtf/CompletionHandler.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

BlobRegistryImpl::~BlobRegistryImpl() = default;

static Ref<ResourceHandle> createBlobResourceHandle(const ResourceRequest& request, ResourceHandleClient* client)
{
    return blobRegistry().blobRegistryImpl()->createResourceHandle(request, client);
}

static void loadBlobResourceSynchronously(NetworkingContext*, const ResourceRequest& request, StoredCredentialsPolicy, ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
{
    auto* blobData = blobRegistry().blobRegistryImpl()->getBlobDataFromURL(request.url());
    BlobResourceHandle::loadResourceSynchronously(blobData, request, error, response, data);
}

static void registerBlobResourceHandleConstructor()
{
    static bool didRegister = false;
    if (!didRegister) {
        ResourceHandle::registerBuiltinConstructor("blob", createBlobResourceHandle);
        ResourceHandle::registerBuiltinSynchronousLoader("blob", loadBlobResourceSynchronously);
        didRegister = true;
    }
}

Ref<ResourceHandle> BlobRegistryImpl::createResourceHandle(const ResourceRequest& request, ResourceHandleClient* client)
{
    auto handle = BlobResourceHandle::createAsync(getBlobDataFromURL(request.url()), request, client);
    handle->start();
    return handle;
}

void BlobRegistryImpl::appendStorageItems(BlobData* blobData, const BlobDataItemList& items, long long offset, long long length)
{
    ASSERT(length != BlobDataItem::toEndOfFile);

    BlobDataItemList::const_iterator iter = items.begin();
    if (offset) {
        for (; iter != items.end(); ++iter) {
            if (offset >= iter->length())
                offset -= iter->length();
            else
                break;
        }
    }

    for (; iter != items.end() && length > 0; ++iter) {
        long long currentLength = iter->length() - offset;
        long long newLength = currentLength > length ? length : currentLength;
        if (iter->type() == BlobDataItem::Type::Data)
            blobData->appendData(iter->data(), iter->offset() + offset, newLength);
        else {
            ASSERT(iter->type() == BlobDataItem::Type::File);
            blobData->appendFile(iter->file(), iter->offset() + offset, newLength);
        }
        length -= newLength;
        offset = 0;
    }
    ASSERT(!length);
}

void BlobRegistryImpl::registerFileBlobURL(const URL& url, Ref<BlobDataFileReference>&& file, const String& contentType)
{
    ASSERT(isMainThread());
    registerBlobResourceHandleConstructor();

    auto blobData = BlobData::create(contentType);
    blobData->appendFile(WTFMove(file));
    addBlobData(url.string(), WTFMove(blobData));
}

void BlobRegistryImpl::registerBlobURL(const URL& url, Vector<BlobPart>&& blobParts, const String& contentType)
{
    ASSERT(isMainThread());
    registerBlobResourceHandleConstructor();

    auto blobData = BlobData::create(contentType);

    // The blob data is stored in the "canonical" way. That is, it only contains a list of Data and File items.
    // 1) The Data item is denoted by the raw data and the range.
    // 2) The File item is denoted by the file path, the range and the expected modification time.
    // 3) The URL item is denoted by the URL, the range and the expected modification time.
    // All the Blob items in the passing blob data are resolved and expanded into a set of Data and File items.

    for (BlobPart& part : blobParts) {
        switch (part.type()) {
        case BlobPart::Type::Data: {
            auto movedData = part.moveData();
            auto data = ThreadSafeDataBuffer::create(WTFMove(movedData));
            blobData->appendData(data);
            break;
        }
        case BlobPart::Type::Blob: {
            if (auto blob = m_blobs.get(part.url().string())) {
                for (const BlobDataItem& item : blob->items())
                    blobData->m_items.append(item);
            }
            break;
        }
        }
    }

    addBlobData(url.string(), WTFMove(blobData));
}

void BlobRegistryImpl::registerBlobURL(const URL& url, const URL& srcURL, const PolicyContainer& policyContainer)
{
    registerBlobURLOptionallyFileBacked(url, srcURL, nullptr, { }, policyContainer);
}

void BlobRegistryImpl::registerBlobURLOptionallyFileBacked(const URL& url, const URL& srcURL, RefPtr<BlobDataFileReference>&& file, const String& contentType, const PolicyContainer& policyContainer)
{
    ASSERT(isMainThread());
    registerBlobResourceHandleConstructor();

    BlobData* src = getBlobDataFromURL(srcURL);
    if (src) {
        if (src->policyContainer() == policyContainer)
            addBlobData(url.string(), src);
        else {
            auto clone = src->clone();
            clone->setPolicyContainer(policyContainer);
            addBlobData(url.string(), WTFMove(clone));
        }
        return;
    }

    if (!file || file->path().isEmpty())
        return;

    auto backingFile = BlobData::create(contentType);
    backingFile->appendFile(file.releaseNonNull());
    backingFile->setPolicyContainer(policyContainer);

    addBlobData(url.string(), WTFMove(backingFile));
}

void BlobRegistryImpl::registerBlobURLForSlice(const URL& url, const URL& srcURL, long long start, long long end, const String& contentType)
{
    ASSERT(isMainThread());
    BlobData* originalData = getBlobDataFromURL(srcURL);
    if (!originalData)
        return;

    unsigned long long originalSize = blobSize(srcURL);

    // Convert the negative value that is used to select from the end.
    if (start < 0)
        start = start + originalSize;
    if (end < 0)
        end = end + originalSize;

    // Clamp the range if it exceeds the size limit.
    if (start < 0)
        start = 0;
    if (end < 0)
        end = 0;
    if (static_cast<unsigned long long>(start) >= originalSize) {
        start = 0;
        end = 0;
    } else if (end < start)
        end = start;
    else if (static_cast<unsigned long long>(end) > originalSize)
        end = originalSize;

    unsigned long long newLength = end - start;
    auto newData = BlobData::create(contentType.isEmpty() ? originalData->contentType() : contentType);

    appendStorageItems(newData.ptr(), originalData->items(), start, newLength);

    addBlobData(url.string(), WTFMove(newData));
}

void BlobRegistryImpl::unregisterBlobURL(const URL& url)
{
    ASSERT(isMainThread());
    if (m_blobReferences.remove(url.string()))
        m_blobs.remove(url.string());
}

BlobData* BlobRegistryImpl::getBlobDataFromURL(const URL& url) const
{
    ASSERT(isMainThread());
    if (url.hasFragmentIdentifier())
        return m_blobs.get(url.stringWithoutFragmentIdentifier().toStringWithoutCopying());
    return m_blobs.get(url.string());
}

unsigned long long BlobRegistryImpl::blobSize(const URL& url)
{
    ASSERT(isMainThread());
    BlobData* data = getBlobDataFromURL(url);
    if (!data)
        return 0;

    unsigned long long result = 0;
    for (const BlobDataItem& item : data->items())
        result += item.length();

    return result;
}

static WorkQueue& blobUtilityQueue()
{
    static auto& queue = WorkQueue::create("org.webkit.BlobUtility", WorkQueue::QOS::Utility).leakRef();
    return queue;
}

bool BlobRegistryImpl::populateBlobsForFileWriting(const Vector<String>& blobURLs, Vector<BlobForFileWriting>& blobsForWriting)
{
    for (auto& url : blobURLs) {
        blobsForWriting.append({ });
        blobsForWriting.last().blobURL = url.isolatedCopy();

        auto* blobData = getBlobDataFromURL({ { }, url });
        if (!blobData)
            return false;

        for (auto& item : blobData->items()) {
            switch (item.type()) {
            case BlobDataItem::Type::Data:
                blobsForWriting.last().filePathsOrDataBuffers.append({ { }, item.data() });
                break;
            case BlobDataItem::Type::File:
                blobsForWriting.last().filePathsOrDataBuffers.append({ item.file()->path().isolatedCopy(), { } });
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }
    }
    return true;
}

static bool writeFilePathsOrDataBuffersToFile(const Vector<std::pair<String, ThreadSafeDataBuffer>>& filePathsOrDataBuffers, FileSystem::PlatformFileHandle file, const String& path)
{
    auto fileCloser = makeScopeExit([file]() mutable {
        FileSystem::closeFile(file);
    });

    if (path.isEmpty() || !FileSystem::isHandleValid(file)) {
        LOG_ERROR("Failed to open temporary file for writing a Blob");
        return false;
    }

    for (auto& part : filePathsOrDataBuffers) {
        if (part.second.data()) {
            int length = part.second.data()->size();
            if (FileSystem::writeToFile(file, part.second.data()->data(), length) != length) {
                LOG_ERROR("Failed writing a Blob to temporary file");
                return false;
            }
        } else {
            ASSERT(!part.first.isEmpty());
            if (!FileSystem::appendFileContentsToFileHandle(part.first, file)) {
                LOG_ERROR("Failed copying File contents to a Blob temporary file (%s to %s)", part.first.utf8().data(), path.utf8().data());
                return false;
            }
        }
    }
    return true;
}

void BlobRegistryImpl::writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&& completionHandler)
{
    Vector<BlobForFileWriting> blobsForWriting;
    if (!populateBlobsForFileWriting(blobURLs, blobsForWriting)) {
        completionHandler({ });
        return;
    }

    blobUtilityQueue().dispatch([blobsForWriting = WTFMove(blobsForWriting), completionHandler = WTFMove(completionHandler)]() mutable {
        Vector<String> filePaths;
        for (auto& blob : blobsForWriting) {
            FileSystem::PlatformFileHandle file;
            String tempFilePath = FileSystem::openTemporaryFile("Blob"_s, file);
            if (!writeFilePathsOrDataBuffersToFile(blob.filePathsOrDataBuffers, file, tempFilePath)) {
                filePaths.clear();
                break;
            }
            filePaths.append(tempFilePath.isolatedCopy());
        }

        callOnMainThread([completionHandler = WTFMove(completionHandler), filePaths = WTFMove(filePaths)] () mutable {
            completionHandler(WTFMove(filePaths));
        });
    });
}

void BlobRegistryImpl::writeBlobToFilePath(const URL& blobURL, const String& path, Function<void(bool success)>&& completionHandler)
{
    Vector<BlobForFileWriting> blobsForWriting;
    if (!populateBlobsForFileWriting({ blobURL.string() }, blobsForWriting) || blobsForWriting.size() != 1) {
        completionHandler(false);
        return;
    }

    blobUtilityQueue().dispatch([path, blobsForWriting = WTFMove(blobsForWriting), completionHandler = WTFMove(completionHandler)]() mutable {
        bool success = writeFilePathsOrDataBuffersToFile(blobsForWriting.first().filePathsOrDataBuffers, FileSystem::openFile(path, FileSystem::FileOpenMode::Write), path);
        callOnMainThread([success, completionHandler = WTFMove(completionHandler)]() {
            completionHandler(success);
        });
    });
}

Vector<RefPtr<BlobDataFileReference>> BlobRegistryImpl::filesInBlob(const URL& url) const
{
    auto* blobData = getBlobDataFromURL(url);
    if (!blobData)
        return { };

    Vector<RefPtr<BlobDataFileReference>> result;
    for (const BlobDataItem& item : blobData->items()) {
        if (item.type() == BlobDataItem::Type::File)
            result.append(item.file());
    }

    return result;
}

void BlobRegistryImpl::addBlobData(const String& url, RefPtr<BlobData>&& blobData)
{
    auto addResult = m_blobs.set(url, WTFMove(blobData));
    if (addResult.isNewEntry)
        m_blobReferences.add(url);
}

void BlobRegistryImpl::registerBlobURLHandle(const URL& url)
{
    if (m_blobs.contains(url.string()))
        m_blobReferences.add(url.string());
}

void BlobRegistryImpl::unregisterBlobURLHandle(const URL& url)
{
    if (m_blobReferences.remove(url.string()))
        m_blobs.remove(url.string());
}

} // namespace WebCore
