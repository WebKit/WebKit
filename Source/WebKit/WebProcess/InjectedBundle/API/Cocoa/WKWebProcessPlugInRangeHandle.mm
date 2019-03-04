/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "WKWebProcessPlugInRangeHandleInternal.h"

#import "InjectedBundleNodeHandle.h"
#import "WKWebProcessPlugInFrameInternal.h"
#import <WebCore/DataDetection.h>
#import <WebCore/Range.h>

#if ENABLE(DATA_DETECTION)
#import "WKDataDetectorTypesInternal.h"
#endif

@implementation WKWebProcessPlugInRangeHandle {
    API::ObjectStorage<WebKit::InjectedBundleRangeHandle> _rangeHandle;
}

- (void)dealloc
{
    _rangeHandle->~InjectedBundleRangeHandle();
    [super dealloc];
}

+ (WKWebProcessPlugInRangeHandle *)rangeHandleWithJSValue:(JSValue *)value inContext:(JSContext *)context
{
    JSContextRef contextRef = [context JSGlobalContextRef];
    JSObjectRef objectRef = JSValueToObject(contextRef, [value JSValueRef], nullptr);
    return wrapper(WebKit::InjectedBundleRangeHandle::getOrCreate(contextRef, objectRef));
}

- (WKWebProcessPlugInFrame *)frame
{
    return wrapper(_rangeHandle->document()->documentFrame());
}

- (NSString *)text
{
    return _rangeHandle->text();
}

#if TARGET_OS_IPHONE
- (NSArray *)detectDataWithTypes:(WKDataDetectorTypes)types context:(NSDictionary *)context
{
#if ENABLE(DATA_DETECTION)
    RefPtr<WebCore::Range> coreRange = &_rangeHandle->coreRange();
    return WebCore::DataDetection::detectContentInRange(coreRange, fromWKDataDetectorTypes(types), context);
#else
    return nil;
#endif
}
#endif

- (WebKit::InjectedBundleRangeHandle&)_rangeHandle
{
    return *_rangeHandle;
}

// MARK: WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_rangeHandle;
}

@end
