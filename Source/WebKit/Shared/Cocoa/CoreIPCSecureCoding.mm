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
#import "CoreIPCSecureCoding.h"

#if PLATFORM(COCOA)

#import "ArgumentCodersCocoa.h"
#import "AuxiliaryProcessCreationParameters.h"
#import "WKCrashReporter.h"
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/NSStringExtras.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/StringHash.h>

namespace WebKit {

namespace SecureCoding {

static std::unique_ptr<HashSet<String>>& internalClassNamesExemptFromSecureCodingCrash()
{
    static NeverDestroyed<std::unique_ptr<HashSet<String>>> exemptClassNames = []() -> std::unique_ptr<HashSet<String>> {
        if (isInAuxiliaryProcess())
            return nullptr;

        RetainPtr<NSArray> array = [[NSUserDefaults standardUserDefaults] arrayForKey:@"WebKitCrashOnSecureCodingWithExemptClassesKey"];
        if (!array)
            return nullptr;

        auto exemptClassNames = WTF::makeUnique<HashSet<String>>();

        for (id value in array.get()) {
            if (RetainPtr string = dynamic_objc_cast<NSString>(value))
                exemptClassNames->add(string.get());
        }
        return exemptClassNames;
    }();

    return exemptClassNames.get();
}

const HashSet<String>* classNamesExemptFromSecureCodingCrash()
{
    return internalClassNamesExemptFromSecureCodingCrash().get();
}

void applyProcessCreationParameters(AuxiliaryProcessCreationParameters&& parameters)
{
    RELEASE_ASSERT(isInAuxiliaryProcess());

    auto& exemptClassNames = internalClassNamesExemptFromSecureCodingCrash();
    RELEASE_ASSERT(!exemptClassNames);

    if (parameters.classNamesExemptFromSecureCodingCrash)
        *exemptClassNames = WTFMove(*parameters.classNamesExemptFromSecureCodingCrash);
}

} // namespace SecureCoding

#if !HAVE(WK_SECURE_CODING_NSURLREQUEST)
WTF_MAKE_TZONE_ALLOCATED_IMPL(CoreIPCSecureCoding);
#endif

bool conformsToWebKitSecureCoding(id object)
{
    return [object respondsToSelector:@selector(_webKitPropertyListData)]
        && [object respondsToSelector:@selector(_initWithWebKitPropertyListData:)];
}

#if !HAVE(WK_SECURE_CODING_NSURLREQUEST)
[[noreturn]] static void crashWithClassName(Class objectClass)
{
    WebKit::logAndSetCrashLogMessage("NSSecureCoding path used for unexpected object"_s);

    std::array<uint64_t, 6> values { 0, 0, 0, 0, 0, 0 };
    memcpySpan(asMutableByteSpan(std::span { values }), span(NSStringFromClass(objectClass)));
    CRASH_WITH_INFO(values[0], values[1], values[2], values[3], values[4], values[5]);
}

CoreIPCSecureCoding::CoreIPCSecureCoding(id object)
    : m_secureCoding((NSObject<NSSecureCoding> *)object)
{
    RELEASE_ASSERT(!m_secureCoding || [object conformsToProtocol:@protocol(NSSecureCoding)]);

    auto* exemptClassNames = SecureCoding::classNamesExemptFromSecureCodingCrash();
    if (!exemptClassNames)
        return;

    if (exemptClassNames->contains(NSStringFromClass([object class])))
        return;

    crashWithClassName([object class]);
}
#endif

} // namespace WebKit

#endif // PLATFORM(COCOA)
