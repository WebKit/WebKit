/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GeneratedWebKitSecureCoding.h"

#include "ArgumentCodersCocoa.h"
#if USE(AVFOUNDATION)
#include <pal/cocoa/AVFoundationSoftLink.h>
#endif
#if ENABLE(DATA_DETECTION)
#include <pal/cocoa/DataDetectorsCoreSoftLink.h>
#endif

namespace WebKit {

static RetainPtr<NSDictionary> dictionaryForWebKitSecureCodingType(id object)
{
    if (WebKit::CoreIPCSecureCoding::conformsToWebKitSecureCoding(object))
        return [object _webKitPropertyListData];

    auto archiver = adoptNS([WKKeyedCoder new]);
    [object encodeWithCoder:archiver.get()];
    return [archiver accumulatedDictionary];
}

#if USE(AVFOUNDATION)
CoreIPCAVOutputContext::CoreIPCAVOutputContext(AVOutputContext *object)
    : m_propertyList(dictionaryForWebKitSecureCodingType(object))
{
}

bool CoreIPCAVOutputContext::isValidDictionary(CoreIPCDictionary& dictionary)
{
    if (!dictionary.keyHasValueOfType("AVOutputContextSerializationKeyContextID"_s, IPC::NSType::String))
        return false;

    if (!dictionary.keyHasValueOfType("AVOutputContextSerializationKeyContextType"_s, IPC::NSType::String))
        return false;

    return true;
}

RetainPtr<id> CoreIPCAVOutputContext::toID() const
{
    if (![PAL::getAVOutputContextClass() instancesRespondToSelector:@selector(_initWithWebKitPropertyListData:)]) {
        auto unarchiver = adoptNS([[WKKeyedCoder alloc] initWithDictionary:m_propertyList.toID().get()]);
        return adoptNS([[PAL::getAVOutputContextClass() alloc] initWithCoder:unarchiver.get()]);
    }
    return adoptNS([[PAL::getAVOutputContextClass() alloc] _initWithWebKitPropertyListData:m_propertyList.toID().get()]);
}
#endif // USE(AVFOUNDATION)

CoreIPCNSSomeFoundationType::CoreIPCNSSomeFoundationType(NSSomeFoundationType *object)
    : m_propertyList([object _webKitPropertyListData])
{
}

bool CoreIPCNSSomeFoundationType::isValidDictionary(CoreIPCDictionary& dictionary)
{
    if (!dictionary.keyHasValueOfType("StringKey"_s, IPC::NSType::String))
        return false;

    if (!dictionary.keyHasValueOfType("NumberKey"_s, IPC::NSType::Number))
        return false;

    if (!dictionary.keyIsMissingOrHasValueOfType("OptionalNumberKey"_s, IPC::NSType::Number))
        return false;

    if (!dictionary.keyHasValueOfType("ArrayKey"_s, IPC::NSType::Array))
        return false;

    if (!dictionary.keyIsMissingOrHasValueOfType("OptionalArrayKey"_s, IPC::NSType::Array))
        return false;

    if (!dictionary.keyHasValueOfType("DictionaryKey"_s, IPC::NSType::Dictionary))
        return false;

    if (!dictionary.keyIsMissingOrHasValueOfType("OptionalDictionaryKey"_s, IPC::NSType::Dictionary))
        return false;

    return true;
}

RetainPtr<id> CoreIPCNSSomeFoundationType::toID() const
{
    RELEASE_ASSERT([NSSomeFoundationType instancesRespondToSelector:@selector(_initWithWebKitPropertyListData:)]);
    return adoptNS([[NSSomeFoundationType alloc] _initWithWebKitPropertyListData:m_propertyList.toID().get()]);
}

#if ENABLE(DATA_DETECTION)
CoreIPCDDScannerResult::CoreIPCDDScannerResult(DDScannerResult *object)
    : m_propertyList([object _webKitPropertyListData])
{
}

bool CoreIPCDDScannerResult::isValidDictionary(CoreIPCDictionary& dictionary)
{
    if (!dictionary.keyHasValueOfType("StringKey"_s, IPC::NSType::String))
        return false;

    if (!dictionary.keyHasValueOfType("NumberKey"_s, IPC::NSType::Number))
        return false;

    if (!dictionary.keyIsMissingOrHasValueOfType("OptionalNumberKey"_s, IPC::NSType::Number))
        return false;

    if (!dictionary.keyHasValueOfType("ArrayKey"_s, IPC::NSType::Array))
        return false;
    if (!dictionary.collectionValuesAreOfType("ArrayKey"_s, IPC::NSType::DDScannerResult))
        return false;

    if (!dictionary.keyIsMissingOrHasValueOfType("OptionalArrayKey"_s, IPC::NSType::Array))
        return false;
    if (!dictionary.collectionValuesAreOfType("OptionalArrayKey"_s, IPC::NSType::DDScannerResult))
        return false;

    if (!dictionary.keyHasValueOfType("DictionaryKey"_s, IPC::NSType::Dictionary))
        return false;
    if (!dictionary.collectionValuesAreOfType("DictionaryKey"_s, IPC::NSType::String, IPC::NSType::Number))
        return false;

    if (!dictionary.keyIsMissingOrHasValueOfType("OptionalDictionaryKey"_s, IPC::NSType::Dictionary))
        return false;
    if (!dictionary.collectionValuesAreOfType("OptionalDictionaryKey"_s, IPC::NSType::String, IPC::NSType::DDScannerResult))
        return false;

    return true;
}

RetainPtr<id> CoreIPCDDScannerResult::toID() const
{
    RELEASE_ASSERT([PAL::getDDScannerResultClass() instancesRespondToSelector:@selector(_initWithWebKitPropertyListData:)]);
    return adoptNS([[PAL::getDDScannerResultClass() alloc] _initWithWebKitPropertyListData:m_propertyList.toID().get()]);
}
#endif // ENABLE(DATA_DETECTION)

} // namespace WebKit
