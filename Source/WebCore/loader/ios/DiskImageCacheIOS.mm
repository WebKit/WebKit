/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DiskImageCacheIOS.h"

#if ENABLE(DISK_IMAGE_CACHE) && PLATFORM(IOS)

#include "FileSystemIOS.h"
#include "Logging.h"
#include "WebCoreThread.h"
#include "WebCoreThreadRun.h"
#include <errno.h>
#include <sys/mman.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

DiskImageCache& diskImageCache()
{
    static NeverDestroyed<DiskImageCache> cache;
    return cache;
}

DiskImageCache::Entry::Entry(SharedBuffer* buffer, DiskImageCacheId id)
    : m_buffer(buffer)
    , m_id(id)
    , m_size(0)
    , m_mapping(nullptr)
{
    ASSERT(WebThreadIsCurrent());
    ASSERT(WebThreadIsLocked());
    ASSERT(m_buffer);
    m_buffer->ref();
}

DiskImageCache::Entry::~Entry()
{
    ASSERT(WebThreadIsCurrent());
    ASSERT(WebThreadIsLocked());
    ASSERT(!m_buffer);
    ASSERT(!m_mapping);
}

bool DiskImageCache::Entry::mapInternal(const String& path)
{
    ASSERT(m_buffer);
    ASSERT(!m_mapping);

    m_path = path;
    m_size = m_buffer->size();

    // Open the file for reading and writing.
    PlatformFileHandle handle = open(m_path.utf8().data(), O_CREAT | O_RDWR | O_TRUNC, static_cast<mode_t>(0600));
    if (!isHandleValid(handle))
        return false;

    // Write the data to the file.
    if (writeToFileInternal(handle) == -1) {
        closeFile(handle);
        deleteFile(m_path);
        return false;
    }

    // Seek back to the beginning.
    if (seekFile(handle, 0, SeekFromBeginning) == -1) {
        closeFile(handle);
        deleteFile(m_path);
        return false;
    }

    // Perform memory mapping for reading.
    // NOTE: This must not conflict with the open() above, which must also open for reading.
    m_mapping = mmap(nullptr, m_size, PROT_READ, MAP_SHARED, handle, 0);
    closeFile(handle);
    if (m_mapping == MAP_FAILED) {
        LOG(DiskImageCache, "DiskImageCache: mapping failed (%d): (%s)", errno, strerror(errno));
        m_mapping = nullptr;
        deleteFile(m_path);
        return false;
    }

    return true;
}

void DiskImageCache::Entry::map(const String& path)
{
    ASSERT(m_buffer);
    ASSERT(!m_mapping);
    DiskImageCache::Entry* thisEntry = this;

    bool fileMapped = mapInternal(path);
    if (!fileMapped) {
        // Notify the buffer in the case of a failed mapping.
        WebThreadRun(^{
            m_buffer->failedMemoryMap();
            m_buffer->deref();
            m_buffer = 0;
            thisEntry->deref();
        });
        return;
    }

    // Notify the buffer in the case of a successful mapping.
    // This should happen on the WebThread because this is being run
    // asynchronously inside a dispatch queue.
    WebThreadRun(^{
        m_buffer->markAsMemoryMapped();
        m_buffer->deref();
        m_buffer = 0;
        thisEntry->deref();
    });
}

void DiskImageCache::Entry::unmap()
{
    if (!m_mapping) {
        ASSERT(!m_size);
        return;
    }

    if (munmap(m_mapping, m_size) == -1)
        LOG_ERROR("DiskImageCache: Could not munmap a memory mapped file with id (%d)", m_id);

    m_mapping = nullptr;
    m_size = 0;
}

void DiskImageCache::Entry::removeFile()
{
    ASSERT(!m_mapping);
    ASSERT(!m_size);

    if (!deleteFile(m_path))
        LOG_ERROR("DiskImageCache: Could not delete memory mapped file (%s)", m_path.utf8().data());
}

void DiskImageCache::Entry::clearDataWithoutMapping()
{
    ASSERT(!m_mapping);
    ASSERT(m_buffer);
    m_buffer->deref();
    m_buffer = 0;
}

int DiskImageCache::Entry::writeToFileInternal(PlatformFileHandle handle)
{
    ASSERT(m_buffer);
    int totalBytesWritten = 0;

    const char* segment = nullptr;
    unsigned position = 0;
    while (unsigned length = m_buffer->getSomeData(segment, position)) {
        int bytesWritten = writeToFile(handle, segment, length);
        if (bytesWritten == -1)
            return -1;

        totalBytesWritten += bytesWritten;
        position += length;
    }

    return totalBytesWritten;
}


