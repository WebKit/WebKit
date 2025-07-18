/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TelephoneNumberDetector.h"

#if ENABLE(TELEPHONE_NUMBER_DETECTION)

#include <wtf/SoftLinking.h>

#if USE(APPLE_INTERNAL_SDK)
#include <DataDetectorsCore/DDDFAScanner.h>
#else
typedef struct __DDDFAScanner DDDFAScanner, * DDDFAScannerRef;
struct __DDDFACache;
#endif

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(DataDetectorsCore)
SOFT_LINK(DataDetectorsCore, DDDFACacheCreateFromFramework, struct __DDDFACache*, (), ())
SOFT_LINK(DataDetectorsCore, DDDFAScannerCreateFromCache, DDDFAScannerRef, (struct __DDDFACache* cache), (cache))
SOFT_LINK(DataDetectorsCore, DDDFAScannerFirstResultInUnicharArray, Boolean, (DDDFAScannerRef scanner, const UniChar* str, unsigned length, int* startPos, int* endPos), (scanner, str, length, startPos, endPos))

namespace WebCore {
namespace TelephoneNumberDetector {

static DDDFAScannerRef phoneNumbersScanner()
{
    static NeverDestroyed<RetainPtr<DDDFAScannerRef>> scanner;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        if (DataDetectorsCoreLibrary()) {
            if (auto cache = adoptCF(DDDFACacheCreateFromFramework()))
                scanner.get() = adoptCF(DDDFAScannerCreateFromCache(cache.get()));
        }
    });
    return scanner.get().get();
}

void prewarm()
{
    // Prewarm on a background queue to avoid hanging the main thread.
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        phoneNumbersScanner();
    });
}

bool isSupported()
{
    return phoneNumbersScanner() != nullptr;
}

bool find(std::span<const char16_t> buffer, int* startPos, int* endPos)
{
    ASSERT(isSupported());
    return DDDFAScannerFirstResultInUnicharArray(phoneNumbersScanner(), reinterpret_cast<const UniChar*>(buffer.data()), buffer.size(), startPos, endPos);
}

} // namespace TelephoneNumberDetector
} // namespace WebCore

#endif // ENABLE(TELEPHONE_NUMBER_DETECTION)
