/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKJSScriptingBufferInternal.h"

#import "NetworkCacheData.h"
#import "WKNSData.h"
#import "_WKJSBuffer.h"
#import <WebCore/SharedBuffer.h>
#import <WebCore/SharedMemory.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/FileSystem.h>
#import <wtf/Scope.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/spi/cocoa/MachVMSPI.h>

static bool isInReadOnlyRegion(std::span<const uint8_t> span)
{
    if (span.empty())
        return false;

    auto addr = reinterpret_cast<mach_vm_address_t>(const_cast<uint8_t*>(span.data()));
    auto end = addr + span.size();
    auto regionAddr = addr;
    mach_vm_size_t regionSize = 0;
    vm_region_basic_info_data_64_t info { };
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t vmObject = MACH_PORT_NULL;
    auto scopeExit = makeScopeExit([&] {
        if (vmObject != MACH_PORT_NULL)
            mach_port_deallocate(mach_task_self(), vmObject);
    });

    auto kr = mach_vm_region(mach_task_self(), &regionAddr, &regionSize, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &count, &vmObject);
    if (kr != KERN_SUCCESS)
        return false;

    auto regionEnd = regionAddr + regionSize;
    return (info.protection & VM_PROT_READ) && !(info.protection & VM_PROT_WRITE) && addr >= regionAddr && end <= regionEnd;
}

@implementation WKJSScriptingBuffer

- (instancetype)initWithData:(NSData *)data
{
    if (!(self = [super init]))
        return nil;

    RefPtr<WebCore::SharedMemory> sharedMemory;
    auto dataSpan = span(data);

    if (isInReadOnlyRegion(dataSpan))
        sharedMemory = WebCore::SharedMemory::wrapMap(dataSpan, WebCore::SharedMemoryProtection::ReadOnly);

    if (!sharedMemory) {
        Ref sharedBuffer = WebCore::SharedBuffer::create(data);
        sharedMemory = WebCore::SharedMemory::copyBuffer(sharedBuffer);
    }

    if (!sharedMemory) {
        [self release];
        return nil;
    }

    API::Object::constructInWrapper<API::JSBuffer>(self, sharedMemory.releaseNonNull());

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKJSScriptingBuffer.class, self))
        return;
    SUPPRESS_UNRETAINED_ARG _buffer->API::JSBuffer::~JSBuffer();
    [super dealloc];
}

- (API::Object&)_apiObject
{
    return *_buffer;
}

@end

@implementation _WKJSBuffer
@end
