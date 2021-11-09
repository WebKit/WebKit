/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if ENABLE(CONTENT_EXTENSIONS)

#include "ContentExtensionStringSerialization.h"
#include <wtf/JSONValues.h>

namespace WebCore::ContentExtensions {

struct Action;

using SerializedActionByte = uint8_t;

template<typename T> struct ActionWithoutMetadata {
    T isolatedCopy() const { return { }; }
    bool operator==(const T&) const { return true; }
    void serialize(Vector<uint8_t>&) const { }
    static T deserialize(Span<const uint8_t>) { return { }; }
    static size_t serializedLength(Span<const uint8_t>) { return 0; }
};

template<typename T> struct ActionWithStringMetadata {
    const String string;
    T isolatedCopy() const { return { { string.isolatedCopy() } }; }
    bool operator==(const T& other) const { return other.string == this->string; }
    void serialize(Vector<uint8_t>& vector) const { serializeString(vector, string); }
    static T deserialize(Span<const uint8_t> span) { return { { deserializeString(span) } }; }
    static size_t serializedLength(Span<const uint8_t> span) { return stringSerializedLength(span); }
};

struct BlockLoadAction : public ActionWithoutMetadata<BlockLoadAction> { };
struct BlockCookiesAction : public ActionWithoutMetadata<BlockCookiesAction> { };
struct CSSDisplayNoneSelectorAction : public ActionWithStringMetadata<CSSDisplayNoneSelectorAction> { };
struct NotifyAction : public ActionWithStringMetadata<NotifyAction> { };
struct IgnorePreviousRulesAction : public ActionWithoutMetadata<IgnorePreviousRulesAction> { };
struct MakeHTTPSAction : public ActionWithoutMetadata<MakeHTTPSAction> { };

struct ModifyHeadersAction {
    struct ModifyHeaderInfo {
        struct AppendOperation {
            String header;
            String value;

            AppendOperation isolatedCopy() const { return { header.isolatedCopy(), value.isolatedCopy() }; }
            bool operator==(const AppendOperation& other) const { return other.header == this->header && other.value == this->value; }
        };
        struct SetOperation {
            String header;
            String value;

            SetOperation isolatedCopy() const { return { header.isolatedCopy(), value.isolatedCopy() }; }
            bool operator==(const SetOperation& other) const { return other.header == this->header && other.value == this->value; }
        };
        struct RemoveOperation {
            String header;

            RemoveOperation isolatedCopy() const { return { header.isolatedCopy() }; }
            bool operator==(const RemoveOperation& other) const { return other.header == this->header; }
        };
        std::variant<AppendOperation, SetOperation, RemoveOperation> operation;

        static Expected<ModifyHeaderInfo, std::error_code> parse(const JSON::Value&);
        ModifyHeaderInfo isolatedCopy() const;
        bool operator==(const ModifyHeaderInfo&) const;
        void serialize(Vector<uint8_t>&) const;
        static ModifyHeaderInfo deserialize(Span<const uint8_t>);
        static size_t serializedLength(Span<const uint8_t>);
    };

    Vector<ModifyHeaderInfo> requestHeaders;
    Vector<ModifyHeaderInfo> responseHeaders;

    static Expected<ModifyHeadersAction, std::error_code> parse(const JSON::Object&);
    ModifyHeadersAction isolatedCopy() const;
    WEBCORE_EXPORT bool operator==(const ModifyHeadersAction&) const;
    void serialize(Vector<uint8_t>&) const;
    static ModifyHeadersAction deserialize(Span<const uint8_t>);
    static size_t serializedLength(Span<const uint8_t>);
};

struct RedirectAction {
    struct ExtensionPathAction {
        String extensionPath;

        ExtensionPathAction isolatedCopy() const { return { extensionPath.isolatedCopy() }; }
        bool operator==(const ExtensionPathAction& other) const { return other.extensionPath == this->extensionPath; }
    };
    struct RegexSubstitutionAction {
        String regexSubstitution;

        RegexSubstitutionAction isolatedCopy() const { return { regexSubstitution.isolatedCopy() }; }
        bool operator==(const RegexSubstitutionAction& other) const { return other.regexSubstitution == this->regexSubstitution; }
    };
    struct URLTransformAction {
        struct QueryTransform {
            struct QueryKeyValue {
                String key;
                bool replaceOnly { false };
                String value;

                static Expected<QueryKeyValue, std::error_code> parse(const JSON::Value&);
                QueryKeyValue isolatedCopy() const;
                bool operator==(const QueryKeyValue&) const;
                void serialize(Vector<uint8_t>&) const;
                static QueryKeyValue deserialize(Span<const uint8_t>);
                static size_t serializedLength(Span<const uint8_t>);
            };

            Vector<QueryKeyValue> addOrReplaceParams;
            Vector<String> removeParams;

            static Expected<QueryTransform, std::error_code> parse(const JSON::Object&);
            QueryTransform isolatedCopy() const;
            bool operator==(const QueryTransform&) const;
            void serialize(Vector<uint8_t>&) const;
            static QueryTransform deserialize(Span<const uint8_t>);
            static size_t serializedLength(Span<const uint8_t>);
        };

        String fragment;
        String host;
        String password;
        String path;
        String port;
        std::variant<String, QueryTransform> queryTransform;
        String scheme;
        String username;

        static Expected<URLTransformAction, std::error_code> parse(const JSON::Object&, const HashSet<String>&);
        URLTransformAction isolatedCopy() const;
        bool operator==(const URLTransformAction&) const;
        void serialize(Vector<uint8_t>&) const;
        static URLTransformAction deserialize(Span<const uint8_t>);
        static size_t serializedLength(Span<const uint8_t>);
    };
    struct URLAction {
        String url;

        URLAction isolatedCopy() const { return { url.isolatedCopy() }; }
        bool operator==(const URLAction& other) const { return other.url == this->url; }
    };

    std::variant<ExtensionPathAction, RegexSubstitutionAction, URLTransformAction, URLAction> action;

    static Expected<RedirectAction, std::error_code> parse(const JSON::Object&, const HashSet<String>&);
    RedirectAction isolatedCopy() const;
    WEBCORE_EXPORT bool operator==(const RedirectAction&) const;
    void serialize(Vector<uint8_t>&) const;
    static RedirectAction deserialize(Span<const uint8_t>);
    static size_t serializedLength(Span<const uint8_t>);
};

using ActionData = std::variant<
    BlockLoadAction,
    BlockCookiesAction,
    CSSDisplayNoneSelectorAction,
    NotifyAction,
    IgnorePreviousRulesAction,
    MakeHTTPSAction,
    ModifyHeadersAction,
    RedirectAction
>;

} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
