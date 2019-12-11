/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "WebArchiveResourceFromNSAttributedString.h"

#import "ArchiveResource.h"
#import "MIMETypeRegistry.h"

using namespace WebCore;

@implementation WebArchiveResourceFromNSAttributedString

- (instancetype)initWithData:(NSData *)data URL:(NSURL *)URL MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)textEncodingName frameName:(NSString *)frameName
{
    if (!(self = [super init]))
        return nil;

    if (!data || !URL || !MIMEType) {
        [self release];
        return nil;
    }

    if ([MIMEType isEqualToString:@"application/octet-stream"]) {
        // FIXME: This is a workaround for <rdar://problem/36074429>, and can be removed once that is fixed.
        auto mimeTypeFromURL = MIMETypeRegistry::getMIMETypeForExtension(URL.pathExtension);
        if (!mimeTypeFromURL.isEmpty())
            MIMEType = mimeTypeFromURL;
    }

    resource = ArchiveResource::create(SharedBuffer::create(adoptNS([data copy]).get()), URL, MIMEType, textEncodingName, frameName, { });
    if (!resource) {
        [self release];
        return nil;
    }

    return self;
}

- (NSString *)MIMEType
{
    return resource->mimeType();
}

- (NSURL *)URL
{
    return resource->url();
}

@end
