/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "SharedBuffer.h"

#include "WebCoreObjCExtras.h"
#include <runtime/InitializeThreading.h>
#include <string.h>
#include <wtf/MainThread.h>
#include <wtf/PassRefPtr.h>

using namespace WebCore;

@interface WebCoreSharedBufferData : NSData
{
    RefPtr<SharedBuffer::DataBuffer> sharedBufferDataBuffer;
#if ENABLE(DISK_IMAGE_CACHE)
    RefPtr<SharedBuffer> sharedBuffer;
#endif
}

- (id)initWithSharedBufferDataBuffer:(SharedBuffer::DataBuffer*)dataBuffer;
#if ENABLE(DISK_IMAGE_CACHE)
- (id)initWithMemoryMappedSharedBuffer:(SharedBuffer&)memoryMappedSharedBuffer;
#endif
@end

@implementation WebCoreSharedBufferData

+ (void)initialize
{
#if !USE(WEB_THREAD)
    JSC::initializeThreading();
    WTF::initializeMainThreadToProcessMainThread();
#endif // !USE(WEB_THREAD)
    WebCoreObjCFinalizeOnMainThread(self);
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebCoreSharedBufferData class], self))
        return;

    [super dealloc];
}

- (void)finalize
{
    [super finalize];
}

- (id)initWithSharedBufferDataBuffer:(SharedBuffer::DataBuffer*)dataBuffer
{
    self = [super init];
    
    if (self)
        sharedBufferDataBuffer = dataBuffer;

    return self;
}

#if ENABLE(DISK_IMAGE_CACHE)
- (id)initWithMemoryMappedSharedBuffer:(SharedBuffer&)memoryMappedSharedBuffer
{
    ASSERT(memoryMappedSharedBuffer.isMemoryMapped());
    self = [super init];

    if (!self)
        return nil;

    sharedBuffer = &memoryMappedSharedBuffer;
    return self;
}
#endif

- (NSUInteger)length
{
#if ENABLE(DISK_IMAGE_CACHE)
    if (sharedBuffer)
        return sharedBuffer->size();
#endif
    return sharedBufferDataBuffer->data.size();
}

- (const void *)bytes
{
#if ENABLE(DISK_IMAGE_CACHE)
    if (sharedBuffer)
        return sharedBuffer->data();
#endif
    return sharedBufferDataBuffer->data.data();
}

@end

namespace WebCore {

PassRefPtr<SharedBuffer> SharedBuffer::wrapNSData(NSData *nsData)
{
    return adoptRef(new SharedBuffer((CFDataRef)nsData));
}

RetainPtr<NSData> SharedBuffer::createNSData()
{
    return adoptNS((NSData *)createCFData().leakRef());
}

CFDataRef SharedBuffer::existingCFData()
{
    if (m_cfData)
        return m_cfData.get();

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    if (m_dataArray.size() == 1)
        return m_dataArray.at(0).get();
#endif

    return nullptr;
}

RetainPtr<CFDataRef> SharedBuffer::createCFData()
{
    if (CFDataRef cfData = existingCFData())
        return cfData;

#if ENABLE(DISK_IMAGE_CACHE)
    if (isMemoryMapped())
        return adoptCF((CFDataRef)adoptNS([[WebCoreSharedBufferData alloc] initWithMemoryMappedSharedBuffer:*this]).leakRef());
#endif

    data(); // Force data into m_buffer from segments or data array.
    if (hasPurgeableBuffer()) {
        RefPtr<SharedBuffer::DataBuffer> copiedBuffer = adoptRef(new DataBuffer);
        copiedBuffer->data.append(data(), size());
        return adoptCF((CFDataRef)adoptNS([[WebCoreSharedBufferData alloc] initWithSharedBufferDataBuffer:copiedBuffer.get()]).leakRef());
    }

    return adoptCF((CFDataRef)adoptNS([[WebCoreSharedBufferData alloc] initWithSharedBufferDataBuffer:m_buffer.get()]).leakRef());
}

PassRefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String& filePath)
{
    NSData *resourceData = [NSData dataWithContentsOfFile:filePath];
    if (resourceData) 
        return SharedBuffer::wrapNSData(resourceData);
    return 0;
}

}
