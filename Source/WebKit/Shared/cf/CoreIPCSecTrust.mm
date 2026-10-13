/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if HAVE(WK_SECURE_CODING_SECTRUST) && USE(CF)

#import "config.h"
#import "CoreIPCSecTrust.h"

#import "Logging.h"
#import <wtf/Borrow.h>
#import <wtf/Compiler.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/MakeString.h>

namespace WebKit {


static bool arrayElementsTheSameType(NSArray *array, Class c)
{
    if (!array.count)
        return true;
    // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
    for (SUPPRESS_UNRETAINED_LOCAL id element in array) {
        if (![element isKindOfClass:c])
            return false;
    }
    return true;
}

CoreIPCSecTrust::PolicyOptionValueShape CoreIPCSecTrust::detectPolicyOptionShape(id option)
{
    if ([option isKindOfClass:NSNumber.class]) {
        NSNumber *candidateBool = option;
        if ([candidateBool isEqualToNumber:@YES] || [candidateBool isEqualToNumber:@NO])
            return PolicyOptionValueShape::Bool;
    } else if ([option isKindOfClass:NSString.class])
        return PolicyOptionValueShape::String;
    else if ([option isKindOfClass:NSArray.class]) {
        NSArray *a = option;
        id element = a.firstObject;
        if (!element) {
            RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust::detectPolicyOptionShape element was nil");
            ASSERT_NOT_REACHED();
            return PolicyOptionValueShape::Invalid;
        }
        if ([element isKindOfClass:NSNumber.class])
            return PolicyOptionValueShape::ArrayOfNumbers;
        if ([element isKindOfClass:NSString.class])
            return PolicyOptionValueShape::ArrayOfStrings;
        if ([element isKindOfClass:NSData.class])
            return PolicyOptionValueShape::ArrayOfData;
        if ([element isKindOfClass:NSArray.class])
            return PolicyOptionValueShape::ArrayOfArrayContainingDateOrNumber;
    } else if ([option isKindOfClass:NSDictionary.class])
        return PolicyOptionValueShape::DictionaryValueIsNumber;

    RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust::detectPolicyOptionShape was unable to detect option's shape");
    ASSERT_NOT_REACHED();
    return PolicyOptionValueShape::Invalid;
}

static String updatePolicyVector(NSDictionary *policyOption, CoreIPCSecTrustData::PolicyOption& policyVector)
{
    policyVector.reserveCapacity(policyOption.count);
    // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
    for (SUPPRESS_UNRETAINED_LOCAL NSString *optionKey in policyOption) {
        if (![optionKey isKindOfClass:NSString.class])
            return makeString("optionKey was not an NSString"_s);
        CoreIPCString k { optionKey };
        id optionValue = [policyOption objectForKey:optionKey];

        CoreIPCSecTrust::PolicyOptionValueShape shape = CoreIPCSecTrust::detectPolicyOptionShape(optionValue);

        switch (shape) {
        case CoreIPCSecTrust::PolicyOptionValueShape::Invalid:
            return makeString("policy shape for key "_s, (String)optionKey, "is invalid"_s);
        case CoreIPCSecTrust::PolicyOptionValueShape::Bool: {
            RetainPtr value = dynamic_objc_cast<NSNumber>(optionValue);
            if (!value)
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::Bool unexpected type for key "_s, (String)optionKey);
            CoreIPCSecTrustData::PolicyVariant v = static_cast<bool>([value boolValue]);
            policyVector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            break;
        }
        case CoreIPCSecTrust::PolicyOptionValueShape::String: {
            RetainPtr value = dynamic_objc_cast<NSString>(optionValue);
            if (!value)
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::String unexpected type for key "_s, (String)optionKey);
            CoreIPCSecTrustData::PolicyVariant v = CoreIPCString(value.get());
            policyVector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            break;
        }
        case CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfNumbers: {
            RetainPtr value = dynamic_objc_cast<NSArray>(optionValue);
            if (!value)
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfNumbers unexpected type for key "_s, (String)optionKey, " (expecting NSArray)"_s);
            if (![value count])
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfNumbers array length 0 for key "_s, (String)optionKey);
            if (!arrayElementsTheSameType(value.get(), NSNumber.class))
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfNumbers unexpected type for key "_s, (String)optionKey, " (expecting NSNumber)"_s);
            Vector<CoreIPCNumber> vector;
            vector.reserveCapacity([value count]);
            // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
            for (SUPPRESS_UNRETAINED_LOCAL NSNumber *element in value.get()) {
                CoreIPCNumber n { element };
                vector.append(WTF::move(n));
            }
            CoreIPCSecTrustData::PolicyVariant v = WTF::move(vector);
            policyVector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            break;
        }
        case CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfStrings: {
            RetainPtr value = dynamic_objc_cast<NSArray>(optionValue);
            if (!value)
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfStrings unexpected type for key "_s, (String)optionKey, " (expecting NSArray)"_s);
            if (![value count])
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfStrings array length 0 for key "_s, (String)optionKey);
            if (!arrayElementsTheSameType(value.get(), NSString.class))
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfStrings unexpected type for key "_s, (String)optionKey, " (expecting NSString)"_s);
            Vector<CoreIPCString> vector;
            vector.reserveCapacity([value count]);
            // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
            for (SUPPRESS_UNRETAINED_LOCAL NSString *element in value.get()) {
                CoreIPCString s { element };
                vector.append(WTF::move(s));
            }
            CoreIPCSecTrustData::PolicyVariant v = WTF::move(vector);
            policyVector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            break;
        }
        case CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfData: {
            RetainPtr value = dynamic_objc_cast<NSArray>(optionValue);
            if (!value)
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfData unexpected type for key %@ "_s, (String)optionKey, " (expecting NSArray)"_s);
            if (![value count])
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfData array length 0 for key "_s, (String)optionKey);
            if (!arrayElementsTheSameType(value.get(), NSData.class))
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfData unexpected type for key "_s, (String)optionKey, " (expecting NSData)"_s);
            Vector<CoreIPCData> vector;
            vector.reserveCapacity([value count]);
            // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
            for (SUPPRESS_UNRETAINED_LOCAL NSData *element in value.get()) {
                CoreIPCData d { element };
                vector.append(WTF::move(d));
            }
            CoreIPCSecTrustData::PolicyVariant v = WTF::move(vector);
            policyVector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            break;
        }
        case CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfArrayContainingDateOrNumber: {
            RetainPtr value = dynamic_objc_cast<NSArray>(optionValue);
            if (!value)
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfArrayContainingDateOrNumber unexpected type for key "_s, (String)optionKey, " (expecting NSArray)"_s);
            if (![value count])
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfArrayContainingDateOrNumber array length 0 for key "_s, (String)optionKey);
            if (!arrayElementsTheSameType(value.get(), NSArray.class))
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfArrayContainingDateOrNumber unexpected type for key "_s, (String)optionKey, " (expecting NSArray)"_s);

            CoreIPCSecTrustData::PolicyArrayOfArrayContainingDateOrNumbers outerVector;
            outerVector.reserveCapacity([value count]);

            // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
            for (SUPPRESS_UNRETAINED_LOCAL NSArray *secondLevelArray in value.get()) {
                if (![secondLevelArray isKindOfClass:NSArray.class])
                    return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfArrayContainingDateOrNumber second level array unexpected type for key "_s, (String)optionKey);

                Vector<Variant<WebKit::CoreIPCNumber, WebKit::CoreIPCDate>> innerVector;
                innerVector.reserveCapacity(secondLevelArray.count);

                // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
                for (SUPPRESS_UNRETAINED_LOCAL id element in secondLevelArray) {
                    if (RetainPtr e = dynamic_objc_cast<NSNumber>(element)) {
                        Variant<WebKit::CoreIPCNumber, WebKit::CoreIPCDate> v = CoreIPCNumber(e.get());
                        innerVector.append(WTF::move(v));
                    } else if (RetainPtr d = dynamic_objc_cast<NSDate>(element)) {
                        Variant<WebKit::CoreIPCNumber, WebKit::CoreIPCDate> v = CoreIPCDate(d.get());
                        innerVector.append(WTF::move(v));
                    } else
                        return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfArrayContainingDateOrNumber second level array contents unexpected type for key "_s, (String)optionKey);
                }
                outerVector.append(WTF::move(innerVector));
            }
            CoreIPCSecTrustData::PolicyVariant v = WTF::move(outerVector);
            policyVector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            break;
        }
        case CoreIPCSecTrust::PolicyOptionValueShape::DictionaryValueIsNumber: {
            RetainPtr d = dynamic_objc_cast<NSDictionary>(optionValue);
            if (!d)
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::DictionaryValueIsNumber unexpected type for key "_s, (String)optionKey, " (expecting NSDictionary)"_s);
            CoreIPCSecTrustData::PolicyDictionaryValueIsNumber vector;
            vector.reserveCapacity([d count]);
            // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
            for (SUPPRESS_UNRETAINED_LOCAL NSString* key in d.get()) {
                if (![key isKindOfClass:NSString.class])
                    return makeString("CoreIPCSecTrust::PolicyOptionValueShape::DictionaryValueIsNumber unexpected dictionary key type for key "_s, (String)optionKey, " (expecting NSString)"_s);
                NSNumber *value = [d objectForKey:key];
                if (![value isKindOfClass:NSNumber.class])
                    return makeString("CoreIPCSecTrust::PolicyOptionValueShape::DictionaryValueIsNumber unexpected dictionary value type for key "_s, (String)optionKey, " (expecting NSNumber)"_s);
                CoreIPCString s { key };
                CoreIPCNumber n { value };
                vector.append(std::make_pair(WTF::move(s), WTF::move(n)));
            }
            CoreIPCSecTrustData::PolicyVariant v = WTF::move(vector);
            policyVector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            break;
        }
        default:
            return makeString("unknown shape for key "_s, (String)optionKey);
        }
    }
    return { }; // no error
}

#define RETURN_IF_OPTIONAL_ERROR \
    if (!optionalDataError.isNull()) { \
        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust optionalArrayOfDataHelper error: %s", optionalDataError.utf8()); \
        ASSERT_NOT_REACHED(); \
        return; \
    }

static String optionalArrayOfDataHelper(std::optional<Vector<CoreIPCData>>& toSet, RetainPtr<NSDictionary> dict, NSString* topLevelKey)
{
    if (![dict isKindOfClass:NSDictionary.class])
        return makeString("optionalArrayOfDataHelper was called with wrong argument"_s);
    if (RetainPtr array = dynamic_objc_cast<NSArray>([dict objectForKey:topLevelKey])) {
        Vector<CoreIPCData> vector;
        vector.reserveCapacity([array count]);
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL NSData* item in array.get()) {
            if (![item isKindOfClass:NSData.class])
                return makeString("optionalArrayOfDataHelper had invalid type in array"_s);
            CoreIPCData c { item };
            vector.append(WTF::move(c));
        }
        toSet = { WTF::move(vector) };
    }
    return { }; // no error
}

CoreIPCSecTrust::CoreIPCSecTrust(SecTrustRef trust)
{
    if (!trust)
        return;

    RetainPtr<CFDictionaryRef> cfDictionary;
    RetainPtr<CFErrorRef> error;
    {
        // FIXME: The Security framework API is missing the `CF_RETURNS_RETAINED` annotation (rdar://161546781).
        CFErrorRef rawError = NULL;
        cfDictionary = dynamic_cf_cast<CFDictionaryRef>(adoptCF(SecTrustCopyPropertyListRepresentation(trust, &rawError)));
        SUPPRESS_RETAINPTR_CTOR_ADOPT error = adoptCF(rawError);
    }
    if (!cfDictionary || error)
        return;
    RetainPtr dict = bridge_cast(cfDictionary.get());

    CoreIPCSecTrustData secTrustData { };

    if (RetainPtr verifyDate = dynamic_objc_cast<NSDate>([dict objectForKey:@"verifyDate"])) {
        CoreIPCDate d { verifyDate.get() };
        secTrustData.verifyDate = WTF::move(d);
    }

    RetainPtr result = dynamic_objc_cast<NSNumber>([dict objectForKey:@"result"]);
    if (!result) {
        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'result' value is nil or not an NSNumber and not optional");
        return;
    }
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    switch ([result unsignedCharValue]) {
    case kSecTrustResultInvalid:
        secTrustData.result = CoreIPCSecTrustResult::Invalid;
        break;
    case kSecTrustResultProceed:
        secTrustData.result = CoreIPCSecTrustResult::Proceed;
        break;
    case kSecTrustResultConfirm: // Deprecated
        secTrustData.result = CoreIPCSecTrustResult::Confirm;
        break;
    case kSecTrustResultDeny:
        secTrustData.result = CoreIPCSecTrustResult::Deny;
        break;
    case kSecTrustResultUnspecified:
        secTrustData.result = CoreIPCSecTrustResult::Unspecified;
        break;
    case kSecTrustResultRecoverableTrustFailure:
        secTrustData.result = CoreIPCSecTrustResult::RecoverableTrustFailure;
        break;
    case kSecTrustResultFatalTrustFailure:
        secTrustData.result = CoreIPCSecTrustResult::FatalTrustFailure;
        break;
    case kSecTrustResultOtherError:
        secTrustData.result = CoreIPCSecTrustResult::OtherError;
        break;
    default:
        secTrustData.result = CoreIPCSecTrustResult::Invalid;
        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'result' value was not set to a known constant");
        ASSERT_NOT_REACHED();
        return;
    }
ALLOW_DEPRECATED_DECLARATIONS_END
    RetainPtr anchorsOnly = dynamic_objc_cast<NSNumber>([dict objectForKey:@"anchorsOnly"]);
    if (!anchorsOnly) {
        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'anchorsOnly' value is nil or not an NSNumber and not optional");
        return;
    }
    secTrustData.anchorsOnly = [anchorsOnly boolValue];

    if (RetainPtr certificates = dynamic_objc_cast<NSArray>([dict objectForKey:@"certificates"])) {
        Vector<CoreIPCData> vector;
        vector.reserveCapacity([certificates count]);
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL NSData* item in certificates.get()) {
            if (![item isKindOfClass:NSData.class]) {
                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'certificates' array contains a non NSData item");
                ASSERT_NOT_REACHED();
                return;
            }
            CoreIPCData c { item };
            vector.append(WTF::move(c));
        }
        secTrustData.certificates = WTF::move(vector);
    }

