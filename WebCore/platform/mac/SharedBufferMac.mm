/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "FoundationExtras.h"
#include "SharedBuffer.h"
#include "WebCoreObjCExtras.h"
#include <string.h>
#include <wtf/PassRefPtr.h>

using namespace WebCore;

@interface WebCoreSharedBufferData : NSData
{
    SharedBuffer* sharedBuffer;
}

- (id)initWithSharedBuffer:(SharedBuffer*)buffer;
@end

@implementation WebCoreSharedBufferData

#ifndef BUILDING_ON_TIGER
+ (void)initialize
{
    WebCoreObjCFinalizeOnMainThread(self);
}
#endif

- (void)dealloc
{
    sharedBuffer->deref();
    
    [super dealloc];
}

- (void)finalize
{
    sharedBuffer->deref();
    
    [super finalize];
}

- (id)initWithSharedBuffer:(SharedBuffer*)buffer
{
    self = [super init];
    
    if (self) {
        sharedBuffer = buffer;
        sharedBuffer->ref();
    }
    
    return self;
}

- (unsigned)length
{
    return sharedBuffer->size();
}

- (const void *)bytes
{
    return reinterpret_cast<const void*>(sharedBuffer->data());
}

@end

namespace WebCore {

PassRefPtr<SharedBuffer> SharedBuffer::wrapNSData(NSData *nsData)
{
    return new SharedBuffer(nsData);
}

SharedBuffer::SharedBuffer(NSData *nsData)
    : m_nsData(nsData)
{
}

NSData *SharedBuffer::createNSData()
{    
    return [[WebCoreSharedBufferData alloc] initWithSharedBuffer:this];
}

CFDataRef SharedBuffer::createCFData()
{    
    return (CFDataRef)HardRetainWithNSRelease([[WebCoreSharedBufferData alloc] initWithSharedBuffer:this]);
}

bool SharedBuffer::hasPlatformData() const
{
    return m_nsData;
}

const char* SharedBuffer::platformData() const
{
    return (const char*)[m_nsData.get() bytes];
}

unsigned SharedBuffer::platformDataSize() const
{
    return [m_nsData.get() length];
}

void SharedBuffer::maybeTransferPlatformData()
{
    if (!m_nsData)
        return;
    
    ASSERT(m_buffer.size() == 0);
        
    m_buffer.append(reinterpret_cast<const char*>([m_nsData.get() bytes]), [m_nsData.get() length]);
        
    m_nsData = nil;
}

void SharedBuffer::clearPlatformData()
{
    m_nsData = 0;
}

PassRefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String& filePath)
{
    NSData *resourceData = [NSData dataWithContentsOfFile:filePath];
    if (resourceData) 
        return SharedBuffer::wrapNSData(resourceData);
    return 0;
}

}

