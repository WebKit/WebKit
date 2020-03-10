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
#include "CDMLogging.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDMKeySystemConfiguration.h"
#include "CDMMediaCapability.h"
#include "CDMRestrictions.h"
#include "JSMediaKeyEncryptionScheme.h"
#include "JSMediaKeyMessageEvent.h"
#include "JSMediaKeySessionType.h"
#include "JSMediaKeyStatusMap.h"
#include "JSMediaKeysRequirement.h"
#include <wtf/JSONValues.h>

namespace WebCore {

static Ref<JSON::Object> toJSONObject(const CDMMediaCapability& capability)
{
    auto object = JSON::Object::create();
    object->setString("contentType"_s, capability.contentType);
    object->setString("robustness"_s, capability.robustness);
    if (capability.encryptionScheme)
        object->setString("encryptionScheme"_s, convertEnumerationToString(capability.encryptionScheme.value()));
    return object;
}

static Ref<JSON::Object> toJSONObject(const CDMRestrictions& restrictions)
{
    auto object = JSON::Object::create();
    object->setBoolean("distinctiveIdentifierDenied"_s, restrictions.distinctiveIdentifierDenied);
    object->setBoolean("persistentStateDenied"_s, restrictions.persistentStateDenied);
    auto deniedSessionTypes = JSON::Array::create();
    for (auto type : restrictions.deniedSessionTypes)
        deniedSessionTypes->pushString(convertEnumerationToString(type));
    object->setArray("deniedSessionTypes"_s, WTFMove(deniedSessionTypes));
    return object;
}

static Ref<JSON::Object> toJSONObject(const CDMKeySystemConfiguration& configuration)
{
    auto object = JSON::Object::create();
    object->setString("label"_s, configuration.label);

    auto initDataTypes = JSON::Array::create();
    for (auto initDataType : configuration.initDataTypes)
        initDataTypes->pushString(initDataType);
    object->setArray("initDataTypes"_s, WTFMove(initDataTypes));

    auto audioCapabilities = JSON::Array::create();
    for (auto capability : configuration.audioCapabilities)
        audioCapabilities->pushObject(toJSONObject(capability));
    object->setArray("audioCapabilities"_s, WTFMove(audioCapabilities));

    auto videoCapabilities = JSON::Array::create();
    for (auto capability : configuration.videoCapabilities)
        videoCapabilities->pushObject(toJSONObject(capability));
    object->setArray("videoCapabilities"_s, WTFMove(videoCapabilities));

    object->setString("distinctiveIdentifier"_s, convertEnumerationToString(configuration.distinctiveIdentifier));
    object->setString("persistentState"_s, convertEnumerationToString(configuration.persistentState));

    auto sessionTypes = JSON::Array::create();
    for (auto type : configuration.sessionTypes)
        sessionTypes->pushString(convertEnumerationToString(type));
    object->setArray("sessionTypes"_s, WTFMove(sessionTypes));

    return object;
}

static String toJSONString(const CDMKeySystemConfiguration& configuration)
{
    return toJSONObject(configuration)->toJSONString();
}

static String toJSONString(const CDMMediaCapability& capability)
{
    return toJSONObject(capability)->toJSONString();
}

static String toJSONString(const CDMRestrictions& restrictions)
{
    return toJSONObject(restrictions)->toJSONString();
}

}

namespace WTF {

String LogArgument<WebCore::CDMKeySystemConfiguration>::toString(const WebCore::CDMKeySystemConfiguration& configuration)
{
    return toJSONString(configuration);
}

String LogArgument<WebCore::CDMMediaCapability>::toString(const WebCore::CDMMediaCapability& capability)
{
    return toJSONString(capability);
}

String LogArgument<WebCore::CDMRestrictions>::toString(const WebCore::CDMRestrictions& restrictions)
{
    return toJSONString(restrictions);
}

String LogArgument<WebCore::CDMEncryptionScheme>::toString(const WebCore::CDMEncryptionScheme& type)
{
    return convertEnumerationToString(type);
}

String LogArgument<WebCore::CDMKeyStatus>::toString(const WebCore::CDMKeyStatus& type)
{
    return convertEnumerationToString(type);
}

String LogArgument<WebCore::CDMMessageType>::toString(const WebCore::CDMMessageType& type)
{
    return convertEnumerationToString(type);
}

String LogArgument<WebCore::CDMRequirement>::toString(const WebCore::CDMRequirement& type)
{
    return convertEnumerationToString(type);
}

String LogArgument<WebCore::CDMSessionType>::toString(const WebCore::CDMSessionType& type)
{
    return convertEnumerationToString(type);
}

}

#endif // ENABLE(ENCRYPTED_MEDIA)
