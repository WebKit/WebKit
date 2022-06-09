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
#include "ResourceRequest.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/URL.h>
#include <wtf/URLParser.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore::ContentExtensions {

static void append(Vector<uint8_t>& vector, size_t length)
{
    RELEASE_ASSERT(length <= std::numeric_limits<uint32_t>::max());
    uint32_t integer = length;
    vector.append(Span<const uint8_t> { reinterpret_cast<const uint8_t*>(&integer), sizeof(integer) });
}

static void uncheckedAppend(Vector<uint8_t>& vector, size_t length)
{
    RELEASE_ASSERT(length <= std::numeric_limits<uint32_t>::max());
    uint32_t integer = length;
    vector.uncheckedAppend(Span<const uint8_t> { reinterpret_cast<const uint8_t*>(&integer), sizeof(integer) });
}

static void append(Vector<uint8_t>& vector, const CString& string)
{
    vector.append(Span<const uint8_t> { reinterpret_cast<const uint8_t*>(string.data()), string.length() });
}

static void uncheckedAppend(Vector<uint8_t>& vector, const CString& string)
{
    vector.uncheckedAppend(Span<const uint8_t> { reinterpret_cast<const uint8_t*>(string.data()), string.length() });
}

static size_t deserializeLength(Span<const uint8_t> span, size_t offset)
{
    RELEASE_ASSERT(span.size() >= offset + sizeof(uint32_t));
    return *reinterpret_cast<const uint32_t*>(span.data() + offset);
}

static String deserializeUTF8String(Span<const uint8_t> span, size_t offset, size_t length)
{
    RELEASE_ASSERT(span.size() >= offset + length);
    return String::fromUTF8(span.data() + offset, length);
}

static void writeLengthToVectorAtOffset(Vector<uint8_t>& vector, size_t offset)
{
    auto length = vector.size() - offset;
    RELEASE_ASSERT(length <= std::numeric_limits<uint32_t>::max());
    uint32_t integer = length;
    RELEASE_ASSERT(vector.size() >= offset + sizeof(uint32_t));
    RELEASE_ASSERT(!*reinterpret_cast<uint32_t*>(vector.data() + offset));
    *reinterpret_cast<uint32_t*>(vector.data() + offset) = integer;
}

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
    return other.hashTableType == this->hashTableType
        && other.requestHeaders == this->requestHeaders
        && other.responseHeaders == this->responseHeaders;
}

void ModifyHeadersAction::serialize(Vector<uint8_t>& vector) const
{
    auto beginIndex = vector.size();
    append(vector, 0);
    auto requestHeadersLengthIndex = vector.size();
    append(vector, 0);
    for (auto& headerInfo : requestHeaders)
        headerInfo.serialize(vector);
    writeLengthToVectorAtOffset(vector, requestHeadersLengthIndex);
    for (auto& headerInfo : responseHeaders)
        headerInfo.serialize(vector);
    writeLengthToVectorAtOffset(vector, beginIndex);
}

ModifyHeadersAction ModifyHeadersAction::deserialize(Span<const uint8_t> span)
{
    auto serializedLength = deserializeLength(span, 0);
    auto requestHeadersLength = deserializeLength(span, sizeof(uint32_t));
    size_t progress = sizeof(uint32_t) * 2;
    Vector<ModifyHeaderInfo> requestHeaders;
    while (progress < requestHeadersLength + sizeof(uint32_t)) {
        auto subspan = span.subspan(progress);
        progress += ModifyHeaderInfo::serializedLength(subspan);
        requestHeaders.append(ModifyHeaderInfo::deserialize(subspan));
    }
    RELEASE_ASSERT(progress == requestHeadersLength + sizeof(uint32_t));
    Vector<ModifyHeaderInfo> responseHeaders;
    while (progress < serializedLength) {
        auto subspan = span.subspan(progress);
        progress += ModifyHeaderInfo::serializedLength(subspan);
        responseHeaders.append(ModifyHeaderInfo::deserialize(subspan));
    }
    RELEASE_ASSERT(progress == serializedLength);
    return { WTFMove(requestHeaders), WTFMove(responseHeaders) };
}

size_t ModifyHeadersAction::serializedLength(Span<const uint8_t> span)
{
    return deserializeLength(span, 0);
}

