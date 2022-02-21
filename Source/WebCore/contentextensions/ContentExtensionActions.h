/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include <wtf/HashFunctions.h>
#include <wtf/Hasher.h>

namespace WebCore {
class ResourceRequest;
}

namespace WebCore::ContentExtensions {

// FIXME: This probably belongs in WTF, such as in HashFunctions.h.
struct VariantHasher {
    template <typename V>
    static uint32_t hash(V&& variant)
    {
        return pairIntHash(variant.index(), std::visit([](const auto& type) {
            return type.hash();
        }, std::forward<V>(variant)));
    }
};

// FIXME: This probably belongs in WTF, such as in Hasher.h.
struct VectorHasher {
    template <typename V>
    static uint32_t hash(V&& vector)
    {
        Hasher hasher;
        for (auto& element : vector)
            add(hasher, element.hash());
        return hasher.hash();
    }
};

struct Action;

using SerializedActionByte = uint8_t;

template<typename T> struct ActionWithoutMetadata {
    T isolatedCopy() const { return { }; }
    bool operator==(const ActionWithoutMetadata&) const { return true; }
    void serialize(Vector<uint8_t>&) const { }
    static T deserialize(Span<const uint8_t>) { return { }; }
    static size_t serializedLength(Span<const uint8_t>) { return 0; }
};

template<typename T> struct ActionWithStringMetadata {
    const String string;
    T isolatedCopy() const { return { { string.isolatedCopy() } }; }
    bool operator==(const ActionWithStringMetadata& other) const { return other.string == this->string; }
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

struct WEBCORE_EXPORT ModifyHeadersAction {
    struct ModifyHeaderInfo {
        struct AppendOperation {
            String header;
            String value;

            uint32_t hash() const { return pairIntHash(header.hash(), value.hash()); }
            AppendOperation isolatedCopy() const { return { header.isolatedCopy(), value.isolatedCopy() }; }
            bool operator==(const AppendOperation& other) const { return other.header == this->header && other.value == this->value; }
        };
        struct SetOperation {
            String header;
            String value;

            uint32_t hash() const { return pairIntHash(header.hash(), value.hash()); }
            SetOperation isolatedCopy() const { return { header.isolatedCopy(), value.isolatedCopy() }; }
            bool operator==(const SetOperation& other) const { return other.header == this->header && other.value == this->value; }
        };
        struct RemoveOperation {
            String header;

            uint32_t hash() const { return header.hash(); }
            RemoveOperation isolatedCopy() const { return { header.isolatedCopy() }; }
            bool operator==(const RemoveOperation& other) const { return other.header == this->header; }
        };
        using OperationVariant = std::variant<AppendOperation, SetOperation, RemoveOperation>;
        OperationVariant operation;

        uint32_t hash() const { return VariantHasher::hash(operation); }
        static Expected<ModifyHeaderInfo, std::error_code> parse(const JSON::Value&);
        ModifyHeaderInfo isolatedCopy() const;
        bool operator==(const ModifyHeaderInfo&) const;
        void serialize(Vector<uint8_t>&) const;
        static ModifyHeaderInfo deserialize(Span<const uint8_t>);
        static size_t serializedLength(Span<const uint8_t>);
        void applyToRequest(ResourceRequest&);
    };

    enum class HashTableType : uint8_t { Empty, Deleted, Full } hashTableType;
    Vector<ModifyHeaderInfo> requestHeaders;
    Vector<ModifyHeaderInfo> responseHeaders;

    ModifyHeadersAction(Vector<ModifyHeaderInfo>&& requestHeaders, Vector<ModifyHeaderInfo>&& responseHeaders)
        : hashTableType(HashTableType::Full)
        , requestHeaders(WTFMove(requestHeaders))
        , responseHeaders(WTFMove(responseHeaders)) { }

    enum EmptyValueTag { EmptyValue };
    enum DeletedValueTag { DeletedValue };
    ModifyHeadersAction(EmptyValueTag) : hashTableType(HashTableType::Empty) { }
    ModifyHeadersAction(DeletedValueTag) : hashTableType(HashTableType::Deleted) { }
    bool isDeletedValue() const { return hashTableType == HashTableType::Deleted; }

    uint32_t hash() const { return pairIntHash(VectorHasher::hash(requestHeaders), VectorHasher::hash(responseHeaders)); }
    static Expected<ModifyHeadersAction, std::error_code> parse(const JSON::Object&);
    ModifyHeadersAction isolatedCopy() const;
    bool operator==(const ModifyHeadersAction&) const;
    void serialize(Vector<uint8_t>&) const;
    static ModifyHeadersAction deserialize(Span<const uint8_t>);
    static size_t serializedLength(Span<const uint8_t>);
    void applyToRequest(ResourceRequest&);
};

struct WEBCORE_EXPORT RedirectAction {
    struct ExtensionPathAction {
        String extensionPath;

        uint32_t hash() const { return extensionPath.hash(); }
        ExtensionPathAction isolatedCopy() const { return { extensionPath.isolatedCopy() }; }
        bool operator==(const ExtensionPathAction& other) const { return other.extensionPath == this->extensionPath; }
    };
    struct RegexSubstitutionAction {
        String regexSubstitution;

        uint32_t hash() const { return regexSubstitution.hash(); }
        RegexSubstitutionAction isolatedCopy() const { return { regexSubstitution.isolatedCopy() }; }
        bool operator==(const RegexSubstitutionAction& other) const { return other.regexSubstitution == this->regexSubstitution; }
    };
    struct URLTransformAction {
        struct QueryTransform {
            struct QueryKeyValue {
                String key;
                bool replaceOnly { false };
                String value;

                uint32_t hash() const { return computeHash(key, replaceOnly, value); }
                static Expected<QueryKeyValue, std::error_code> parse(const JSON::Value&);
                QueryKeyValue isolatedCopy() const;
                bool operator==(const QueryKeyValue&) const;
                void serialize(Vector<uint8_t>&) const;
                static QueryKeyValue deserialize(Span<const uint8_t>);
                static size_t serializedLength(Span<const uint8_t>);
            };

            Vector<QueryKeyValue> addOrReplaceParams;
            Vector<String> removeParams;

            uint32_t hash() const { return computeHash(VectorHasher::hash(addOrReplaceParams), removeParams); }
            static Expected<QueryTransform, std::error_code> parse(const JSON::Object&);
            QueryTransform isolatedCopy() const;
            bool operator==(const QueryTransform&) const;
            void serialize(Vector<uint8_t>&) const;
            static QueryTransform deserialize(Span<const uint8_t>);
            static size_t serializedLength(Span<const uint8_t>);
            void applyToURL(URL&) const;
        };

        String fragment;
        String host;
        String password;
        String path;
        std::optional<std::optional<uint16_t>> port;
        using QueryTransformVariant = std::variant<String, QueryTransform>;
        QueryTransformVariant queryTransform;
        String scheme;
        String username;

        uint32_t hash() const { return computeHash(fragment.hash(), host.hash(), password.hash(), path.hash(), port, VariantHasher::hash(queryTransform), scheme.hash(), username.hash()); }
        static Expected<URLTransformAction, std::error_code> parse(const JSON::Object&);
        URLTransformAction isolatedCopy() const;
        bool operator==(const URLTransformAction&) const;
        void serialize(Vector<uint8_t>&) const;
        static URLTransformAction deserialize(Span<const uint8_t>);
        static size_t serializedLength(Span<const uint8_t>);
        void applyToURL(URL&) const;
    };
    struct URLAction {
        String url;

        uint32_t hash() const { return url.hash(); }
        URLAction isolatedCopy() const { return { url.isolatedCopy() }; }
        bool operator==(const URLAction& other) const { return other.url == this->url; }
    };

    enum class HashTableType : uint8_t { Empty, Deleted, Full } hashTableType;
    using ActionVariant = std::variant<ExtensionPathAction, RegexSubstitutionAction, URLTransformAction, URLAction>;
    ActionVariant action;

    RedirectAction(ActionVariant&& action)
        : hashTableType(HashTableType::Full)
        , action(WTFMove(action)) { }

    enum EmptyValueTag { EmptyValue };
    enum DeletedValueTag { DeletedValue };
    RedirectAction(EmptyValueTag) : hashTableType(HashTableType::Empty) { }
    RedirectAction(DeletedValueTag) : hashTableType(HashTableType::Deleted) { }
    bool isDeletedValue() const { return hashTableType == HashTableType::Deleted; }
    uint32_t hash() const { return VariantHasher::hash(action); }

    static Expected<RedirectAction, std::error_code> parse(const JSON::Object&);
    RedirectAction isolatedCopy() const;
    bool operator==(const RedirectAction&) const;
    void serialize(Vector<uint8_t>&) const;
    static RedirectAction deserialize(Span<const uint8_t>);
    static size_t serializedLength(Span<const uint8_t>);
    void applyToRequest(ResourceRequest&, const URL&);
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

namespace WTF {

template<> struct DefaultHash<WebCore::ContentExtensions::RedirectAction> {
    using Action = WebCore::ContentExtensions::RedirectAction;
    static uint32_t hash(const Action& action) { return action.hash(); }
    static bool equal(const Action& a, const Action& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};
template<> struct HashTraits<WebCore::ContentExtensions::RedirectAction> : public CustomHashTraits<WebCore::ContentExtensions::RedirectAction> { };

template<> struct DefaultHash<WebCore::ContentExtensions::ModifyHeadersAction> {
    using Action = WebCore::ContentExtensions::ModifyHeadersAction;
    static uint32_t hash(const Action& action) { return action.hash(); }
    static bool equal(const Action& a, const Action& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};
template<> struct HashTraits<WebCore::ContentExtensions::ModifyHeadersAction> : public CustomHashTraits<WebCore::ContentExtensions::ModifyHeadersAction> { };

} // namespace WTF

#endif // ENABLE(CONTENT_EXTENSIONS)
