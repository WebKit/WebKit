/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "StringUtilities.h"

#import "WKSharedAPICast.h"
#import "WKStringCF.h"
#import <JavaScriptCore/RegularExpression.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/StringBuilder.h>

namespace WebKit {

NSString *nsStringFromWebCoreString(const String& string)
{
    return string.isEmpty() ? @"" : CFBridgingRelease(WKStringCopyCFString(0, toAPI(string.impl())));
}

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)

SOFT_LINK_PRIVATE_FRAMEWORK(PhoneNumbers);

typedef struct __CFPhoneNumber* CFPhoneNumberRef;

// These functions are declared with __attribute__((visibility ("default")))
// We currently don't have a way to soft link such functions, so we forward declare them again here.
extern "C" CFPhoneNumberRef CFPhoneNumberCreate(CFAllocatorRef, CFStringRef, CFStringRef);
SOFT_LINK(PhoneNumbers, CFPhoneNumberCreate, CFPhoneNumberRef, (CFAllocatorRef allocator, CFStringRef digits, CFStringRef countryCode), (allocator, digits, countryCode));

extern "C" CFStringRef CFPhoneNumberCopyFormattedRepresentation(CFPhoneNumberRef);
SOFT_LINK(PhoneNumbers, CFPhoneNumberCopyFormattedRepresentation, CFStringRef, (CFPhoneNumberRef phoneNumber), (phoneNumber));

extern "C" CFStringRef CFPhoneNumberCopyUnformattedRepresentation(CFPhoneNumberRef);
SOFT_LINK(PhoneNumbers, CFPhoneNumberCopyUnformattedRepresentation, CFStringRef, (CFPhoneNumberRef phoneNumber), (phoneNumber));


NSString *formattedPhoneNumberString(NSString *originalPhoneNumber)
{
    NSString *countryCode = [[[NSLocale currentLocale] objectForKey:NSLocaleCountryCode] lowercaseString];

    RetainPtr<CFPhoneNumberRef> phoneNumber = adoptCF(CFPhoneNumberCreate(kCFAllocatorDefault, (__bridge CFStringRef)originalPhoneNumber, (__bridge CFStringRef)countryCode));
    if (!phoneNumber)
        return originalPhoneNumber;

    CFStringRef phoneNumberString = CFPhoneNumberCopyFormattedRepresentation(phoneNumber.get());
    if (!phoneNumberString)
        phoneNumberString = CFPhoneNumberCopyUnformattedRepresentation(phoneNumber.get());

    return CFBridgingRelease(phoneNumberString);
}

#else

NSString *formattedPhoneNumberString(NSString *)
{
    return nil;
}

#endif // ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)

}
