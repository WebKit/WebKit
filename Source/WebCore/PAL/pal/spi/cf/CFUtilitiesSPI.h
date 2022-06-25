/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#pragma once

#if USE(APPLE_INTERNAL_SDK)

#import <CoreFoundation/CFPriv.h>

#else

#include <CoreFoundation/CoreFoundation.h>

typedef CF_ENUM(CFIndex, CFSystemVersion) {
    CFSystemVersionLion = 7,
    CFSystemVersionMountainLion = 8,
};

typedef CF_OPTIONS(uint64_t, __CFRunLoopOptions) {
    __CFRunLoopOptionsEnableAppNap = 0x3b000000
};

#endif

WTF_EXTERN_C_BEGIN

extern const CFStringRef kCFWebServicesProviderDefaultDisplayNameKey;
extern const CFStringRef kCFWebServicesTypeWebSearch;
extern const CFStringRef _kCFSystemVersionBuildVersionKey;
extern const CFStringRef _kCFSystemVersionProductUserVisibleVersionKey;
extern const CFStringRef _kCFSystemVersionProductVersionKey;

Boolean _CFAppVersionCheckLessThan(CFStringRef bundleID, int linkedOnAnOlderSystemThan, double versionNumberLessThan);
Boolean _CFExecutableLinkedOnOrAfter(CFSystemVersion);
CFDictionaryRef _CFCopySystemVersionDictionary();
CFDictionaryRef _CFWebServicesCopyProviderInfo(CFStringRef serviceType, Boolean* outIsUserSelection);

void __CFRunLoopSetOptionsReason(__CFRunLoopOptions opts, CFStringRef reason);

#ifdef __OBJC__
void _CFPrefsSetDirectModeEnabled(BOOL enabled);
#endif
void _CFPrefsSetReadOnly(Boolean flag);

WTF_EXTERN_C_END
