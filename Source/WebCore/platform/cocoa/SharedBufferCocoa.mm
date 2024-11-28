/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
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

#import "WebCoreJITOperations.h"
#import "WebCoreObjCExtras.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <pal/cf/CoreMediaSoftLink.h>
#import <string.h>
#import <wtf/MainThread.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

@interface WebCoreSharedBufferData : NSData
- (instancetype)initWithDataSegment:(const WebCore::DataSegment&)dataSegment position:(NSUInteger)position size:(NSUInteger)size;
@end

@implementation WebCoreSharedBufferData {
    RefPtr<const WebCore::DataSegment> _dataSegment;
    NSUInteger _position;
    NSUInteger _size;
}

+ (void)initialize
{
#if !USE(WEB_THREAD)
    JSC::initialize();
    WTF::initializeMainThread();
    WebCore::populateJITOperations();
#endif // !USE(WEB_THREAD)
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebCoreSharedBufferData class], self))
        return;

    [super dealloc];
}

- (instancetype)initWithDataSegment:(const WebCore::DataSegment&)dataSegment position:(NSUInteger)position size:(NSUInteger)size
{
    if (!(self = [super init]))
        return nil;

    RELEASE_ASSERT(position <= dataSegment.size());
    RELEASE_ASSERT(size <= dataSegment.size() - position);
    _dataSegment = &dataSegment;
    _position = position;
    _size = size;
    return self;
}

- (NSUInteger)length
{
    return _size;
}

- (const void *)bytes
{
    return _dataSegment->span().subspan(_position).data();
}

@end

namespace WebCore {

Ref<FragmentedSharedBuffer> FragmentedSharedBuffer::create(NSData *data)
{
    return adoptRef(*new FragmentedSharedBuffer(bridge_cast(data)));
}

void FragmentedSharedBuffer::append(NSData *data)
{
    ASSERT(!m_contiguous);
    return append(bridge_cast(data));
}

static void FreeDataSegment(void* refcon, void*, size_t)
{
    auto* buffer = reinterpret_cast<const DataSegment*>(refcon);
    buffer->deref();
}

RetainPtr<CMBlockBufferRef> FragmentedSharedBuffer::createCMBlockBuffer() const
{
    auto segmentToCMBlockBuffer = [] (const DataSegment& segment) -> RetainPtr<CMBlockBufferRef> {
        // From CMBlockBufferCustomBlockSource documentation:
        // Note that for 64-bit architectures, this struct contains misaligned function pointers.
        // To avoid link-time issues, it is recommended that clients fill CMBlockBufferCustomBlockSource's function pointer fields
        // by using assignment statements, rather than declaring them as global or static structs.
        CMBlockBufferCustomBlockSource allocator;
        allocator.version = 0;
        allocator.AllocateBlock = nullptr;
        allocator.FreeBlock = FreeDataSegment;
        allocator.refCon = const_cast<DataSegment*>(&segment);
        segment.ref();
        CMBlockBufferRef partialBuffer = nullptr;
        if (PAL::CMBlockBufferCreateWithMemoryBlock(nullptr, const_cast<uint8_t*>(segment.span().data()), segment.size(), nullptr, &allocator, 0, segment.size(), 0, &partialBuffer) != kCMBlockBufferNoErr)
            return nullptr;
        return adoptCF(partialBuffer);
    };

    if (hasOneSegment() && !isEmpty())
        return segmentToCMBlockBuffer(m_segments[0].segment);

    CMBlockBufferRef rawBlockBuffer = nullptr;
    auto err = PAL::CMBlockBufferCreateEmpty(kCFAllocatorDefault, isEmpty() ? 0 : m_segments.size(), 0, &rawBlockBuffer);
    if (err != kCMBlockBufferNoErr || !rawBlockBuffer)
        return nullptr;
    auto blockBuffer = adoptCF(rawBlockBuffer);

    if (isEmpty())
        return blockBuffer;

    for (auto& segment : m_segments) {
        if (!segment.segment->size())
            continue;
        auto partialBuffer = segmentToCMBlockBuffer(segment.segment);
        if (!partialBuffer)
            return nullptr;
        if (PAL::CMBlockBufferAppendBufferReference(rawBlockBuffer, partialBuffer.get(), 0, 0, 0) != kCMBlockBufferNoErr)
            return nullptr;
    }
    return blockBuffer;
}

RetainPtr<NSData> SharedBuffer::createNSData() const
{
    return bridge_cast(createCFData());
}

RetainPtr<CFDataRef> SharedBuffer::createCFData() const
{
    if (!m_segments.size())
        return adoptCF(CFDataCreate(nullptr, nullptr, 0));
    return bridge_cast(m_segments[0].segment->createNSData());
}

RetainPtr<NSArray> FragmentedSharedBuffer::createNSDataArray() const
{
    return createNSArray(m_segments, [] (auto& segment) {
        return segment.segment->createNSData();
    });
}

RetainPtr<NSData> DataSegment::createNSData() const
{
    return adoptNS([[WebCoreSharedBufferData alloc] initWithDataSegment:*this position:0 size:size()]);
}

void DataSegment::iterate(CFDataRef data, const Function<void(std::span<const uint8_t>)>& apply) const
{
    [(__bridge NSData *)data enumerateByteRangesUsingBlock:^(const void *bytes, NSRange byteRange, BOOL *) {
        apply({ static_cast<const uint8_t*>(bytes), byteRange.length });
    }];
}

RetainPtr<NSData> SharedBufferDataView::createNSData() const
{
    return adoptNS([[WebCoreSharedBufferData alloc] initWithDataSegment:m_segment.get() position:m_positionWithinSegment size:size()]);
}

} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
