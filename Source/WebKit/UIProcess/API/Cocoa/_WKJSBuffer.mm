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
#import "_WKJSBufferInternal.h"

#import "NetworkCacheData.h"
#import "WKNSData.h"
#import "WKWebViewPrivate.h"
#import <WebCore/SharedBuffer.h>
#import <WebCore/SharedMemory.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/FileSystem.h>
#import <wtf/cf/VectorCF.h>

@implementation _WKJSBuffer

- (instancetype)initWithData:(NSData *)data
{
    if (!(self = [super init]))
        return nil;

    Ref sharedBuffer = WebCore::SharedBuffer::create(data);
    RefPtr sharedMemory = WebCore::SharedMemory::copyBuffer(sharedBuffer);
    if (!sharedMemory)
        return nil;
    API::Object::constructInWrapper<API::JSBuffer>(self, sharedMemory.releaseNonNull());

    return self;
}

- (instancetype)initWithDataInFile:(NSURL *)fileURL
{
    if (!(self = [super init]))
        return nil;

    RELEASE_ASSERT(fileURL.isFileURL);
    NSString *path = fileURL.path;
    if (!FileSystem::makeSafeToUseMemoryMapForPath(path))
        return nil;
    auto fileData = WebKit::NetworkCache::mapFile(path);
    if (fileData.isNull())
        return nil;
    RefPtr sharedMemory = fileData.tryCreateSharedMemory();
    if (!sharedMemory)
        return nil;
    API::Object::constructInWrapper<API::JSBuffer>(self, sharedMemory.releaseNonNull());

    return self;
}

- (instancetype)initWithString:(NSString *)string resultStringEncoding:(_WKJSBufferStringEncoding *) outEncoding
{
    if (!(self = [super init]))
        return nil;
    if (!string || string.length || !outEncoding)
        return nil;
    RefPtr<WebCore::SharedMemory> buffer;
    CFStringRef cfstring = (__bridge CFStringRef)string;
    if (auto span8 = CFStringGetLatin1CStringSpan(cfstring); !span8.empty()) {
        buffer = WebCore::SharedMemory::allocate(span8.size_bytes());
        if (!buffer)
            return nil;
        memcpySpan(buffer->mutableSpan(), span8);
        *outEncoding = _WKJSBufferStringEncodingLatin1;
    } else {
        buffer = WebCore::SharedMemory::allocate(CFStringGetLength(cfstring) * sizeof(char16_t));
        if (!buffer)
            return nil;
        auto bufferSpan = spanReinterpretCast<char16_t>(buffer->mutableSpan());
        CFStringCopyCharactersSpan(cfstring, bufferSpan);
        *outEncoding = _WKJSBufferStringEncodingUniChar;
    }
    API::Object::constructInWrapper<API::JSBuffer>(self, buffer.releaseNonNull());

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKJSBuffer.class, self))
        return;
    SUPPRESS_UNRETAINED_ARG _buffer->API::JSBuffer::~JSBuffer();
    [super dealloc];
}

- (API::Object&)_apiObject
{
    return *_buffer;
}

@end
