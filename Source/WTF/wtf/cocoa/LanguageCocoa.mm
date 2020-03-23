/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import <wtf/Language.h>

#import <wtf/NeverDestroyed.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/spi/cocoa/NSLocaleSPI.h>

namespace WTF {

bool canMinimizeLanguages()
{
    static const bool result = []() -> bool {
#if PLATFORM(MAC)
        if (applicationSDKVersion() < DYLD_MACOSX_VERSION_10_15_4)
            return false;
#endif
#if PLATFORM(IOS)
        if (applicationSDKVersion() < DYLD_IOS_VERSION_13_4)
            return false;
#endif
        return [NSLocale respondsToSelector:@selector(minimizedLanguagesFromLanguages:)];
    }();
    return result;
}

RetainPtr<CFArrayRef> minimizedLanguagesFromLanguages(CFArrayRef languages)
{
    if (!canMinimizeLanguages())
        return languages;

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    return (__bridge CFArrayRef)[NSLocale minimizedLanguagesFromLanguages:(__bridge NSArray<NSString *> *)languages];
ALLOW_NEW_API_WITHOUT_GUARDS_END
}

}