void ModifyHeadersAction::applyToRequest(ResourceRequest& request)
{
    for (auto& info : requestHeaders)
        info.applyToRequest(request);
}

void ModifyHeadersAction::ModifyHeaderInfo::applyToRequest(ResourceRequest& request)
{
    std::visit(WTF::makeVisitor([&] (const AppendOperation& operation) {
        auto existingValue = request.httpHeaderField(operation.header);
        if (existingValue.isEmpty())
            request.setHTTPHeaderField(operation.header, operation.value);
        else
            request.setHTTPHeaderField(operation.header, makeString(existingValue, "; ", operation.value));
    }, [&] (const SetOperation& operation) {
        request.setHTTPHeaderField(operation.header, operation.value);
    }, [&] (const RemoveOperation& operation) {
        request.removeHTTPHeaderField(operation.header);
    }), operation);
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

void ModifyHeadersAction::ModifyHeaderInfo::serialize(Vector<uint8_t>& vector) const
{
    auto beginIndex = vector.size();
    append(vector, 0);
    vector.append(operation.index());
    std::visit(WTF::makeVisitor([&] (const RemoveOperation& operation) {
        append(vector, operation.header.utf8());
    }, [&] (const auto& operation) {
        auto valueUTF8 = operation.value.utf8();
        append(vector, valueUTF8.length());
        append(vector, operation.header.utf8());
        append(vector, valueUTF8);
    }), operation);
    writeLengthToVectorAtOffset(vector, beginIndex);
}

auto ModifyHeadersAction::ModifyHeaderInfo::deserialize(Span<const uint8_t> span) -> ModifyHeaderInfo
{
    constexpr auto headerSize = sizeof(uint32_t) + sizeof(uint8_t);
    RELEASE_ASSERT(span.size() >= headerSize);
    auto serializedLength = deserializeLength(span, 0);
    return { [&] () -> OperationVariant {
        switch (auto index = span[sizeof(uint32_t)]) {
        case WTF::alternativeIndexV<RemoveOperation, OperationVariant>:
            return RemoveOperation { deserializeUTF8String(span, headerSize, serializedLength - headerSize) };
        case WTF::alternativeIndexV<AppendOperation, OperationVariant>:
        case WTF::alternativeIndexV<SetOperation, OperationVariant>: {
            auto valueLength = deserializeLength(span, headerSize);
            auto headerLength = serializedLength - headerSize - valueLength - sizeof(uint32_t);
            auto header = deserializeUTF8String(span, headerSize + sizeof(uint32_t), headerLength);
            auto value = deserializeUTF8String(span, headerSize + sizeof(uint32_t) + headerLength, valueLength);
            if (index == WTF::alternativeIndexV<AppendOperation, OperationVariant>)
                return AppendOperation { WTFMove(header), WTFMove(value) };
            return SetOperation { WTFMove(header), WTFMove(value) };
        }

        }
        RELEASE_ASSERT_NOT_REACHED();
    }() };
}

size_t ModifyHeadersAction::ModifyHeaderInfo::serializedLength(Span<const uint8_t> span)
{
    return deserializeLength(span, 0);
}

Expected<RedirectAction, std::error_code> RedirectAction::parse(const JSON::Object& redirectObject)
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
        auto parsedTransform = URLTransformAction::parse(*transform);
        if (!parsedTransform)
            return makeUnexpected(parsedTransform.error());
        return RedirectAction { WTFMove(*parsedTransform) };
    }

    if (auto urlString = redirect->getString("url"); !!urlString) {
        auto url = URL(URL(), urlString);
        if (!url.isValid())
            return makeUnexpected(ContentExtensionError::JSONRedirectURLInvalid);
        if (url.protocolIsJavaScript())
            return makeUnexpected(ContentExtensionError::JSONRedirectToJavaScriptURL);
        return RedirectAction { URLAction { WTFMove(urlString) } };
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
    return other.hashTableType == this->hashTableType
        && other.action == this->action;
}

