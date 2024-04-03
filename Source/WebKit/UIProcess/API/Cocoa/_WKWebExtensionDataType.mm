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
#import "_WKWebExtensionDataType.h"

#import "_WKWebExtensionDataTypeInternal.h"
#import <wtf/OptionSet.h>

_WKWebExtensionDataType const _WKWebExtensionDataTypeLocal = @"local";
_WKWebExtensionDataType const _WKWebExtensionDataTypeSession = @"session";
_WKWebExtensionDataType const _WKWebExtensionDataTypeSync = @"sync";

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

OptionSet<WebExtensionDataType> toWebExtensionDataTypes(NSSet *types)
{
    OptionSet<WebExtensionDataType> result;

    if ([types containsObject:_WKWebExtensionDataTypeLocal])
        result.add(WebExtensionDataType::Local);

    if ([types containsObject:_WKWebExtensionDataTypeSession])
        result.add(WebExtensionDataType::Session);

    if ([types containsObject:_WKWebExtensionDataTypeSync])
        result.add(WebExtensionDataType::Sync);

    return result;
}

NSSet *toAPI(OptionSet<WebExtensionDataType> types)
{
    auto *result = [[NSMutableSet alloc] init];

    if (types.contains(WebExtensionDataType::Local))
        [result addObject:_WKWebExtensionDataTypeLocal];

    if (types.contains(WebExtensionDataType::Session))
        [result addObject:_WKWebExtensionDataTypeSession];

    if (types.contains(WebExtensionDataType::Sync))
        [result addObject:_WKWebExtensionDataTypeSync];

    return [result copy];
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
