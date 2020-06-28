/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "SharedBuffer.h"

#import "WebCoreObjCExtras.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <string.h>
#import <wtf/MainThread.h>
#import <wtf/cocoa/VectorCocoa.h>

@interface WebCoreSharedBufferData : NSData
- (instancetype)initWithDataSegment:(const WebCore::SharedBuffer::DataSegment&)dataSegment position:(NSUInteger)position;
@end

@implementation WebCoreSharedBufferData {
    RefPtr<const WebCore::SharedBuffer::DataSegment> _dataSegment;
    NSUInteger _position;
}

+ (void)initialize
{
#if !USE(WEB_THREAD)
    JSC::initialize();
    WTF::initializeMainThread();
#endif // !USE(WEB_THREAD)
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebCoreSharedBufferData class], self))
        return;

    [super dealloc];
}

- (instancetype)initWithDataSegment:(const WebCore::SharedBuffer::DataSegment&)dataSegment position:(NSUInteger)position
{
    if (!(self = [super init]))
        return nil;

    RELEASE_ASSERT(!position || position < dataSegment.size());
    _dataSegment = &dataSegment;
    _position = position;
    return self;
}

- (NSUInteger)length
{
    return _dataSegment->size() - _position;
}

- (const void *)bytes
{
    return _dataSegment->data() + _position;
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
    return adoptCF((__bridge CFDataRef)m_segments[0].segment->createNSData().leakRef());
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
    return createNSArray(m_segments, [] (auto& segment) {
        return segment.segment->createNSData();
    });
}

RetainPtr<NSData> SharedBuffer::DataSegment::createNSData() const
{
    return adoptNS([[WebCoreSharedBufferData alloc] initWithDataSegment:*this position:0]);
}

RetainPtr<NSData> SharedBufferDataView::createNSData() const
{
    return adoptNS([[WebCoreSharedBufferData alloc] initWithDataSegment:m_segment.get() position:m_positionWithinSegment]);
}

} // namespace WebCore
