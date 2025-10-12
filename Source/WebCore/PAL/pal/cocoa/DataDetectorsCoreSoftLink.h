/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>

#if ENABLE(DATA_DETECTION)

#include <pal/spi/cocoa/DataDetectorsCoreSPI.h>
#include <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, DataDetectorsCore);

SOFT_LINK_CLASS_FOR_HEADER(PAL, DDScannerResult)

#if PLATFORM(MAC)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, DataDetectorsCore, DDBinderPhoneNumberKey, CFStringRef)
#elif PLATFORM(IOS_FAMILY)
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDResultGetRange, CFRange, (DDResultRef result), (result))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDResultGetType, CFStringRef, (DDResultRef result), (result))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDResultGetCategory, DDResultCategory, (DDResultRef result), (result))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDScanQueryAddTextFragment, void, (DDScanQueryRef query, CFStringRef fragment, CFRange range, void *identifier, DDTextFragmentMode mode, DDTextCoalescingType type), (query, fragment, range, identifier, mode, type))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDScanQueryAddSeparator, void, (DDScanQueryRef query, DDTextCoalescingType type), (query, type))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDScanQueryAddLineBreak, void, (DDScanQueryRef query), (query))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDScanQueryGetFragmentMetaData, void *, (DDScanQueryRef query, CFIndex queryIndex), (query, queryIndex))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDResultHasProperties, bool, (DDResultRef result, CFIndex propertySet), (result, propertySet))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDResultGetSubResults, CFArrayRef, (DDResultRef result), (result))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDResultGetQueryRangeForURLification, DDQueryRange, (DDResultRef result), (result))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDURLStringForResult, NSString *, (DDResultRef currentResult, NSString * resultIdentifier, DDURLifierPhoneNumberDetectionTypes includingTelGroups, NSDate * referenceDate, NSTimeZone * referenceTimeZone), (currentResult, resultIdentifier, includingTelGroups, referenceDate, referenceTimeZone))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDURLTapAndHoldSchemes, NSArray *, (), ())
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDShouldImmediatelyShowActionSheetForURL, BOOL, (NSURL *url), (url))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDShouldImmediatelyShowActionSheetForResult, BOOL, (DDResultRef result), (result))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDShouldUseLightLinksForResult, BOOL, (DDResultRef result, BOOL extractedFromSignature), (result, extractedFromSignature))
SOFT_LINK_POINTER_FOR_HEADER(PAL, DataDetectorsCore, DDBinderParsecSourceKey, CFStringRef)
SOFT_LINK_POINTER_FOR_HEADER(PAL, DataDetectorsCore, DDBinderHttpURLKey, CFStringRef)
SOFT_LINK_POINTER_FOR_HEADER(PAL, DataDetectorsCore, DDBinderWebURLKey, CFStringRef)
SOFT_LINK_POINTER_FOR_HEADER(PAL, DataDetectorsCore, DDBinderMailURLKey, CFStringRef)
SOFT_LINK_POINTER_FOR_HEADER(PAL, DataDetectorsCore, DDBinderGenericURLKey, CFStringRef)
SOFT_LINK_POINTER_FOR_HEADER(PAL, DataDetectorsCore, DDBinderEmailKey, CFStringRef)
SOFT_LINK_POINTER_FOR_HEADER(PAL, DataDetectorsCore, DDBinderTrackingNumberKey, CFStringRef)
SOFT_LINK_POINTER_FOR_HEADER(PAL, DataDetectorsCore, DDBinderFlightInformationKey, CFStringRef)
SOFT_LINK_POINTER_FOR_HEADER(PAL, DataDetectorsCore, DDBinderSignatureBlockKey, CFStringRef)
SOFT_LINK_CONSTANT_FOR_HEADER(PAL, DataDetectorsCore, DDScannerCopyResultsOptionsForPassiveUse, DDScannerCopyResultsOptions)
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDScannerEnableOptionalSource, void, (DDScannerRef scanner, DDScannerSource source, Boolean enable), (scanner, source, enable))
#endif // PLATFORM(IOS_FAMILY)
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDResultIsPastDate, Boolean, (DDResultRef result, CFDateRef referenceDate, CFTimeZoneRef referenceTimeZone), (result, referenceDate, referenceTimeZone))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDScannerCreate, DDScannerRef, (DDScannerType type, DDScannerOptions options, CFErrorRef * errorRef), (type, options, errorRef))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDScannerScanQuery, Boolean, (DDScannerRef scanner, DDScanQueryRef query), (scanner, query))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDScanQueryCreate, DDScanQueryRef, (CFAllocatorRef allocator), (allocator))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDScanQueryCreateFromString, DDScanQueryRef, (CFAllocatorRef allocator, CFStringRef string, CFRange range), (allocator, string, range))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDScannerCopyResultsWithOptions, CFArrayRef, (DDScannerRef scanner, DDScannerCopyResultsOptions options), (scanner, options))
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_HEADER(PAL, DataDetectorsCore, DDResultDisableURLSchemeChecking, void, (), ())
#if HAVE(DDSCANNER_QOS_CONFIGURATION)
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectorsCore, DDScannerSetQOS, void, (DDScannerRef scanner, DDQOS qos), (scanner, qos))
#endif
#endif // ENABLE(DATA_DETECTION)