    if (RetainPtr chain = dynamic_objc_cast<NSArray>([dict objectForKey:@"chain"])) {
        Vector<CoreIPCData> vector;
        vector.reserveCapacity([chain count]);
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL NSData* item in chain.get()) {
            if (![item isKindOfClass:NSData.class]) {
                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'chain' array contains a non NSData item");
                ASSERT_NOT_REACHED();
                return;
            }
            CoreIPCData c { item };
            vector.append(WTF::move(c));
        }
        secTrustData.chain = WTF::move(vector);
    }

    if (RetainPtr details = dynamic_objc_cast<NSArray>([dict objectForKey:@"details"])) {
        Vector<CoreIPCSecTrustData::Detail> vector;
        vector.reserveCapacity([details count]);
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL NSDictionary *detail in details.get()) {
            if (![detail isKindOfClass:NSDictionary.class]) {
                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'details' array contains unexpected type");
                ASSERT_NOT_REACHED();
                return;
            }
            CoreIPCSecTrustData::Detail d;
            d.reserveCapacity(detail.count);
            // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
            for (SUPPRESS_UNRETAINED_LOCAL NSString *key in detail) {
                if (![key isKindOfClass:NSString.class]) {
                    RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'details' dictionary contains unexpected key type");
                    ASSERT_NOT_REACHED();
                    return;
                }
                NSNumber *value = [detail objectForKey:key];
                if (![value isKindOfClass:NSNumber.class]) {
                    RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'details' dictionary contains unexpected value type");
                    ASSERT_NOT_REACHED();
                    return;
                }
                CoreIPCString k { key };
                d.append(std::make_pair(WTF::move(k), [value boolValue]));
            }
            vector.append(WTF::move(d));
        }
        secTrustData.details = WTF::move(vector);
    }

    if (RetainPtr info = dynamic_objc_cast<NSDictionary>([dict objectForKey:@"info"])) {
        CoreIPCSecTrustData::InfoType vector;
        vector.reserveCapacity([info count]);
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL NSString *key in info.get()) {
            if (![key isKindOfClass:NSString.class]) {
                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'info' dictionary key contains unexpected type");
                ASSERT_NOT_REACHED();
                return;
            }
            CoreIPCString k { key };
            id value = [info objectForKey:key];
            if (RetainPtr dateValue = dynamic_objc_cast<NSDate>(value)) {
                CoreIPCDate date { dateValue.get() };
                CoreIPCSecTrustData::InfoOption v = WTF::move(date);
                vector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            } else if (RetainPtr stringValue = dynamic_objc_cast<NSString>(value)) {
                CoreIPCString s { stringValue.get() };
                CoreIPCSecTrustData::InfoOption v = WTF::move(s);
                vector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            } else if (RetainPtr candidateBool = dynamic_objc_cast<NSNumber>(value)) {
                if ([candidateBool isEqualToNumber:@YES] || [candidateBool isEqualToNumber:@NO]) {
                    bool v = [candidateBool boolValue];
                    vector.append(std::make_pair(WTF::move(k), v));
                } else {
                    RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'info' dictionary value contains unexpected NSNumber value");
                    ASSERT_NOT_REACHED();
                    return;
                }
            } else if (RetainPtr revocationInfoArray = dynamic_objc_cast<NSArray>(value)) {
                CoreIPCSecTrustData::RevocationInfoArray revocationInfo;
                revocationInfo.reserveCapacity([revocationInfoArray count]);

                // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
                for (SUPPRESS_UNRETAINED_LOCAL NSDictionary *entry in revocationInfoArray.get()) {
                    if (![entry isKindOfClass:NSDictionary.class]) {
                        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'RevocationInfo' array contains non-dictionary element");
                        ASSERT_NOT_REACHED();
                        return;
                    }

                    CoreIPCSecTrustData::RevocationInfoEntry revocationEntry;
                    revocationEntry.reserveCapacity([entry count]);

                    // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
                    for (SUPPRESS_UNRETAINED_LOCAL NSString *entryKey in entry) {
                        if (![entryKey isKindOfClass:NSString.class]) {
                            RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'RevocationInfo' entry key is not a string");
                            ASSERT_NOT_REACHED();
                            return;
                        }

                        NSDictionary *subDict = [entry objectForKey:entryKey];
                        if (![subDict isKindOfClass:NSDictionary.class]) {
                            RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'RevocationInfo' entry value is not a dictionary");
                            ASSERT_NOT_REACHED();
                            return;
                        }

                        CoreIPCSecTrustData::RevocationInfoSubDict revocationSubDict;
                        revocationSubDict.reserveCapacity([subDict count]);

                        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
                        for (SUPPRESS_UNRETAINED_LOCAL NSString *subKey in subDict) {
                            if (![subKey isKindOfClass:NSString.class]) {
                                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'RevocationInfo' sub-dictionary key is not a string");
                                ASSERT_NOT_REACHED();
                                return;
                            }

                            CoreIPCString subKeyString { subKey };
                            id subValue = [subDict objectForKey:subKey];

                            if (RetainPtr number = dynamic_objc_cast<NSNumber>(subValue)) {
                                if ([number isEqualToNumber:@YES] || [number isEqualToNumber:@NO]) {
                                    CoreIPCSecTrustData::RevocationInfoSubDictValue v = [number boolValue];
                                    revocationSubDict.append(std::make_pair(WTF::move(subKeyString), WTF::move(v)));
                                } else {
                                    CoreIPCNumber n { number.get() };
                                    CoreIPCSecTrustData::RevocationInfoSubDictValue v = WTF::move(n);
                                    revocationSubDict.append(std::make_pair(WTF::move(subKeyString), WTF::move(v)));
                                }
                            } else if (RetainPtr subDataValue = dynamic_objc_cast<NSData>(subValue)) {
                                CoreIPCData data { subDataValue.get() };
                                CoreIPCSecTrustData::RevocationInfoSubDictValue v = WTF::move(data);
                                revocationSubDict.append(std::make_pair(WTF::move(subKeyString), WTF::move(v)));
                            } else if (RetainPtr subDateValue = dynamic_objc_cast<NSDate>(subValue)) {
                                CoreIPCDate date { subDateValue.get() };
                                CoreIPCSecTrustData::RevocationInfoSubDictValue v = WTF::move(date);
                                revocationSubDict.append(std::make_pair(WTF::move(subKeyString), WTF::move(v)));
                            } else {
                                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'RevocationInfo' sub-dictionary value has unexpected type");
                                ASSERT_NOT_REACHED();
                                return;
                            }
                        }

                        CoreIPCString entryKeyString { entryKey };
                        revocationEntry.append(std::make_pair(WTF::move(entryKeyString), WTF::move(revocationSubDict)));
                    }

                    revocationInfo.append(WTF::move(revocationEntry));
                }

                CoreIPCSecTrustData::InfoOption v = WTF::move(revocationInfo);
                vector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            } else if (RetainPtr subDict = dynamic_objc_cast<NSDictionary>(value)) {
                CoreIPCSecTrustData::InfoSubDict infoSubDict;
                infoSubDict.reserveCapacity([subDict count]);
                // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
                for (SUPPRESS_UNRETAINED_LOCAL NSString *subKey in subDict.get()) {
                    if (![subKey isKindOfClass:NSString.class]) {
                        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'info' sub-dictionary key is not a string");
                        ASSERT_NOT_REACHED();
                        return;
                    }
                    id subValue = [subDict objectForKey:subKey];
                    if (![subValue isKindOfClass:NSNumber.class]) {
                        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'info' sub-dictionary value has unexpected type");
                        ASSERT_NOT_REACHED();
                        return;
                    }
                    CoreIPCString subKeyString { subKey };
                    infoSubDict.append(std::make_pair(WTF::move(subKeyString), [(NSNumber *)subValue boolValue]));
                }
                CoreIPCSecTrustData::InfoOption v = WTF::move(infoSubDict);
                vector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            } else {
                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'info' dictionary contains unexpected type");
                ASSERT_NOT_REACHED();
                return;
            }
        }
        secTrustData.info = WTF::move(vector);
    }

    RetainPtr keychainsAllowed = dynamic_objc_cast<NSNumber>([dict objectForKey:@"keychainsAllowed"]);
    if (!keychainsAllowed) {
        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'keychainsAllowed' value is nil or not an NSNumber and not optional");
        return;
    }
    secTrustData.keychainsAllowed = [keychainsAllowed boolValue];

    if (RetainPtr policies = dynamic_objc_cast<NSArray>([dict objectForKey:@"policies"])) {
        Vector<CoreIPCSecTrustData::PolicyType> outerVector;
        outerVector.reserveCapacity([policies count]);
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL NSDictionary *policy in policies.get()) {
            if (![policy isKindOfClass:NSDictionary.class]) {
                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust policy is not an NSDictionary");
                ASSERT_NOT_REACHED();
                return;
            }
            CoreIPCSecTrustData::PolicyType innerVector;
            innerVector.reserveCapacity(policy.count);
            // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
            for (SUPPRESS_UNRETAINED_LOCAL NSString *key in policy) {
                if (![key isKindOfClass:NSString.class]) {
                    RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust policy key is not an NSString");
                    ASSERT_NOT_REACHED();
                    return;
                }
                id value = [policy objectForKey:key];
                if (RetainPtr stringValue = dynamic_objc_cast<NSString>(value)) {
                    CoreIPCString k { key };
                    CoreIPCSecTrustData::PolicyValue v = CoreIPCString(stringValue.get());
                    auto p = std::make_pair(WTF::move(k), WTF::move(v));
                    innerVector.append(WTF::move(p));
                } else if ([value isKindOfClass:NSDictionary.class]) {
                    CoreIPCSecTrustData::PolicyOption policyVector;
                    String error = updatePolicyVector((NSDictionary *)value, policyVector);
                    if (!error.isNull()) {
                        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust updatePolicyVector error %s", error.utf8());
                        ASSERT_NOT_REACHED();
                        return;
                    }
                    CoreIPCString k { key };
                    CoreIPCSecTrustData::PolicyValue v = WTF::move(policyVector);
                    auto p = std::make_pair(WTF::move(k), WTF::move(v));
                    innerVector.append(WTF::move(p));
                } else {
                    RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust policy value is unexpected type");
                    ASSERT_NOT_REACHED();
                    return;
                }
            }
            outerVector.append(WTF::move(innerVector));
        }
        secTrustData.policies = WTF::move(outerVector);
    }

    String optionalDataError;
    optionalDataError = optionalArrayOfDataHelper(secTrustData.responses, dict, @"responses");
    RETURN_IF_OPTIONAL_ERROR;

    optionalDataError = optionalArrayOfDataHelper(secTrustData.scts, dict, @"scts");
    RETURN_IF_OPTIONAL_ERROR;

    optionalDataError = optionalArrayOfDataHelper(secTrustData.anchors, dict, @"anchors");
    RETURN_IF_OPTIONAL_ERROR;

    optionalDataError = optionalArrayOfDataHelper(secTrustData.trustedLogs, dict, @"trustedLogs");
    RETURN_IF_OPTIONAL_ERROR;

    if (RetainPtr exceptions = dynamic_objc_cast<NSArray>([dict objectForKey:@"exceptions"])) {
        Vector<CoreIPCSecTrustData::ExceptionType> vector;
        vector.reserveCapacity([exceptions count]);
        // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
        for (SUPPRESS_UNRETAINED_LOCAL NSDictionary *exception in exceptions.get()) {
            if (![exception isKindOfClass:NSDictionary.class]) {
                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'exceptions' array contains non NSDictionary member");
                ASSERT_NOT_REACHED();
                return;
            }
            CoreIPCSecTrustData::ExceptionType innerVector;
            innerVector.reserveCapacity([exception count]);
            // FIXME: Remove the suppression once https://github.com/llvm/llvm-project/pull/230818 is deployed (rdar://189710680).
            for (SUPPRESS_UNRETAINED_LOCAL NSString *key in exception) {
                if (![key isKindOfClass:NSString.class]) {
                    RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'exceptions' dictionary key is not an NSString");
                    ASSERT_NOT_REACHED();
                    return;
                }
                CoreIPCString k { key };
                id value = [exception objectForKey:key];
                if (RetainPtr dataValue = dynamic_objc_cast<NSData>(value)) {
                    CoreIPCData data { dataValue.get() };
                    Variant<CoreIPCNumber, CoreIPCData, bool> v = WTF::move(data);
                    auto p = std::make_pair(WTF::move(k), WTF::move(v));
                    innerVector.append(WTF::move(p));
                } else if (RetainPtr number = dynamic_objc_cast<NSNumber>(value)) {
                    if ([number isEqualToNumber:@YES] || [number isEqualToNumber:@NO]) {
                        bool v = [number boolValue];
                        auto p = std::make_pair(WTF::move(k), v);
                        innerVector.append(WTF::move(p));
                    } else {
                        CoreIPCNumber n { number.get() };
                        auto p = std::make_pair(WTF::move(k), n);
                        innerVector.append(WTF::move(p));
                    }
                } else {
                    RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'exceptions' dictionary contains unexpected type");
                    ASSERT_NOT_REACHED();
                    return;
                }
            }
            vector.append(WTF::move(innerVector));
        }
        secTrustData.exceptions = { WTF::move(vector) };
    }

    m_data = WTF::move(secTrustData);
}