void RedirectAction::serialize(Vector<uint8_t>& vector) const
{
    auto beginIndex = vector.size();
    append(vector, 0);
    vector.append(action.index());
    std::visit(WTF::makeVisitor([&](const ExtensionPathAction& action) {
        append(vector, action.extensionPath.utf8());
    }, [&](const RegexSubstitutionAction& action) {
        append(vector, action.regexSubstitution.utf8());
    }, [&](const URLTransformAction& action) {
        action.serialize(vector);
    }, [&](const URLAction& action) {
        append(vector, action.url.utf8());
    }), action);
    writeLengthToVectorAtOffset(vector, beginIndex);
}

RedirectAction RedirectAction::deserialize(Span<const uint8_t> span)
{
    constexpr auto headerSize = sizeof(uint32_t) + sizeof(uint8_t);
    auto stringLength = deserializeLength(span, 0) - headerSize;
    RELEASE_ASSERT(span.size() >= headerSize);
    return { [&] () -> ActionVariant {
        switch (span[sizeof(uint32_t)]) {
        case WTF::alternativeIndexV<ExtensionPathAction, ActionVariant>:
            return ExtensionPathAction { deserializeUTF8String(span, headerSize, stringLength) };
        case WTF::alternativeIndexV<RegexSubstitutionAction, ActionVariant>:
            return RegexSubstitutionAction { deserializeUTF8String(span, headerSize, stringLength) };
        case WTF::alternativeIndexV<URLTransformAction, ActionVariant>:
            return URLTransformAction::deserialize(span.subspan(headerSize));
        case WTF::alternativeIndexV<URLAction, ActionVariant>:
            return URLAction { deserializeUTF8String(span, headerSize, stringLength) };
        }
        RELEASE_ASSERT_NOT_REACHED();
    }() };
}

size_t RedirectAction::serializedLength(Span<const uint8_t> span)
{
    return deserializeLength(span, 0);
}

void RedirectAction::applyToRequest(ResourceRequest& request, const URL& extensionBaseURL)
{
    std::visit(WTF::makeVisitor([&](const ExtensionPathAction& action) {
        auto url = extensionBaseURL;
        url.setPath(action.extensionPath);
        request.setURL(WTFMove(url));
    }, [&] (const RegexSubstitutionAction&) {
        // FIXME: Implement, ideally in a way that doesn't require making a new VM and global object for each redirect operation.
    }, [&] (const URLTransformAction& action) {
        auto url = request.url();
        action.applyToURL(url);
        request.setURL(WTFMove(url));
    }, [&] (const URLAction& action) {
        request.setURL(URL(URL(), action.url));
    }), action);
}

