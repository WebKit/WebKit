/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import "WKProcessGroup.h"
#import "WKProcessGroupInternal.h"

#import "WKContext.h"
#import "WKRetainPtr.h"
#import "WKStringCF.h"

@interface WKProcessGroupData : NSObject {
@public
    WKRetainPtr<WKContextRef> _contextRef;
}
@end

@implementation WKProcessGroupData
@end

@implementation WKProcessGroup

- (id)init
{
    return [self initWithInjectedBundleURL:nil];
}

- (id)initWithInjectedBundleURL:(NSURL *)bundleURL
{
    self = [super init];
    if (!self)
        return nil;

    _data = [[WKProcessGroupData alloc] init];
    
    if (bundleURL)
        _data->_contextRef = adoptWK(WKContextCreateWithInjectedBundlePath(adoptWK(WKStringCreateWithCFString((CFStringRef)[bundleURL absoluteString])).get()));
    else
        _data->_contextRef = adoptWK(WKContextCreate());

    return self;
}

- (void)dealloc
{
    [_data release];
    [super dealloc];
}

@end

@implementation WKProcessGroup (Internal)

- (WKContextRef)contextRef
{
    return _data->_contextRef.get();
}

@end