static bool appendDataObject(NSMutableArray *array, const CoreIPCData& data, ASCIILiteral description)
{
    RetainPtr nsData = data.toID();
    if (!nsData) {
        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust had a null value in %" PUBLIC_LOG_STRING, description.characters());
        return false;
    }
    [array addObject:nsData.get()];
    return true;
}

static RetainPtr<NSDictionary> createPolicyDictionary(const CoreIPCSecTrustData::PolicyOption& options)
{
    RetainPtr<NSMutableDictionary> dict = adoptNS([[NSMutableDictionary alloc] initWithCapacity:options.size()]);
    for (const auto& item : borrow(options)) {
        auto key = item.first.toID();
        RetainPtr<id> value;

        WTF::switchOn(item.second,
            [&] (const bool& b) {
                value = @(b);
            },
            [&] (const CoreIPCString& s) {
                value = s.toID();
            },
            [&] (const CoreIPCSecTrustData::PolicyArrayOfNumbers& a) {
                RetainPtr array = adoptNS([[NSMutableArray alloc] initWithCapacity:a.size()]);
                for (const auto& number : borrow(a))
                    [array addObject:number.toID().get()];
                value = array;
            },
            [&] (const CoreIPCSecTrustData::PolicyArrayOfStrings& a) {
                RetainPtr array = adoptNS([[NSMutableArray alloc] initWithCapacity:a.size()]);
                for (const auto& str : borrow(a))
                    [array addObject:str.toID().get()];
                value = array;
            },
            [&] (const CoreIPCSecTrustData::PolicyArrayOfData& a) {
                RetainPtr array = adoptNS([[NSMutableArray alloc] initWithCapacity:a.size()]);
                for (const auto& d : borrow(a)) {
                    if (!appendDataObject(array, d, "policy dictionary"_s))
                        return;
                }
                value = array;
            },
            [&] (const CoreIPCSecTrustData::PolicyArrayOfArrayContainingDateOrNumbers& a) {
                RetainPtr outerArray = adoptNS([[NSMutableArray alloc] initWithCapacity:a.size()]);
                for (const Vector<Variant<CoreIPCNumber, CoreIPCDate>>& inner : borrow(a)) {
                    RetainPtr innerArray = adoptNS([[NSMutableArray alloc] initWithCapacity:inner.size()]);
                    for (const Variant<CoreIPCNumber, CoreIPCDate>& v : borrow(inner)) {
                        WTF::switchOn(v,
                            [&] (const CoreIPCNumber& i) {
                                [innerArray addObject:i.toID().get()];
                            },
                            [&] (const CoreIPCDate& d) {
                                [innerArray addObject:d.toID().get()];
                            }
                        );
                    }
                    [outerArray addObject:innerArray.get()];
                }
                value = outerArray;
            },
            [&] (const CoreIPCSecTrustData::PolicyDictionaryValueIsNumber& a) {
                RetainPtr d = adoptNS([[NSMutableDictionary alloc] initWithCapacity:a.size()]);
                for (const auto& i : borrow(a))
                    [d setObject:i.second.toID().get() forKey:i.first.toID().get()];
                value = d;
            }
        );
        if (!value)
            return nullptr;
        [dict setObject:value.get() forKey:key.get()];
    }
    return dict;
}

