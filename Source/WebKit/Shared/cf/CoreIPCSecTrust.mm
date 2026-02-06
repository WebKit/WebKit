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
#import <wtf/Compiler.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/MakeString.h>

namespace WebKit {


static bool arrayElementsTheSameType(NSArray *array, Class c)
{
    if (!array.count)
        return true;
    for (id element in array) {
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
    for (NSString *optionKey in policyOption) {
        if (![optionKey isKindOfClass:NSString.class])
            return makeString("optionKey was not an NSString"_s);
        CoreIPCString k { optionKey };
        id optionValue = [policyOption objectForKey:optionKey];

        CoreIPCSecTrust::PolicyOptionValueShape shape = CoreIPCSecTrust::detectPolicyOptionShape(optionValue);

        switch (shape) {
        case CoreIPCSecTrust::PolicyOptionValueShape::Invalid:
            return makeString("policy shape for key "_s, (String)optionKey, "is invalid"_s);
        case CoreIPCSecTrust::PolicyOptionValueShape::Bool: {
            if (![optionValue isKindOfClass:NSNumber.class])
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::Bool unexpected type for key "_s, (String)optionKey);
            NSNumber *value = optionValue;
            CoreIPCSecTrustData::PolicyVariant v = static_cast<bool>([value boolValue]);
            policyVector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            break;
        }
        case CoreIPCSecTrust::PolicyOptionValueShape::String: {
            if (![optionValue isKindOfClass:NSString.class])
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::String unexpected type for key "_s, (String)optionKey);
            NSString *value = optionValue;
            CoreIPCSecTrustData::PolicyVariant v = CoreIPCString(value);
            policyVector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            break;
        }
        case CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfNumbers: {
            if (![optionValue isKindOfClass:NSArray.class])
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfNumbers unexpected type for key "_s, (String)optionKey, " (expecting NSArray)"_s);
            NSArray* value = optionValue;
            if (!value.count)
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfNumbers array length 0 for key "_s, (String)optionKey);
            if (!arrayElementsTheSameType(value, NSNumber.class))
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfNumbers unexpected type for key "_s, (String)optionKey, " (expecting NSNumber)"_s);
            Vector<CoreIPCNumber> vector;
            vector.reserveCapacity(value.count);
            for (NSNumber *element in value) {
                CoreIPCNumber n { element };
                vector.append(WTF::move(n));
            }
            CoreIPCSecTrustData::PolicyVariant v = WTF::move(vector);
            policyVector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            break;
        }
        case CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfStrings: {
            if (![optionValue isKindOfClass:NSArray.class])
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfStrings unexpected type for key "_s, (String)optionKey, " (expecting NSArray)"_s);
            NSArray* value = optionValue;
            if (!value.count)
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfStrings array length 0 for key "_s, (String)optionKey);
            if (!arrayElementsTheSameType(value, NSString.class))
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfStrings unexpected type for key "_s, (String)optionKey, " (expecting NSString)"_s);
            Vector<CoreIPCString> vector;
            vector.reserveCapacity(value.count);
            for (NSString *element in value) {
                CoreIPCString s { element };
                vector.append(WTF::move(s));
            }
            CoreIPCSecTrustData::PolicyVariant v = WTF::move(vector);
            policyVector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            break;
        }
        case CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfData: {
            if (![optionValue isKindOfClass:NSArray.class])
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfData unexpected type for key %@ "_s, (String)optionKey, " (expecting NSArray)"_s);
            NSArray* value = optionValue;
            if (!value.count)
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfData array length 0 for key "_s, (String)optionKey);
            if (!arrayElementsTheSameType(value, NSData.class))
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfData unexpected type for key "_s, (String)optionKey, " (expecting NSData)"_s);
            Vector<CoreIPCData> vector;
            vector.reserveCapacity(value.count);
            for (NSData *element in value) {
                CoreIPCData d { element };
                vector.append(WTF::move(d));
            }
            CoreIPCSecTrustData::PolicyVariant v = WTF::move(vector);
            policyVector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            break;
        }
        case CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfArrayContainingDateOrNumber: {
            if (![optionValue isKindOfClass:NSArray.class])
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfArrayContainingDateOrNumber unexpected type for key "_s, (String)optionKey, " (expecting NSArray)"_s);
            NSArray *value = optionValue;
            if (!value.count)
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfArrayContainingDateOrNumber array length 0 for key "_s, (String)optionKey);
            if (!arrayElementsTheSameType(value, NSArray.class))
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfArrayContainingDateOrNumber unexpected type for key "_s, (String)optionKey, " (expecting NSArray)"_s);

            CoreIPCSecTrustData::PolicyArrayOfArrayContainingDateOrNumbers outerVector;
            outerVector.reserveCapacity(value.count);

            for (NSArray *secondLevelArray in value) {
                if (![secondLevelArray isKindOfClass:NSArray.class])
                    return makeString("CoreIPCSecTrust::PolicyOptionValueShape::ArrayOfArrayContainingDateOrNumber second level array unexpected type for key "_s, (String)optionKey);

                Vector<Variant<WebKit::CoreIPCNumber, WebKit::CoreIPCDate>> innerVector;
                innerVector.reserveCapacity(secondLevelArray.count);

                for (id element in secondLevelArray) {
                    if ([element isKindOfClass:NSNumber.class]) {
                        NSNumber *e = element;
                        Variant<WebKit::CoreIPCNumber, WebKit::CoreIPCDate> v = CoreIPCNumber(e);
                        innerVector.append(WTF::move(v));
                    } else if ([element isKindOfClass:NSDate.class]) {
                        NSDate *d = element;
                        Variant<WebKit::CoreIPCNumber, WebKit::CoreIPCDate> v = CoreIPCDate(d);
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
            if (![optionValue isKindOfClass:NSDictionary.class])
                return makeString("CoreIPCSecTrust::PolicyOptionValueShape::DictionaryValueIsNumber unexpected type for key "_s, (String)optionKey, " (expecting NSDictionary)"_s);
            NSDictionary *d = optionValue;
            CoreIPCSecTrustData::PolicyDictionaryValueIsNumber vector;
            vector.reserveCapacity(d.count);
            for (NSString* key in d) {
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
        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust optionalArrayOfDataHelper error: %s", optionalDataError.utf8().data()); \
        ASSERT_NOT_REACHED(); \
        return; \
    }

static String optionalArrayOfDataHelper(std::optional<Vector<CoreIPCData>>& toSet, RetainPtr<NSDictionary> dict, NSString* topLevelKey)
{
    if (![dict isKindOfClass:NSDictionary.class])
        return makeString("optionalArrayOfDataHelper was called with wrong argument"_s);
    RetainPtr<NSArray> array = [dict objectForKey:topLevelKey];
    if ([array isKindOfClass:NSArray.class]) {
        Vector<CoreIPCData> vector;
        vector.reserveCapacity([array count]);
        for (NSData* item in array.get()) {
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
    CFErrorRef error = nullptr;

    if (!trust)
        return;

    RetainPtr cfDictionary = adoptCF(dynamic_cf_cast<CFDictionaryRef>(SecTrustCopyPropertyListRepresentation(trust, &error)));
    if (!cfDictionary || error)
        return;
    RetainPtr dict = bridge_cast(cfDictionary.get());

    CoreIPCSecTrustData secTrustData { };

    RetainPtr<NSDate> verifyDate = [dict objectForKey:@"verifyDate"];
    if ([verifyDate isKindOfClass:NSDate.class]) {
        CoreIPCDate d { verifyDate.get() };
        secTrustData.verifyDate = WTF::move(d);
    }

    RetainPtr<NSNumber> result = [dict objectForKey:@"result"];
    if (![result isKindOfClass:NSNumber.class]) {
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
    RetainPtr<NSNumber> anchorsOnly = [dict objectForKey:@"anchorsOnly"];
    if (![anchorsOnly isKindOfClass:NSNumber.class]) {
        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'anchorsOnly' value is nil or not an NSNumber and not optional");
        return;
    }
    secTrustData.anchorsOnly = [anchorsOnly boolValue];

    RetainPtr<NSArray> certificates = [dict objectForKey:@"certificates"];
    if ([certificates isKindOfClass:NSArray.class]) {
        Vector<CoreIPCData> vector;
        vector.reserveCapacity([certificates count]);
        for (NSData* item in certificates.get()) {
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

    RetainPtr<NSArray> chain = [dict objectForKey:@"chain"];
    if ([chain isKindOfClass:NSArray.class]) {
        Vector<CoreIPCData> vector;
        vector.reserveCapacity([chain count]);
        for (NSData* item in chain.get()) {
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

    RetainPtr<NSArray> details = [dict objectForKey:@"details"];
    if ([details isKindOfClass:NSArray.class]) {
        Vector<CoreIPCSecTrustData::Detail> vector;
        vector.reserveCapacity([details count]);
        for (NSDictionary *detail in details.get()) {
            if (![detail isKindOfClass:NSDictionary.class]) {
                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'details' array contains unexpected type");
                ASSERT_NOT_REACHED();
                return;
            }
            CoreIPCSecTrustData::Detail d;
            d.reserveCapacity(detail.count);
            for (NSString *key in detail) {
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

    RetainPtr<NSDictionary> info = [dict objectForKey:@"info"];
    if ([info isKindOfClass:NSDictionary.class]) {
        CoreIPCSecTrustData::InfoType vector;
        vector.reserveCapacity([info count]);
        for (NSString *key in info.get()) {
            if (![key isKindOfClass:NSString.class]) {
                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'info' dictionary key contains unexpected type");
                ASSERT_NOT_REACHED();
                return;
            }
            CoreIPCString k { key };
            id value = [info objectForKey:key];
            if ([value isKindOfClass:NSDate.class]) {
                CoreIPCDate date { value };
                CoreIPCSecTrustData::InfoOption v = WTF::move(date);
                vector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            } else if ([value isKindOfClass:NSString.class]) {
                CoreIPCString s { value };
                CoreIPCSecTrustData::InfoOption v = WTF::move(s);
                vector.append(std::make_pair(WTF::move(k), WTF::move(v)));
            } else if ([value isKindOfClass:NSNumber.class]) {
                NSNumber *candidateBool = value;
                if ([candidateBool isEqualToNumber:@YES] || [candidateBool isEqualToNumber:@NO]) {
                    bool v = [candidateBool boolValue];
                    vector.append(std::make_pair(WTF::move(k), v));
                } else {
                    RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'info' dictionary value contains unexpected NSNumber value");
                    ASSERT_NOT_REACHED();
                    return;
                }
            } else if ([value isKindOfClass:NSArray.class]) {
                NSArray *revocationInfoArray = value;
                CoreIPCSecTrustData::RevocationInfoArray revocationInfo;
                revocationInfo.reserveCapacity([revocationInfoArray count]);

                for (NSDictionary *entry in revocationInfoArray) {
                    if (![entry isKindOfClass:NSDictionary.class]) {
                        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'RevocationInfo' array contains non-dictionary element");
                        ASSERT_NOT_REACHED();
                        return;
                    }

                    CoreIPCSecTrustData::RevocationInfoEntry revocationEntry;
                    revocationEntry.reserveCapacity([entry count]);

                    for (NSString *entryKey in entry) {
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

                        for (NSString *subKey in subDict) {
                            if (![subKey isKindOfClass:NSString.class]) {
                                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'RevocationInfo' sub-dictionary key is not a string");
                                ASSERT_NOT_REACHED();
                                return;
                            }

                            CoreIPCString subKeyString { subKey };
                            id subValue = [subDict objectForKey:subKey];

                            if ([subValue isKindOfClass:NSNumber.class]) {
                                NSNumber *number = subValue;
                                if ([number isEqualToNumber:@YES] || [number isEqualToNumber:@NO]) {
                                    CoreIPCSecTrustData::RevocationInfoSubDictValue v = [number boolValue];
                                    revocationSubDict.append(std::make_pair(WTF::move(subKeyString), WTF::move(v)));
                                } else {
                                    CoreIPCNumber n { number };
                                    CoreIPCSecTrustData::RevocationInfoSubDictValue v = WTF::move(n);
                                    revocationSubDict.append(std::make_pair(WTF::move(subKeyString), WTF::move(v)));
                                }
                            } else if ([subValue isKindOfClass:NSData.class]) {
                                CoreIPCData data { subValue };
                                CoreIPCSecTrustData::RevocationInfoSubDictValue v = WTF::move(data);
                                revocationSubDict.append(std::make_pair(WTF::move(subKeyString), WTF::move(v)));
                            } else if ([subValue isKindOfClass:NSDate.class]) {
                                CoreIPCDate date { subValue };
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
            } else {
                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'info' dictionary contains unexpected type");
                ASSERT_NOT_REACHED();
                return;
            }
        }
        secTrustData.info = WTF::move(vector);
    }

    RetainPtr<NSNumber> keychainsAllowed = [dict objectForKey:@"keychainsAllowed"];
    if (![keychainsAllowed isKindOfClass:NSNumber.class]) {
        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'keychainsAllowed' value is nil or not an NSNumber and not optional");
        return;
    }
    secTrustData.keychainsAllowed = [keychainsAllowed boolValue];

    RetainPtr<NSArray> policies = [dict objectForKey:@"policies"];
    if ([policies isKindOfClass:NSArray.class]) {
        Vector<CoreIPCSecTrustData::PolicyType> outerVector;
        outerVector.reserveCapacity([policies count]);
        for (NSDictionary *policy in policies.get()) {
            if (![policy isKindOfClass:NSDictionary.class]) {
                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust policy is not an NSDictionary");
                ASSERT_NOT_REACHED();
                return;
            }
            CoreIPCSecTrustData::PolicyType innerVector;
            innerVector.reserveCapacity(policy.count);
            for (NSString *key in policy) {
                if (![key isKindOfClass:NSString.class]) {
                    RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust policy key is not an NSString");
                    ASSERT_NOT_REACHED();
                    return;
                }
                id value = [policy objectForKey:key];
                if ([value isKindOfClass:NSString.class]) {
                    CoreIPCString k { key };
                    CoreIPCSecTrustData::PolicyValue v = CoreIPCString(value);
                    auto p = std::make_pair(WTF::move(k), WTF::move(v));
                    innerVector.append(WTF::move(p));
                } else if ([value isKindOfClass:NSDictionary.class]) {
                    CoreIPCSecTrustData::PolicyOption policyVector;
                    String error = updatePolicyVector((NSDictionary *)value, policyVector);
                    if (!error.isNull()) {
                        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust updatePolicyVector error %s", error.utf8().data());
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

    RetainPtr<NSArray> exceptions = [dict objectForKey:@"exceptions"];
    if ([exceptions isKindOfClass:NSArray.class]) {
        Vector<CoreIPCSecTrustData::ExceptionType> vector;
        vector.reserveCapacity([exceptions count]);
        for (NSDictionary *exception in exceptions.get()) {
            if (![exception isKindOfClass:NSDictionary.class]) {
                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'exceptions' array contains non NSDictionary member");
                ASSERT_NOT_REACHED();
                return;
            }
            CoreIPCSecTrustData::ExceptionType innerVector;
            innerVector.reserveCapacity([exception count]);
            for (NSString *key in exception) {
                if (![key isKindOfClass:NSString.class]) {
                    RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust 'exceptions' dictionary key is not an NSString");
                    ASSERT_NOT_REACHED();
                    return;
                }
                CoreIPCString k { key };
                id value = [exception objectForKey:key];
                if ([value isKindOfClass:NSData.class]) {
                    CoreIPCData data { value };
                    Variant<CoreIPCNumber, CoreIPCData, bool> v = WTF::move(data);
                    auto p = std::make_pair(WTF::move(k), WTF::move(v));
                    innerVector.append(WTF::move(p));
                } else if ([value isKindOfClass:NSNumber.class]) {
                    NSNumber *number = value;
                    if ([number isEqualToNumber:@YES] || [number isEqualToNumber:@NO]) {
                        bool v = [number boolValue];
                        auto p = std::make_pair(WTF::move(k), v);
                        innerVector.append(WTF::move(p));
                    } else {
                        CoreIPCNumber n { number };
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

static RetainPtr<NSDictionary> createPolicyDictionary(const CoreIPCSecTrustData::PolicyOption& options)
{
    RetainPtr<NSMutableDictionary> dict = adoptNS([[NSMutableDictionary alloc] initWithCapacity:options.size()]);
    for (const auto& item : options) {
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
                for (const auto& number : a)
                    [array addObject:number.toID().get()];
                value = array;
            },
            [&] (const CoreIPCSecTrustData::PolicyArrayOfStrings& a) {
                RetainPtr array = adoptNS([[NSMutableArray alloc] initWithCapacity:a.size()]);
                for (const auto& str : a)
                    [array addObject:str.toID().get()];
                value = array;
            },
            [&] (const CoreIPCSecTrustData::PolicyArrayOfData& a) {
                RetainPtr array = adoptNS([[NSMutableArray alloc] initWithCapacity:a.size()]);
                for (const auto& d : a) {
                    if (RetainPtr nsD = d.toID())
                        [array addObject:d.toID().get()];
                    else {
                        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrustData had an null value in policy dictionary");
                        ASSERT_NOT_REACHED();
                    }
                }
                value = array;
            },
            [&] (const CoreIPCSecTrustData::PolicyArrayOfArrayContainingDateOrNumbers& a) {
                RetainPtr outerArray = adoptNS([[NSMutableArray alloc] initWithCapacity:a.size()]);
                for (const Vector<Variant<CoreIPCNumber, CoreIPCDate>>& inner : a) {
                    RetainPtr innerArray = adoptNS([[NSMutableArray alloc] initWithCapacity:inner.size()]);
                    for (const Variant<CoreIPCNumber, CoreIPCDate>& v : inner) {
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
                for (const auto& i : a)
                    [d setObject:i.second.toID().get() forKey:i.first.toID().get()];
                value = d;
            }
        );
        [dict setObject:value.get() forKey:key.get()];
    }
    return dict;
}

static void addToDictFromOptionalDataHelper(const std::optional<Vector<CoreIPCData>>& opt, RetainPtr<NSMutableDictionary> dict, NSString* key)
{
    if (!opt)
        return;
    RetainPtr array = adoptNS([[NSMutableArray alloc] initWithCapacity:opt->size()]);
    for (const CoreIPCData& d : *opt) {
        if (RetainPtr nsD = d.toID())
            [array addObject:nsD.get()];
        else {
            RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrustData had an null value in data helper");
            ASSERT_NOT_REACHED();
        }
    }
    [dict.get() setObject:array.get() forKey:key];
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
        for (const CoreIPCData& cert : m_data->certificates) {
            if (RetainPtr nsCert = cert.toID())
                [certificates addObject:nsCert.get()];
            else {
                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrustData had an null value in certificates");
                ASSERT_NOT_REACHED();
            }
        }
        [dict setObject:certificates.get() forKey:@"certificates"];
    }

    if (!m_data->chain.isEmpty()) {
        RetainPtr chain = adoptNS([[NSMutableArray alloc] initWithCapacity:m_data->chain.size()]);
        for (const CoreIPCData& cert : m_data->chain) {
            if (RetainPtr nsCert = cert.toID())
                [chain addObject:nsCert.get()];
            else {
                RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrustData had an null value in chain");
                ASSERT_NOT_REACHED();
            }
        }
        [dict setObject:chain.get() forKey:@"chain"];
    }

    if (!m_data->policies.isEmpty()) {
        RetainPtr policies = adoptNS([[NSMutableArray alloc] initWithCapacity:m_data->policies.size()]);

        for (const auto& policyType : m_data->policies) {
            if (!policyType.isEmpty()) {
                RetainPtr policy = adoptNS([[NSMutableDictionary alloc] initWithCapacity:policyType.size()]);
                for (const auto& typeValue : policyType) {
                    RetainPtr<id> value;
                    RetainPtr<NSString> key = typeValue.first.toID();
                    WTF::switchOn(typeValue.second,
                        [&] (const CoreIPCString& s) {
                            value = s.toID();
                        },
                        [&] (const CoreIPCSecTrustData::PolicyOption& options) {
                            RetainPtr<NSDictionary> d = createPolicyDictionary(options);
                            value = d;
                        }
                    );
                    [policy setObject:value.get() forKey:key.get()];
                }
                [policies addObject:policy.get()];
            }
        }
        [dict setObject:policies.get() forKey:@"policies"];
    }

    if (!m_data->details.isEmpty()) {
        RetainPtr details = adoptNS([[NSMutableArray alloc] initWithCapacity:m_data->details.size()]);
        for (const auto& detail : m_data->details) {
            RetainPtr d = adoptNS([[NSMutableDictionary alloc] initWithCapacity:detail.size()]);
            for (const auto& item : detail)
                [d setObject:@(item.second) forKey:item.first.toID().get()];
            [details addObject:d.get()];
        }
        [dict setObject:details.get() forKey:@"details"];
    }

    if (m_data->verifyDate)
        [dict setObject:m_data->verifyDate->toID().get() forKey:@"verifyDate"];

    if (m_data->info) {
        RetainPtr info = adoptNS([[NSMutableDictionary alloc] initWithCapacity:m_data->info->size()]);
        for (const auto& pair : *m_data->info) {
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
                    for (const auto& revocationEntry : revocationInfoArray) {
                        RetainPtr entryDict = adoptNS([[NSMutableDictionary alloc] initWithCapacity:revocationEntry.size()]);
                        for (const auto& entryPair : revocationEntry) {
                            RetainPtr subDict = adoptNS([[NSMutableDictionary alloc] initWithCapacity:entryPair.second.size()]);
                            for (const auto& subPair : entryPair.second) {
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
                                [subDict setObject:subValue.get() forKey:subPair.first.toID().get()];
                            }
                            [entryDict setObject:subDict.get() forKey:entryPair.first.toID().get()];
                        }
                        [array addObject:entryDict.get()];
                    }
                    value = array;
                }
            );
            [info setObject:value.get() forKey:key.get()];
        }
        [dict setObject:info.get() forKey:@"info"];
    }

    addToDictFromOptionalDataHelper(m_data->responses, dict, @"responses");
    addToDictFromOptionalDataHelper(m_data->scts, dict, @"scts");
    addToDictFromOptionalDataHelper(m_data->anchors, dict, @"anchors");
    addToDictFromOptionalDataHelper(m_data->trustedLogs, dict, @"trustedLogs");

    if (m_data->exceptions) {
        RetainPtr exceptions = adoptNS([[NSMutableArray alloc] initWithCapacity:m_data->exceptions->size()]);
        for (const auto& e : *m_data->exceptions) {
            RetainPtr exception = adoptNS([[NSMutableDictionary alloc] initWithCapacity:e.size()]);
            for (const auto& item : e) {
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
                [exception setObject:value.get() forKey:key.get()];
            }
            [exceptions addObject:exception.get()];
        }
        [dict setObject:exceptions.get() forKey:@"exceptions"];
    }

    CFErrorRef error = nullptr;
    RetainPtr trust = adoptCF(SecTrustCreateFromPropertyListRepresentation(dict.get(), &error));
    if (error) {
        RELEASE_LOG_ERROR(IPC, "CoreIPCSecTrust error creating trust object");
        return { nullptr };
    }
    return trust;
}

#undef RETURN_IF_OPTIONAL_ERROR

} // namespace WebKit

#endif
