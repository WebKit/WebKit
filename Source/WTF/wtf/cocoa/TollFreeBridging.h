/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#ifdef __OBJC__
#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <wtf/spi/cocoa/IOSurfaceSPI.h>
#endif

namespace WTF {

template<typename> struct CFTollFreeBridgingTraits;
template<typename> struct NSTollFreeBridgingTraits;

#ifdef __OBJC__

#define WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFClassName, NSClassName) \
template<> struct CFTollFreeBridgingTraits<CFClassName##Ref> { using BridgedType = NSClassName *; }; \
template<> struct NSTollFreeBridgingTraits<NSClassName> { using BridgedType = CFClassName##Ref; };

WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFArray, NSArray)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFAttributedString, NSAttributedString)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFCharacterSet, NSCharacterSet)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFData, NSData)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFDate, NSDate)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFDictionary, NSDictionary)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFError, NSError)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFFileSecurity, NSFileSecurity)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFLocale, NSLocale)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFNull, NSNull)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFNumber, NSNumber)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFSet, NSSet)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFString, NSString)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFTimeZone, NSTimeZone)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFURL, NSURL)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(IOSurface, ::IOSurface)

template<> struct CFTollFreeBridgingTraits<CFBooleanRef> { using BridgedType = NSNumber *; };

WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFMutableArray, NSMutableArray)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFMutableAttributedString, NSMutableAttributedString)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFMutableData, NSMutableData)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFMutableDictionary, NSMutableDictionary)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFMutableSet, NSMutableSet)
WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS(CFMutableString, NSMutableString)

#undef WTF_DECLARE_TOLL_FREE_BRIDGING_TRAITS

#endif // defined(__OBJC__)

} // namespace WTF