static bool addToDictFromOptionalDataHelper(const std::optional<Vector<CoreIPCData>>& opt, RetainPtr<NSMutableDictionary> dict, NSString* key)
{
    if (!opt)
        return true;
    RetainPtr array = adoptNS([[NSMutableArray alloc] initWithCapacity:opt->size()]);
    for (const CoreIPCData& d : borrow(*opt)) {
        if (!appendDataObject(array, d, "data helper"_s))
            return false;
    }
    [dict.get() setObject:array.get() forKey:key];
    return true;
}

RetainPtr<SecTrustRef> CoreIPCSecTrust::createSecTrust() const
{
    if (!m_data)
        return { nullptr };

    auto dict = adoptNS([[NSMutableDictionary alloc] initWithCapacity:5]);

    RetainPtr<NSNumber> result;

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    switch (m_data->result) {
    case CoreIPCSecTrustResult::Invalid:
        result = @(kSecTrustResultInvalid);
        break;
    case CoreIPCSecTrustResult::Proceed:
        result = @(kSecTrustResultProceed);
        break;
    case CoreIPCSecTrustResult::Confirm:
        result = @(kSecTrustResultConfirm); // Deprecated
        break;
    case CoreIPCSecTrustResult::Deny:
        result = @(kSecTrustResultDeny);
        break;
    case CoreIPCSecTrustResult::Unspecified:
        result = @(kSecTrustResultUnspecified);
        break;
    case CoreIPCSecTrustResult::RecoverableTrustFailure:
        result = @(kSecTrustResultRecoverableTrustFailure);
        break;
    case CoreIPCSecTrustResult::FatalTrustFailure:
        result = @(kSecTrustResultFatalTrustFailure);
        break;
    case CoreIPCSecTrustResult::OtherError:
        result = @(kSecTrustResultOtherError);
        break;
    default:
        result = @(kSecTrustResultInvalid);
        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrustData had an unexpected value");
        ASSERT_NOT_REACHED();
        break;
    }
ALLOW_DEPRECATED_DECLARATIONS_END
    [dict setObject:result.get() forKey:@"result"];

    [dict setObject:@(m_data->anchorsOnly) forKey:@"anchorsOnly"];
    [dict setObject:@(m_data->keychainsAllowed) forKey:@"keychainsAllowed"];

    if (!m_data->certificates.isEmpty()) {
        RetainPtr certificates = adoptNS([[NSMutableArray alloc] initWithCapacity:m_data->certificates.size()]);
        for (const CoreIPCData& cert : borrow(m_data->certificates)) {
            if (!appendDataObject(certificates, cert, "certificates"_s))
                return { nullptr };
        }
        [dict setObject:certificates.get() forKey:@"certificates"];
    }

    if (!m_data->chain.isEmpty()) {
        RetainPtr chain = adoptNS([[NSMutableArray alloc] initWithCapacity:m_data->chain.size()]);
        for (const CoreIPCData& cert : borrow(m_data->chain)) {
            if (!appendDataObject(chain, cert, "chain"_s))
                return { nullptr };
        }
        [dict setObject:chain.get() forKey:@"chain"];
    }

    if (!m_data->policies.isEmpty()) {
        RetainPtr policies = adoptNS([[NSMutableArray alloc] initWithCapacity:m_data->policies.size()]);

        for (const auto& policyType : borrow(m_data->policies)) {
            if (!policyType.isEmpty()) {
                RetainPtr policy = adoptNS([[NSMutableDictionary alloc] initWithCapacity:policyType.size()]);
                for (const auto& typeValue : borrow(policyType)) {
                    RetainPtr<id> value;
                    RetainPtr<NSString> key = typeValue.first.toID();
                    WTF::switchOn(typeValue.second,
                        [&] (const CoreIPCString& s) {
                            value = s.toID();
                        },
                        [&] (const CoreIPCSecTrustData::PolicyOption& options) {
                            value = createPolicyDictionary(options);
                        }
                    );
                    if (!value)
                        return { nullptr };
                    [policy setObject:value.get() forKey:key.get()];
                }
                [policies addObject:policy.get()];
            }
        }
        [dict setObject:policies.get() forKey:@"policies"];
    }

    if (!m_data->details.isEmpty()) {
        RetainPtr details = adoptNS([[NSMutableArray alloc] initWithCapacity:m_data->details.size()]);
        for (const auto& detail : borrow(m_data->details)) {
            RetainPtr d = adoptNS([[NSMutableDictionary alloc] initWithCapacity:detail.size()]);
            for (const auto& item : borrow(detail))
                [d setObject:@(item.second) forKey:item.first.toID().get()];
            [details addObject:d.get()];
        }
        [dict setObject:details.get() forKey:@"details"];
    }

    if (m_data->verifyDate)
        [dict setObject:m_data->verifyDate->toID().get() forKey:@"verifyDate"];

    if (m_data->info) {
        RetainPtr info = adoptNS([[NSMutableDictionary alloc] initWithCapacity:m_data->info->size()]);
        for (const auto& pair : borrow(*m_data->info)) {
            RetainPtr<id> value;
            auto key = pair.first.toID();
            WTF::switchOn(pair.second,
                [&] (const bool& b) {
                    value = @(b);
                },
                [&] (const CoreIPCDate& d) {
                    value = d.toID();
                },
                [&] (const CoreIPCString& s) {
                    value = s.toID();
                },
                [&] (const CoreIPCSecTrustData::RevocationInfoArray& revocationInfoArray) {
                    RetainPtr array = adoptNS([[NSMutableArray alloc] initWithCapacity:revocationInfoArray.size()]);
                    for (const auto& revocationEntry : borrow(revocationInfoArray)) {
                        RetainPtr entryDict = adoptNS([[NSMutableDictionary alloc] initWithCapacity:revocationEntry.size()]);
                        for (const auto& entryPair : borrow(revocationEntry)) {
                            RetainPtr subDict = adoptNS([[NSMutableDictionary alloc] initWithCapacity:entryPair.second.size()]);
                            for (const auto& subPair : borrow(entryPair.second)) {
                                RetainPtr<id> subValue;
                                WTF::switchOn(subPair.second,
                                    [&] (const bool& b) {
                                        subValue = @(b);
                                    },
                                    [&] (const CoreIPCNumber& n) {
                                        subValue = n.toID();
                                    },
                                    [&] (const CoreIPCData& d) {
                                        subValue = d.toID();
                                    },
                                    [&] (const CoreIPCDate& d) {
                                        subValue = d.toID();
                                    }
                                );
                                if (!subValue) {
                                    RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust had a null value in 'info' revocation sub-dictionary");
                                    return;
                                }
                                [subDict setObject:subValue.get() forKey:subPair.first.toID().get()];
                            }
                            [entryDict setObject:subDict.get() forKey:entryPair.first.toID().get()];
                        }
                        [array addObject:entryDict.get()];
                    }
                    value = array;
                },
                [&] (const CoreIPCSecTrustData::InfoSubDict& infoSubDict) {
                    RetainPtr subDict = adoptNS([[NSMutableDictionary alloc] initWithCapacity:infoSubDict.size()]);
                    for (const auto& subPair : borrow(infoSubDict))
                        [subDict setObject:@(subPair.second) forKey:subPair.first.toID().get()];
                    value = subDict;
                }
            );
            if (!value)
                return { nullptr };
            [info setObject:value.get() forKey:key.get()];
        }
        [dict setObject:info.get() forKey:@"info"];
    }

    // Not short circuited, so every helper still runs and logs, as before.
    bool addedOptionalData = true;
    addedOptionalData &= addToDictFromOptionalDataHelper(m_data->responses, dict, @"responses");
    addedOptionalData &= addToDictFromOptionalDataHelper(m_data->scts, dict, @"scts");
    addedOptionalData &= addToDictFromOptionalDataHelper(m_data->anchors, dict, @"anchors");
    addedOptionalData &= addToDictFromOptionalDataHelper(m_data->trustedLogs, dict, @"trustedLogs");
    if (!addedOptionalData)
        return { nullptr };

    if (m_data->exceptions) {
        RetainPtr exceptions = adoptNS([[NSMutableArray alloc] initWithCapacity:m_data->exceptions->size()]);
        for (const auto& e : borrow(*m_data->exceptions)) {
            RetainPtr exception = adoptNS([[NSMutableDictionary alloc] initWithCapacity:e.size()]);
            for (const auto& item : borrow(e)) {
                RetainPtr<id> value;
                RetainPtr<NSString> key = item.first.toID();
                WTF::switchOn(item.second,
                    [&] (const bool& b) {
                        value = @(b);
                    },
                    [&] (const CoreIPCData& d) {
                        value = d.toID();
                    },
                    [&] (const CoreIPCNumber& n) {
                        value = n.toID();
                    }
                );
                if (!value) {
                    RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust had a null value in 'exceptions'");
                    return { nullptr };
                }
                [exception setObject:value.get() forKey:key.get()];
            }
            [exceptions addObject:exception.get()];
        }
        [dict setObject:exceptions.get() forKey:@"exceptions"];
    }

    RetainPtr<SecTrustRef> trust;
    RetainPtr<CFErrorRef> error;
    {
        // FIXME: The Security framework API is missing the `CF_RETURNS_RETAINED` annotation (rdar://161546781).
        CFErrorRef rawError = NULL;
        trust = adoptCF(SecTrustCreateFromPropertyListRepresentation(dict.get(), &rawError));
        SUPPRESS_RETAINPTR_CTOR_ADOPT error = adoptCF(rawError);
    }
    if (error) {
        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust error creating trust object");
        return { nullptr };
    }
    return trust;
}

#undef RETURN_IF_OPTIONAL_ERROR

} // namespace WebKit

#endif