DiskImageCache::DiskImageCache()
    : m_enabled(false)
    , m_size(0)
    , m_maximumCacheSize(100 * 1024 * 1024)
    , m_minimumImageSize(100 * 1024)
    , m_nextAvailableId(DiskImageCache::invalidDiskCacheId + 1)
{
}

DiskImageCacheId DiskImageCache::writeItem(PassRefPtr<SharedBuffer> item)
{
    if (!isEnabled() || !createDirectoryIfNeeded())
        return DiskImageCache::invalidDiskCacheId;

    // We are already full, cannot add anything until something is removed.
    if (isFull()) {
        LOG(DiskImageCache, "DiskImageCache: could not process an item because the cache was full at (%d). The \"max\" being (%d)", m_size, m_maximumCacheSize);
        return DiskImageCache::invalidDiskCacheId;
    }

    // Create an entry.
    DiskImageCacheId id = nextAvailableId();
    RefPtr<SharedBuffer> buffer = item;
    RefPtr<DiskImageCache::Entry> entry = DiskImageCache::Entry::create(buffer.get(), id);
    m_table.add(id, entry);

    // Create a temporary file path.
    String path = temporaryFile();
    LOG(DiskImageCache, "DiskImageCache: creating entry (%d) at (%s)", id, path.utf8().data());
    if (path.isNull())
        return DiskImageCache::invalidDiskCacheId;

    // The lifetime of the Entry is handled on the WebThread.
    // Before we send to a dispatch queue we need to ref so
    // that we are sure the object still exists. This call
    // is balanced in the WebThreadRun inside of Entry::map.
    // or the early return in this dispatch.
    DiskImageCache::Entry* localEntryForBlock = entry.get();
    localEntryForBlock->ref();

    // Map to disk asynchronously.
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        // The cache became full since the time we were added to the queue. Don't map.
        if (diskImageCache().isFull()) {
            WebThreadRun(^{
                localEntryForBlock->clearDataWithoutMapping();
                localEntryForBlock->deref();
            });
            return;
        }

        localEntryForBlock->map(path);

        // Update the size on a successful mapping.
        if (localEntryForBlock->isMapped())
            diskImageCache().updateSize(localEntryForBlock->size());
    });

    return id;
}

void DiskImageCache::updateSize(unsigned delta)
{
    MutexLocker lock(m_mutex);
    m_size += delta;
}

void DiskImageCache::removeItem(DiskImageCacheId id)
{
    LOG(DiskImageCache, "DiskImageCache: removeItem (%d)", id);
    RefPtr<DiskImageCache::Entry> entry = m_table.get(id);
    m_table.remove(id);
    if (!entry->isMapped())
        return;

    updateSize(-(entry->size()));

    DiskImageCache::Entry *localEntryForBlock = entry.get();
    localEntryForBlock->ref();
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        localEntryForBlock->unmap();
        localEntryForBlock->removeFile();
        WebThreadRun(^{ localEntryForBlock->deref(); });
    });
}

void* DiskImageCache::dataForItem(DiskImageCacheId id)
{
    ASSERT(id);

    RefPtr<DiskImageCache::Entry> entry = m_table.get(id);
    ASSERT(entry->isMapped());
    return entry->data();
}

bool DiskImageCache::createDirectoryIfNeeded()
{
    if (!m_cacheDirectory.isNull())
        return true;

    m_cacheDirectory = temporaryDirectory();
    LOG(DiskImageCache, "DiskImageCache: Created temporary directory (%s)", m_cacheDirectory.utf8().data());
    if (m_cacheDirectory.isNull()) {
        LOG_ERROR("DiskImageCache: could not create cache directory");
        return false;
    }

    if (m_client)
        m_client->didCreateDiskImageCacheDirectory(m_cacheDirectory);

    return true;
}

DiskImageCacheId DiskImageCache::nextAvailableId()
{
    return m_nextAvailableId++;
}

String DiskImageCache::temporaryDirectory()
{
    NSString *tempDiskCacheDirectory = createTemporaryDirectory(@"DiskImageCache");
    if (!tempDiskCacheDirectory)
        LOG_ERROR("DiskImageCache: Could not create a temporary directory.");

    return tempDiskCacheDirectory;
}

String DiskImageCache::temporaryFile()
{
    NSString *tempFile = createTemporaryFile(m_cacheDirectory, @"tmp");
    if (!tempFile)
        LOG_ERROR("DiskImageCache: Could not create a temporary file.");

    return tempFile;
}

} // namespace WebCore

#endif // ENABLE(DISK_IMAGE_CACHE) && PLATFORM(IOS)
