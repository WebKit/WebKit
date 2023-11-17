/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "CoreIPCDDScannerResult.h"

#if ENABLE(DATA_DETECTION)
#if PLATFORM(COCOA)

#import "CoreIPCDictionary.h"
#import "CoreIPCNSCFObject.h"
#import <pal/cocoa/DataDetectorsCoreSoftLink.h>

@interface DDScannerResult ()
- (NSDictionary *)_webKitPropertyListData;
- (id)_initWithWebKitPropertyListData:(NSDictionary *)plist;
@end

namespace WebKit {

static bool shouldWrapDDScannerResult()
{
    static std::once_flag onceFlag;
    static bool shouldWrap;
    std::call_once(onceFlag, [&] {
        shouldWrap = PAL::getDDScannerResultClass()
            && [PAL::getDDScannerResultClass() instancesRespondToSelector:@selector(_webKitPropertyListData)]
            && [PAL::getDDScannerResultClass() instancesRespondToSelector:@selector(_initWithWebKitPropertyListData:)];
    });

    return shouldWrap;
}

CoreIPCDDScannerResult::Value CoreIPCDDScannerResult::valueFromDDScannerResult(DDScannerResult *result)
{
    if (shouldWrapDDScannerResult())
        return CoreIPCDictionary { [result _webKitPropertyListData] };
    return CoreIPCSecureCoding { result };
}

CoreIPCDDScannerResult::CoreIPCDDScannerResult(DDScannerResult *result)
    : m_value(valueFromDDScannerResult(result))
{
}

RetainPtr<id> CoreIPCDDScannerResult::toID() const
{
    RetainPtr<id> result;

    WTF::switchOn(m_value,
        [&](const CoreIPCDictionary& dictionary) {
            ASSERT(shouldWrapDDScannerResult());
            result = adoptNS([[PAL::getDDScannerResultClass() alloc] _initWithWebKitPropertyListData:dictionary.toID().get()]);
        }, [&](const CoreIPCSecureCoding& secureCoding) {
            result = secureCoding.toID();
        }
    );

    return result;
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
#endif // ENABLE(DATA_DETECTION)