auto RedirectAction::URLTransformAction::parse(const JSON::Object& transform) -> Expected<URLTransformAction, std::error_code>
{
    URLTransformAction action;
    if (auto fragment = transform.getString("fragment"); !!fragment) {
        if (!fragment.isEmpty() && !fragment.startsWith('#'))
            return makeUnexpected(ContentExtensionError::JSONRedirectInvalidFragment);
        action.fragment = WTFMove(fragment);
    }
    action.host = transform.getString("host");
    action.password = transform.getString("password");
    action.path = transform.getString("path");
    auto port = transform.getString("port");
    if (!!port) {
        if (port.isEmpty())
            action.port = { std::optional<uint16_t> { } };
        else {
            auto parsedPort = parseInteger<uint16_t>(StringView(port));
            if (!parsedPort)
                return makeUnexpected(ContentExtensionError::JSONRedirectInvalidPort);
            action.port = { parsedPort };
        }
    }

    if (auto uncanonicalizedScheme = transform.getString("scheme"); !!uncanonicalizedScheme) {
        auto scheme = WTF::URLParser::maybeCanonicalizeScheme(uncanonicalizedScheme);
        if (!scheme)
            return makeUnexpected(ContentExtensionError::JSONRedirectURLSchemeInvalid);
        if (scheme == "javascript")
            return makeUnexpected(ContentExtensionError::JSONRedirectToJavaScriptURL);
        action.scheme = WTFMove(*scheme);
    }
    action.username = transform.getString("username");
    if (auto queryTransform = transform.getObject("query-transform")) {
        auto parsedQueryTransform = QueryTransform::parse(*queryTransform);
        if (!parsedQueryTransform)
            return makeUnexpected(parsedQueryTransform.error());
        action.queryTransform = *parsedQueryTransform;
    } else {
        auto query = transform.getString("query");
        if (!query.isEmpty() && !query.startsWith('?'))
            return makeUnexpected(ContentExtensionError::JSONRedirectInvalidQuery);
        action.queryTransform = WTFMove(query);
    }
    return action;
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

void RedirectAction::URLTransformAction::serialize(Vector<uint8_t>& vector) const
{
    uint8_t hasFragment = !!fragment;
    uint8_t hasHost = !!host;
    uint8_t hasPassword = !!password;
    uint8_t hasPath = !!path;
    uint8_t hasPort = !!port;
    uint8_t hasScheme = !!scheme;
    uint8_t hasUsername = !!username;
    auto* queryString = std::get_if<String>(&queryTransform);
    auto queryStringUTF8 = queryString ? queryString->utf8() : CString();
    uint8_t hasQuery = !queryString || !!*queryString;

    auto fragmentUTF8 = fragment.utf8();
    auto hostUTF8 = host.utf8();
    auto passwordUTF8 = password.utf8();
    auto pathUTF8 = path.utf8();
    auto schemeUTF8 = scheme.utf8();
    auto usernameUTF8 = username.utf8();

    vector.reserveCapacity(vector.size() + sizeof(uint32_t) + sizeof(uint8_t)
        + (hasFragment ? sizeof(uint32_t) + fragmentUTF8.length() : 0)
        + (hasHost ? sizeof(uint32_t) + hostUTF8.length() : 0)
        + (hasPassword ? sizeof(uint32_t) + passwordUTF8.length() : 0)
        + (hasPath ? sizeof(uint32_t) + pathUTF8.length() : 0)
        + (hasScheme ? sizeof(uint32_t) + schemeUTF8.length() : 0)
        + (hasUsername ? sizeof(uint32_t) + usernameUTF8.length() : 0)
        + (hasPort ? 1 + (*port ? sizeof(uint16_t) : 0) : 0)
        + (hasQuery ? 1 + (queryString ? sizeof(uint32_t) + queryStringUTF8.length() : 0) : 0));

    auto beginIndex = vector.size();
    uncheckedAppend(vector, 0);
    vector.uncheckedAppend(
        hasFragment << 7
        | hasHost << 6
        | hasPassword << 5
        | hasPath << 4
        | hasPort << 3
        | hasScheme << 2
        | hasUsername << 1
        | hasQuery << 0
    );
    auto uncheckedAppendLengthAndString = [&] (const CString& string) {
        uncheckedAppend(vector, string.length());
        uncheckedAppend(vector, string);
    };
    if (hasFragment)
        uncheckedAppendLengthAndString(fragmentUTF8);
    if (hasHost)
        uncheckedAppendLengthAndString(hostUTF8);
    if (hasPassword)
        uncheckedAppendLengthAndString(passwordUTF8);
    if (hasPath)
        uncheckedAppendLengthAndString(pathUTF8);
    if (hasScheme)
        uncheckedAppendLengthAndString(schemeUTF8);
    if (hasUsername)
        uncheckedAppendLengthAndString(usernameUTF8);
    if (hasPort) {
        vector.uncheckedAppend(!!*port);
        if (*port) {
            vector.uncheckedAppend(**port >> 0);
            vector.uncheckedAppend(**port >> 8);
        }
    }
    if (hasQuery) {
        vector.uncheckedAppend(queryTransform.index());
        std::visit(WTF::makeVisitor([&](const String&) {
            uncheckedAppend(vector, queryStringUTF8.length());
            uncheckedAppend(vector, queryStringUTF8);
        }, [&](const QueryTransform& transform) {
            transform.serialize(vector);
        }), queryTransform);
    }
    writeLengthToVectorAtOffset(vector, beginIndex);
}

auto RedirectAction::URLTransformAction::deserialize(Span<const uint8_t> span) -> URLTransformAction
{
    constexpr auto headerLength = sizeof(uint32_t) + sizeof(uint8_t);
    RELEASE_ASSERT(span.size() >= headerLength);
    uint8_t flags = span[sizeof(uint32_t)];

    bool hasFragment = flags & (1 << 7);
    bool hasHost = flags & (1 << 6);
    bool hasPassword = flags & (1 << 5);
    bool hasPath = flags & (1 << 4);
    bool hasPort = flags & (1 << 3);
    bool hasScheme = flags & (1 << 2);
    bool hasUsername = flags & (1 << 1);
    bool hasQuery = flags & (1 << 0);

    size_t progress = headerLength;
    auto deserializeString = [&] {
        auto stringLength = deserializeLength(span, progress);
        auto string = deserializeUTF8String(span, progress + sizeof(uint32_t), stringLength);
        progress += sizeof(uint32_t) + stringLength;
        return string;
    };
    auto fragment = hasFragment ? deserializeString() : String();
    auto host = hasHost ? deserializeString() : String();
    auto password = hasPassword ? deserializeString() : String();
    auto path = hasPath ? deserializeString() : String();
    auto scheme = hasScheme ? deserializeString() : String();
    auto username = hasUsername ? deserializeString() : String();
    std::optional<std::optional<uint16_t>> port;
    if (hasPort) {
        if (span[progress++]) {
            RELEASE_ASSERT(span.size() > progress + 1);
            uint16_t value = (span[progress] << 0) | (span[progress + 1] << 8);
            port = { { value } };
            progress += 2;
        } else
            port = { std::optional<uint16_t> { } };
    }
    QueryTransformVariant queryTransform;
    if (hasQuery) {
        RELEASE_ASSERT(span.size() > progress);
        uint8_t variantIndex = span[progress++];
        if (variantIndex == WTF::alternativeIndexV<String, QueryTransformVariant>) {
            auto queryStringLength = deserializeLength(span, progress);
            queryTransform = deserializeUTF8String(span, progress + sizeof(uint32_t), queryStringLength);
        } else if (variantIndex == WTF::alternativeIndexV<QueryTransform, QueryTransformVariant>)
            queryTransform = QueryTransform::deserialize(span.subspan(progress));
        else
            RELEASE_ASSERT_NOT_REACHED();
    }

    return {
        WTFMove(fragment),
        WTFMove(host),
        WTFMove(password),
        WTFMove(path),
        WTFMove(port),
        WTFMove(queryTransform),
        WTFMove(scheme),
        WTFMove(username)
    };
}

size_t RedirectAction::URLTransformAction::serializedLength(Span<const uint8_t> span)
{
    return deserializeLength(span, 0);
}

auto RedirectAction::URLTransformAction::QueryTransform::parse(const JSON::Object& queryTransform) -> Expected<QueryTransform, std::error_code>
{
    QueryTransform parsedQueryTransform;

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

    return parsedQueryTransform;
}

void RedirectAction::URLTransformAction::applyToURL(URL& url) const
{
    if (!!fragment) {
        if (fragment.isEmpty())
            url.removeFragmentIdentifier();
        else
            url.setFragmentIdentifier(StringView(fragment).substring(1));
    }
    if (!!host)
        url.setHost(host);
    if (!!password)
        url.setPassword(password);
    if (!!path)
        url.setPath(path);
    if (!!port)
        url.setPort(*port);
    std::visit(WTF::makeVisitor([&] (const String& query) {
        if (!query.isNull())
            url.setQuery(query.isEmpty() ? StringView() : StringView(query));
    }, [&] (const URLTransformAction::QueryTransform& transform) {
        transform.applyToURL(url);
    }), queryTransform);
    if (!!scheme)
        url.setProtocol(scheme);
    if (!!username)
        url.setUser(username);
}

void RedirectAction::URLTransformAction::QueryTransform::applyToURL(URL& url) const
{
    auto form = WTF::URLParser::parseURLEncodedForm(url.query());

    HashSet<String> keysToRemove;
    for (auto& key : removeParams)
        keysToRemove.add(key);
    form.removeAllMatching([&] (auto& keyValue) {
        return keysToRemove.contains(keyValue.key);
    });

    Vector<WTF::KeyValuePair<String, String>> keysToAdd;
    HashMap<String, String> keysToReplace;
    for (auto& keyValue : addOrReplaceParams) {
        if (keyValue.replaceOnly)
            keysToReplace.add(keyValue.key, keyValue.value);
        else
            keysToAdd.append({ keyValue.key, keyValue.value });
    }
    for (auto& keyValue : form) {
        auto iterator = keysToReplace.find(keyValue.key);
        if (iterator != keysToReplace.end())
            keyValue.value = iterator->value;
    }
    form.appendVector(WTFMove(keysToAdd));

    StringBuilder transformedQuery;
    for (auto& keyValue : form)
        transformedQuery.append(transformedQuery.isEmpty() ? "" : "&", keyValue.key, !!keyValue.value ? "=" : "", keyValue.value);
    url.setQuery(transformedQuery);
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

void RedirectAction::URLTransformAction::QueryTransform::serialize(Vector<uint8_t>& vector) const
{
    auto beginIndex = vector.size();
    append(vector, 0);
    auto keyValueBeginIndex = vector.size();
    append(vector, 0);
    for (auto& queryKeyValue : addOrReplaceParams)
        queryKeyValue.serialize(vector);
    writeLengthToVectorAtOffset(vector, keyValueBeginIndex);
    for (auto& string : removeParams) {
        auto utf8 = string.utf8();
        append(vector, utf8.length());
        append(vector, utf8);
    }
    writeLengthToVectorAtOffset(vector, beginIndex);
}

auto RedirectAction::URLTransformAction::QueryTransform::deserialize(Span<const uint8_t> span) -> QueryTransform
{
    auto serializedLength = deserializeLength(span, 0);
    auto keyValuesSerializedLength = deserializeLength(span, sizeof(uint32_t));
    Vector<QueryKeyValue> queryKeyValues;
    auto queryKeyValuesProgress = sizeof(uint32_t);
    while (queryKeyValuesProgress < keyValuesSerializedLength) {
        auto subspan = span.subspan(sizeof(uint32_t) + queryKeyValuesProgress);
        queryKeyValuesProgress += QueryKeyValue::serializedLength(subspan);
        queryKeyValues.append(QueryKeyValue::deserialize(subspan));
    }
    RELEASE_ASSERT(queryKeyValuesProgress == keyValuesSerializedLength);
    
    Vector<String> strings;
    auto stringsProgress = sizeof(uint32_t) + queryKeyValuesProgress;
    while (stringsProgress < serializedLength) {
        auto stringLength = deserializeLength(span, stringsProgress);
        strings.append(deserializeUTF8String(span, stringsProgress + sizeof(uint32_t), stringLength));
        stringsProgress += sizeof(uint32_t) + stringLength;
    }
    return { WTFMove(queryKeyValues), WTFMove(strings) };
}

size_t RedirectAction::URLTransformAction::QueryTransform::serializedLength(Span<const uint8_t> span)
{
    return deserializeLength(span, 0);
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

void RedirectAction::URLTransformAction::QueryTransform::QueryKeyValue::serialize(Vector<uint8_t>& vector) const
{
    constexpr auto headerLength = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(bool);
    // FIXME: All these allocations and copies aren't strictly necessary.
    // We would need a way to query exactly what the UTF-8 encoded length would be to remove them, though.
    auto keyUTF8 = key.utf8();
    auto valueUTF8 = value.utf8();
    auto serializedLength = headerLength + keyUTF8.length() + valueUTF8.length();
    vector.reserveCapacity(vector.size() + serializedLength);
    uncheckedAppend(vector, serializedLength);
    uncheckedAppend(vector, keyUTF8.length());
    vector.uncheckedAppend(replaceOnly);
    uncheckedAppend(vector, keyUTF8);
    uncheckedAppend(vector, valueUTF8);
}

auto RedirectAction::URLTransformAction::QueryTransform::QueryKeyValue::deserialize(Span<const uint8_t> span) -> QueryKeyValue
{
    // FIXME: Using null terminated strings would reduce the binary size considerably.
    // We would need to disallow null strings when parsing, though.
    constexpr auto headerLength = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(bool);
    RELEASE_ASSERT(span.size() >= headerLength);
    auto serializedLength = deserializeLength(span, 0);
    auto keyUTF8Length = deserializeLength(span, sizeof(uint32_t));
    bool replaceOnly = span[sizeof(uint32_t) + sizeof(uint32_t)];
    auto key = deserializeUTF8String(span, headerLength, keyUTF8Length);
    auto value = deserializeUTF8String(span, headerLength + keyUTF8Length, serializedLength - headerLength - keyUTF8Length);
    return { WTFMove(key), replaceOnly, WTFMove(value) };
}

size_t RedirectAction::URLTransformAction::QueryTransform::QueryKeyValue::serializedLength(Span<const uint8_t> span)
{
    return deserializeLength(span, 0);
}

} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
