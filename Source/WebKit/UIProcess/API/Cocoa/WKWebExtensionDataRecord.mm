/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WKWebExtensionDataRecordInternal.h"

#import "WKWebExtensionDataTypeInternal.h"

NSErrorDomain const WKWebExtensionDataRecordErrorDomain = @"WKWebExtensionDataRecordErrorDomain";

@implementation WKWebExtensionDataRecord

#if ENABLE(WK_WEB_EXTENSIONS)

WK_OBJECT_DEALLOC_IMPL_ON_MAIN_THREAD(WKWebExtensionDataRecord, WebExtensionDataRecord, _webExtensionDataRecord);

- (BOOL)isEqual:(id)object
{
    if (self == object)
        return YES;

    auto *other = dynamic_objc_cast<WKWebExtensionDataRecord>(object);
    if (!other)
        return NO;

    return *_webExtensionDataRecord == *other->_webExtensionDataRecord;
}

- (NSString *)displayName
{
    return _webExtensionDataRecord->displayName().createNSString().autorelease();
}

- (NSString *)uniqueIdentifier
{
    return _webExtensionDataRecord->uniqueIdentifier().createNSString().autorelease();
}

- (NSSet<WKWebExtensionDataType> *)containedDataTypes
{
    return toAPI(protect(*_webExtensionDataRecord)->types());
}

- (NSUInteger)totalSizeInBytes
{
    return protect(*_webExtensionDataRecord)->totalSize();
}

- (NSUInteger)sizeInBytesOfTypes:(NSSet<WKWebExtensionDataType> *)dataTypes
{
    return protect(*_webExtensionDataRecord)->sizeOfTypes(WebKit::toWebExtensionDataTypes(dataTypes));
}

- (NSArray<NSError *> *)errors
{
    return protect(*_webExtensionDataRecord)->errors();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_webExtensionDataRecord;
}

- (WebKit::WebExtensionDataRecord&)_webExtensionDataRecord
{
    return *_webExtensionDataRecord;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

- (NSString *)displayName
{
    return nil;
}

- (NSString *)uniqueIdentifier
{
    return nil;
}

- (NSSet<WKWebExtensionDataType> *)containedDataTypes
{
    return nil;
}

- (NSUInteger)totalSizeInBytes
{
    return 0;
}

- (NSUInteger)sizeInBytesOfTypes:(NSSet<WKWebExtensionDataType> *)dataTypes
{
    return 0;
}

- (NSArray<NSError *> *)errors
{
    return nil;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

static std::optional<Ref<WebExtensionDataRecord>> makeVectorElement(const Ref<WebExtensionDataRecord>*, id arrayElement)
{
    if (auto *record = dynamic_objc_cast<WKWebExtensionDataRecord>(arrayElement))
        return Ref { record._webExtensionDataRecord };
    return std::nullopt;
}

static RetainPtr<id> makeNSArrayElement(const Ref<WebExtensionDataRecord>& vectorElement)
{
    return vectorElement->wrapper();
}

Vector<Ref<WebExtensionDataRecord>> toWebExtensionDataRecords(NSArray *records)
{
    return makeVector<Ref<WebExtensionDataRecord>>(records);
}

NSArray *toAPI(const Vector<Ref<WebExtensionDataRecord>>& records)
{
    return createNSArray(records).get();
}

NSError *createDataRecordError(WKWebExtensionDataRecordError error, NSString *debugDescription)
{
    NSDictionary *userInfo = debugDescription ? @{ NSDebugDescriptionErrorKey: debugDescription } : @{ };
    return [NSError errorWithDomain:WKWebExtensionDataRecordErrorDomain code:error userInfo:userInfo];
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
