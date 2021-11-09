/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "ContentExtensionActions.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "ContentExtensionError.h"
#include <wtf/CrossThreadCopier.h>

namespace WebCore::ContentExtensions {

Expected<ModifyHeadersAction, std::error_code> ModifyHeadersAction::parse(const JSON::Object& modifyHeaders)
{
    auto parseHeaders = [] (const JSON::Object& modifyHeaders, ASCIILiteral arrayName) -> Expected<Vector<ModifyHeaderInfo>, std::error_code> {
        auto value = modifyHeaders.getValue(arrayName);
        if (!value)
            return { };
        auto array = value->asArray();
        if (!array)
            return makeUnexpected(ContentExtensionError::JSONModifyHeadersNotArray);
        Vector<ModifyHeaderInfo> vector;
        vector.reserveInitialCapacity(array->length());
        for (auto& value : *array) {
            auto info = ModifyHeaderInfo::parse(value.get());
            if (!info)
                return makeUnexpected(info.error());
            vector.uncheckedAppend(WTFMove(*info));
        }
        return vector;
    };

    auto requestHeaders = parseHeaders(modifyHeaders, "request-headers"_s);
    if (!requestHeaders)
        return makeUnexpected(requestHeaders.error());

    auto responseHeaders = parseHeaders(modifyHeaders, "response-headers"_s);
    if (!responseHeaders)
        return makeUnexpected(responseHeaders.error());

    return ModifyHeadersAction { WTFMove(*requestHeaders), WTFMove(*responseHeaders) };
}

ModifyHeadersAction ModifyHeadersAction::isolatedCopy() const
{
    return {
        crossThreadCopy(requestHeaders),
        crossThreadCopy(responseHeaders)
    };
}

bool ModifyHeadersAction::operator==(const ModifyHeadersAction& other) const
{
    return other.requestHeaders == this->requestHeaders
        && other.responseHeaders == this->responseHeaders;
}

// FIXME: Implement serialization and deserialization of each of these types, then do their actions.
void ModifyHeadersAction::serialize(Vector<uint8_t>&) const
{
}

ModifyHeadersAction ModifyHeadersAction::deserialize(Span<const uint8_t>)
{
    return { };
}

size_t ModifyHeadersAction::serializedLength(Span<const uint8_t>)
{
    return 0;
}

auto ModifyHeadersAction::ModifyHeaderInfo::parse(const JSON::Value& infoValue) -> Expected<ModifyHeaderInfo, std::error_code>
{
    auto object = infoValue.asObject();
    if (!object)
        return makeUnexpected(ContentExtensionError::JSONModifyHeadersInfoNotADictionary);

    String operation = object->getString("operation");
    if (!operation)
        return makeUnexpected(ContentExtensionError::JSONModifyHeadersMissingOperation);

    String header = object->getString("header");
    if (!header)
        return makeUnexpected(ContentExtensionError::JSONModifyHeadersMissingHeader);

    String value = object->getString("value");

    if (operation == "set") {
        if (!value)
            return makeUnexpected(ContentExtensionError::JSONModifyHeadersMissingValue);
        return ModifyHeaderInfo { SetOperation { WTFMove(header), WTFMove(value) } };
    }
    if (operation == "append") {
        if (!value)
            return makeUnexpected(ContentExtensionError::JSONModifyHeadersMissingValue);
        return ModifyHeaderInfo { AppendOperation { WTFMove(header), WTFMove(value) } };
    }
    if (operation == "remove")
        return ModifyHeaderInfo { RemoveOperation { WTFMove(header) } };
    return makeUnexpected(ContentExtensionError::JSONModifyHeadersInvalidOperation);
}

auto ModifyHeadersAction::ModifyHeaderInfo::isolatedCopy() const -> ModifyHeaderInfo
{
    return {
        crossThreadCopy(operation)
    };
}

bool ModifyHeadersAction::ModifyHeaderInfo::operator==(const ModifyHeaderInfo& other) const
{
    return other.operation == this->operation;
}

void ModifyHeadersAction::ModifyHeaderInfo::serialize(Vector<uint8_t>&) const
{
}

auto ModifyHeadersAction::ModifyHeaderInfo::deserialize(Span<const uint8_t>) -> ModifyHeaderInfo
{
    return { };
}

size_t ModifyHeadersAction::ModifyHeaderInfo::serializedLength(Span<const uint8_t>)
{
    return 0;
}

Expected<RedirectAction, std::error_code> RedirectAction::parse(const JSON::Object& redirectObject, const HashSet<String>& allowedRedirectURLSchemes)
{
    auto redirect = redirectObject.getObject("redirect");
    if (!redirect)
        return makeUnexpected(ContentExtensionError::JSONRedirectMissing);

    if (auto extensionPath = redirect->getString("extension-path"); !!extensionPath) {
        if (!extensionPath.startsWith('/'))
            return makeUnexpected(ContentExtensionError::JSONRedirectExtensionPathDoesNotStartWithSlash);
        return RedirectAction { ExtensionPathAction { WTFMove(extensionPath) } };
    }

    if (auto regexSubstitution = redirect->getString("regex-substitution"); !!regexSubstitution)
        return RedirectAction { RegexSubstitutionAction { WTFMove(regexSubstitution) } };

    if (auto transform = redirect->getObject("transform")) {
        auto parsedTransform = URLTransformAction::parse(*transform, allowedRedirectURLSchemes);
        if (!parsedTransform)
            return makeUnexpected(parsedTransform.error());
        return RedirectAction { WTFMove(*parsedTransform) };
    }

    if (auto url = redirect->getString("url"); !!url) {
        if (!allowedRedirectURLSchemes.contains(URL(URL(), url).protocol().toStringWithoutCopying()))
            return makeUnexpected(ContentExtensionError::JSONRedirectURLSchemeNotAllowed);
        return RedirectAction { URLAction { WTFMove(url) } };
    }

    return makeUnexpected(ContentExtensionError::JSONRedirectInvalidType);
}

RedirectAction RedirectAction::isolatedCopy() const
{
    return {
        crossThreadCopy(action)
    };
}

bool RedirectAction::operator==(const RedirectAction& other) const
{
    return other.action == this->action;
}

void RedirectAction::serialize(Vector<uint8_t>&) const
{
}

RedirectAction RedirectAction::deserialize(Span<const uint8_t>)
{
    return { };
}

size_t RedirectAction::serializedLength(Span<const uint8_t>)
{
    return 0;
}

auto RedirectAction::URLTransformAction::parse(const JSON::Object& transform, const HashSet<String>& allowedRedirectURLSchemes) -> Expected<URLTransformAction, std::error_code>
{
    RedirectAction::URLTransformAction action;
    action.fragment = transform.getString("fragment");
    action.host = transform.getString("host");
    action.password = transform.getString("password");
    action.path = transform.getString("path");
    action.port = transform.getString("port");
    auto scheme = transform.getString("scheme");
    if (!!scheme && !allowedRedirectURLSchemes.contains(scheme))
        return makeUnexpected(ContentExtensionError::JSONRedirectURLSchemeNotAllowed);
    action.scheme = WTFMove(scheme);
    action.username = transform.getString("username");
    if (auto queryTransform = transform.getObject("query-transform")) {
        auto parsedQueryTransform = QueryTransform::parse(*queryTransform);
        if (!parsedQueryTransform)
            return makeUnexpected(parsedQueryTransform.error());
        action.queryTransform = *parsedQueryTransform;
    } else
        action.queryTransform = transform.getString("query");
    return WTFMove(action);
}

auto RedirectAction::URLTransformAction::isolatedCopy() const -> URLTransformAction
{
    return {
        crossThreadCopy(fragment),
        crossThreadCopy(host),
        crossThreadCopy(password),
        crossThreadCopy(path),
        crossThreadCopy(port),
        crossThreadCopy(queryTransform),
        crossThreadCopy(scheme),
        crossThreadCopy(username)
    };
}

bool RedirectAction::URLTransformAction::operator==(const URLTransformAction& other) const
{
    return other.fragment == this->fragment
        && other.host == this->host
        && other.password == this->password
        && other.path == this->path
        && other.port == this->port
        && other.queryTransform == this->queryTransform
        && other.scheme == this->scheme
        && other.username == this->username;
}

void RedirectAction::URLTransformAction::serialize(Vector<uint8_t>&) const
{
}

auto RedirectAction::URLTransformAction::deserialize(Span<const uint8_t>) -> URLTransformAction
{
    return { };
}

size_t RedirectAction::URLTransformAction::serializedLength(Span<const uint8_t>)
{
    return 0;
}

auto RedirectAction::URLTransformAction::QueryTransform::parse(const JSON::Object& queryTransform) -> Expected<QueryTransform, std::error_code>
{
    RedirectAction::URLTransformAction::QueryTransform parsedQueryTransform;

    if (auto removeParametersValue = queryTransform.getValue("remove-parameters")) {
        auto removeParametersArray = removeParametersValue->asArray();
        if (!removeParametersArray)
            return makeUnexpected(ContentExtensionError::JSONRemoveParametersNotStringArray);
        Vector<String> removeParametersVector;
        removeParametersVector.reserveInitialCapacity(removeParametersArray->length());
        for (auto& parameter : *removeParametersArray) {
            if (parameter.get().type() != JSON::Value::Type::String)
                return makeUnexpected(ContentExtensionError::JSONRemoveParametersNotStringArray);
            removeParametersVector.uncheckedAppend(parameter.get().asString());
        }
        parsedQueryTransform.removeParams = WTFMove(removeParametersVector);
    }

    if (auto addOrReplaceParametersValue = queryTransform.getValue("add-or-replace-parameters")) {
        auto addOrReplaceParametersArray = addOrReplaceParametersValue->asArray();
        if (!addOrReplaceParametersArray)
            return makeUnexpected(ContentExtensionError::JSONAddOrReplaceParametersNotArray);
        Vector<QueryKeyValue> keyValues;
        keyValues.reserveInitialCapacity(addOrReplaceParametersArray->length());
        for (auto& queryKeyValue : *addOrReplaceParametersArray) {
            auto keyValue = QueryKeyValue::parse(queryKeyValue.get());
            if (!keyValue)
                return makeUnexpected(keyValue.error());
            keyValues.uncheckedAppend(WTFMove(*keyValue));
        }
        parsedQueryTransform.addOrReplaceParams = WTFMove(keyValues);
    }

    return WTFMove(parsedQueryTransform);
}

auto RedirectAction::URLTransformAction::QueryTransform::isolatedCopy() const -> QueryTransform
{
    return {
        crossThreadCopy(addOrReplaceParams),
        crossThreadCopy(removeParams)
    };
}

bool RedirectAction::URLTransformAction::QueryTransform::operator==(const QueryTransform& other) const
{
    return other.addOrReplaceParams == this->addOrReplaceParams
        && other.removeParams == this->removeParams;
}

void RedirectAction::URLTransformAction::QueryTransform::serialize(Vector<uint8_t>&) const
{
}

auto RedirectAction::URLTransformAction::QueryTransform::deserialize(Span<const uint8_t>) -> QueryTransform
{
    return { };
}

size_t RedirectAction::URLTransformAction::QueryTransform::serializedLength(Span<const uint8_t>)
{
    return 0;
}

auto RedirectAction::URLTransformAction::QueryTransform::QueryKeyValue::parse(const JSON::Value& keyValueValue) -> Expected<QueryKeyValue, std::error_code>
{
    auto keyValue = keyValueValue.asObject();
    if (!keyValue)
        return makeUnexpected(ContentExtensionError::JSONAddOrReplaceParametersKeyValueNotADictionary);

    String key = keyValue->getString("key");
    if (!key)
        return makeUnexpected(ContentExtensionError::JSONAddOrReplaceParametersKeyValueMissingKeyString);

    String value = keyValue->getString("value");
    if (!value)
        return makeUnexpected(ContentExtensionError::JSONAddOrReplaceParametersKeyValueMissingValueString);

    bool replaceOnly { false };
    if (auto boolean = keyValue->getBoolean("replace-only"))
        replaceOnly = *boolean;

    return { { WTFMove(key), replaceOnly, WTFMove(value) } };
}

auto RedirectAction::URLTransformAction::QueryTransform::QueryKeyValue::isolatedCopy() const -> QueryKeyValue
{
    return {
        crossThreadCopy(key),
        crossThreadCopy(replaceOnly),
        crossThreadCopy(value)
    };
}

bool RedirectAction::URLTransformAction::QueryTransform::QueryKeyValue::operator==(const QueryKeyValue& other) const
{
    return other.key == this->key
        && other.replaceOnly == this->replaceOnly
        && other.value == this->value;
}

void RedirectAction::URLTransformAction::QueryTransform::QueryKeyValue::serialize(Vector<uint8_t>&) const
{
}

auto RedirectAction::URLTransformAction::QueryTransform::QueryKeyValue::deserialize(Span<const uint8_t>) -> QueryKeyValue
{
    return { };
}

size_t RedirectAction::URLTransformAction::QueryTransform::QueryKeyValue::serializedLength(Span<const uint8_t>)
{
    return 0;
}

} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
