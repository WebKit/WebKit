/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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
#include <JavaScriptCore/InitializeThreading.h>
#include <string.h>
#include <wtf/MainThread.h>

@interface WebCoreSharedBufferData : NSData
{
    RefPtr<const WebCore::SharedBuffer::DataSegment> sharedBufferDataSegment;
}

- (id)initWithSharedBufferDataSegment:(const WebCore::SharedBuffer::DataSegment&)dataSegment;
@end

@implementation WebCoreSharedBufferData

+ (void)initialize
{
#if !USE(WEB_THREAD)
    JSC::initializeThreading();
    WTF::initializeMainThreadToProcessMainThread();
#endif // !USE(WEB_THREAD)
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebCoreSharedBufferData class], self))
        return;

    [super dealloc];
}

- (id)initWithSharedBufferDataSegment:(const WebCore::SharedBuffer::DataSegment&)dataSegment
{
    self = [super init];
    
    if (self)
        sharedBufferDataSegment = &dataSegment;

    return self;
}

- (NSUInteger)length
{
    return sharedBufferDataSegment->size();
}

- (const void *)bytes
{
    return sharedBufferDataSegment->data();
}

@end

namespace WebCore {

Ref<SharedBuffer> SharedBuffer::create(NSData *nsData)
{
    return adoptRef(*new SharedBuffer((__bridge CFDataRef)nsData));
}

void SharedBuffer::append(NSData *nsData)
{
    return append((__bridge CFDataRef)nsData);
}

RetainPtr<NSData> SharedBuffer::createNSData() const
{
    return adoptNS((NSData *)createCFData().leakRef());
}

RetainPtr<CFDataRef> SharedBuffer::createCFData() const
{
    combineIntoOneSegment();
    if (!m_segments.size())
        return adoptCF(CFDataCreate(nullptr, nullptr, 0));
    ASSERT(m_segments.size() == 1);
    return adoptCF((CFDataRef)adoptNS([[WebCoreSharedBufferData alloc] initWithSharedBufferDataSegment:m_segments[0].segment]).leakRef());
}

RefPtr<SharedBuffer> SharedBuffer::createFromReadingFile(const String& filePath)
{
    NSData *resourceData = [NSData dataWithContentsOfFile:filePath];
    if (resourceData) 
        return SharedBuffer::create(resourceData);
    return nullptr;
}

RetainPtr<NSArray> SharedBuffer::createNSDataArray() const
{
    auto dataArray = adoptNS([[NSMutableArray alloc] initWithCapacity:m_segments.size()]);
    for (const auto& segment : m_segments)
        [dataArray addObject:adoptNS([[WebCoreSharedBufferData alloc] initWithSharedBufferDataSegment:segment.segment]).get()];
    return WTFMove(dataArray);
}

}
