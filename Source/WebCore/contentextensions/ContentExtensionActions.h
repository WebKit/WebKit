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

#include <wtf/Platform.h>

#if ENABLE(CONTENT_EXTENSIONS)

#include <WebCore/ContentExtensionStringSerialization.h>
#include <system_error>
#include <wtf/JSONValues.h>
#include <wtf/Hasher.h>

namespace WebCore {
class ResourceRequest;
}

namespace WebCore::ContentExtensions {

struct Action;

using SerializedActionByte = uint8_t;

template<typename T> struct ActionWithoutMetadata {
    T isolatedCopy() const { return { }; }
    friend bool operator==(const ActionWithoutMetadata&, const ActionWithoutMetadata&) = default;
    void serialize(Vector<uint8_t>&) const { }
    static T deserialize(std::span<const uint8_t>) { return { }; }
    static size_t serializedLength(std::span<const uint8_t>) { return 0; }
};

template<typename T> struct ActionWithStringMetadata {
    RefPtr<StringImpl> string;
    T isolatedCopy() const & { return { { WTF::isolatedCopy(string) } }; }
    T isolatedCopy() && { return { { WTF::isolatedCopy(WTF::move(string)) } }; }
    friend bool operator==(const ActionWithStringMetadata& a, const ActionWithStringMetadata& b)
    {
        return contentEquals(a.string, b.string);
    }
    void serialize(Vector<uint8_t>& vector) const { serializeString(vector, string); }
    static T deserialize(std::span<const uint8_t> span) { return { { deserializeString(span) } }; }
    static size_t serializedLength(std::span<const uint8_t> span) { return stringSerializedLength(span); }
};

struct BlockLoadAction : public ActionWithoutMetadata<BlockLoadAction> { };
struct BlockCookiesAction : public ActionWithoutMetadata<BlockCookiesAction> { };
struct CSSDisplayNoneSelectorAction : public ActionWithStringMetadata<CSSDisplayNoneSelectorAction> { };
struct NotifyAction : public ActionWithStringMetadata<NotifyAction> { };
struct IgnorePreviousRulesAction : public ActionWithoutMetadata<IgnorePreviousRulesAction> { };
struct IgnoreFollowingRulesAction : public ActionWithoutMetadata<IgnoreFollowingRulesAction> { };
struct MakeHTTPSAction : public ActionWithoutMetadata<MakeHTTPSAction> { };

struct WEBCORE_EXPORT ModifyHeadersAction {
    enum class ModifyHeadersOperationType { Unknown, Append, Set, Remove };

    struct ModifyHeaderInfo {
        struct AppendOperation {
            RefPtr<StringImpl> header;
            RefPtr<StringImpl> value;

            AppendOperation isolatedCopy() const & { return { WTF::isolatedCopy(header), WTF::isolatedCopy(value) }; }
            AppendOperation isolatedCopy() && { return { WTF::isolatedCopy(WTF::move(header)), WTF::isolatedCopy(WTF::move(value)) }; }
            friend bool operator==(const AppendOperation& a, const AppendOperation& b)
            {
                return contentEquals(a.header, b.header)
                    && contentEquals(a.value, b.value);
            }
        };
        struct SetOperation {
            RefPtr<StringImpl> header;
            RefPtr<StringImpl> value;

            SetOperation isolatedCopy() const & { return { WTF::isolatedCopy(header), WTF::isolatedCopy(value) }; }
            SetOperation isolatedCopy() && { return { WTF::isolatedCopy(WTF::move(header)), WTF::isolatedCopy(WTF::move(value)) }; }
            friend bool operator==(const SetOperation& a, const SetOperation& b)
            {
                return contentEquals(a.header, b.header)
                    && contentEquals(a.value, b.value);
            }
        };
        struct RemoveOperation {
            RefPtr<StringImpl> header;

            RemoveOperation isolatedCopy() const & { return { WTF::isolatedCopy(header) }; }
            RemoveOperation isolatedCopy() && { return { WTF::isolatedCopy(WTF::move(header)) }; }
            friend bool operator==(const RemoveOperation& a, const RemoveOperation& b)
            {
                return contentEquals(a.header, b.header);
            }
        };
        using OperationVariant = Variant<AppendOperation, SetOperation, RemoveOperation>;
        OperationVariant operation;

        static std::expected<ModifyHeaderInfo, std::error_code> parse(const JSON::Value&);
        ModifyHeaderInfo isolatedCopy() const &;
        ModifyHeaderInfo isolatedCopy() &&;
        friend bool operator==(const ModifyHeaderInfo&, const ModifyHeaderInfo&) = default;
        void serialize(Vector<uint8_t>&) const;
        static ModifyHeaderInfo deserialize(std::span<const uint8_t>);
        NODELETE static size_t serializedLength(std::span<const uint8_t>);
        void applyToRequest(ResourceRequest&, HashMap<RefPtr<StringImpl>, ModifyHeadersOperationType>&);
    };

    enum class HashTableType : uint8_t { Empty, Deleted, Full } hashTableType;
    Vector<ModifyHeaderInfo> requestHeaders;
    Vector<ModifyHeaderInfo> responseHeaders;
    uint32_t priority = 0;

    ModifyHeadersAction(Vector<ModifyHeaderInfo>&& requestHeaders, Vector<ModifyHeaderInfo>&& responseHeaders, uint32_t priority)
        : hashTableType(HashTableType::Full)
        , requestHeaders(WTF::move(requestHeaders))
        , responseHeaders(WTF::move(responseHeaders))
        , priority(priority) { }

    enum EmptyValueTag { EmptyValue };
    enum DeletedValueTag { DeletedValue };
    ModifyHeadersAction(EmptyValueTag) : hashTableType(HashTableType::Empty) { }
    ModifyHeadersAction(DeletedValueTag) : hashTableType(HashTableType::Deleted) { }
    bool isDeletedValue() const { return hashTableType == HashTableType::Deleted; }
    bool isEmptyValue() const { return hashTableType == HashTableType::Empty; }
    static constexpr bool safeToCompareToHashTableEmptyOrDeletedValue = true;

    static std::expected<ModifyHeadersAction, std::error_code> parse(const JSON::Object&);
    ModifyHeadersAction isolatedCopy() const &;
    ModifyHeadersAction isolatedCopy() &&;
    friend bool operator==(const ModifyHeadersAction&, const ModifyHeadersAction&) = default;
    void serialize(Vector<uint8_t>&) const;
    static ModifyHeadersAction deserialize(std::span<const uint8_t>);
    NODELETE static size_t serializedLength(std::span<const uint8_t>);
    void applyToRequest(ResourceRequest&, HashMap<RefPtr<StringImpl>, ModifyHeadersOperationType>&);
};

struct WEBCORE_EXPORT RedirectAction {
    struct ExtensionPathAction {
        RefPtr<StringImpl> extensionPath;

        ExtensionPathAction isolatedCopy() const & { return { WTF::isolatedCopy(extensionPath) }; }
        ExtensionPathAction isolatedCopy() && { return { WTF::isolatedCopy(WTF::move(extensionPath)) }; }
        friend bool operator==(const ExtensionPathAction& a, const ExtensionPathAction& b)
        {
            return contentEquals(a.extensionPath, b.extensionPath);
        }
    };
    struct RegexSubstitutionAction {
        RefPtr<StringImpl> regexSubstitution;
        RefPtr<StringImpl> regexFilter;

        RegexSubstitutionAction isolatedCopy() const & { return { WTF::isolatedCopy(regexSubstitution), WTF::isolatedCopy(regexFilter) }; }
        RegexSubstitutionAction isolatedCopy() && { return { WTF::isolatedCopy(WTF::move(regexSubstitution)), WTF::isolatedCopy(WTF::move(regexFilter)) }; }
        void serialize(Vector<uint8_t>&) const;
        static RegexSubstitutionAction deserialize(std::span<const uint8_t>);
        friend bool operator==(const RegexSubstitutionAction& a, const RegexSubstitutionAction& b)
        {
            return contentEquals(a.regexSubstitution, b.regexSubstitution)
                && contentEquals(a.regexFilter, b.regexFilter);
        }
        WEBCORE_EXPORT void applyToURL(URL&) const;
    };
    struct URLTransformAction {
        struct QueryTransform {
            struct QueryKeyValue {
                RefPtr<StringImpl> key;
                bool replaceOnly { false };
                RefPtr<StringImpl> value;

                static std::expected<QueryKeyValue, std::error_code> parse(const JSON::Value&);
                QueryKeyValue isolatedCopy() const & { return { WTF::isolatedCopy(key), replaceOnly, WTF::isolatedCopy(value) }; }
                QueryKeyValue isolatedCopy() && { return { WTF::isolatedCopy(WTF::move(key)), replaceOnly, WTF::isolatedCopy(WTF::move(value)) }; }
                friend bool operator==(const QueryKeyValue& a, const QueryKeyValue& b)
                {
                    return contentEquals(a.key, b.key)
                        && a.replaceOnly == b.replaceOnly
                        && contentEquals(a.value, b.value);
                }
                void serialize(Vector<uint8_t>&) const;
                static QueryKeyValue deserialize(std::span<const uint8_t>);
                NODELETE static size_t serializedLength(std::span<const uint8_t>);
            };

            Vector<QueryKeyValue> addOrReplaceParams;
            Vector<RefPtr<StringImpl>> removeParams;

            static std::expected<QueryTransform, std::error_code> parse(const JSON::Object&);
            QueryTransform isolatedCopy() const &;
            QueryTransform isolatedCopy() &&;
            friend bool operator==(const QueryTransform& a, const QueryTransform& b)
            {
                return a.addOrReplaceParams == b.addOrReplaceParams
                    && contentEquals(a.removeParams, b.removeParams);
            }
            void serialize(Vector<uint8_t>&) const;
            static QueryTransform deserialize(std::span<const uint8_t>);
            NODELETE static size_t serializedLength(std::span<const uint8_t>);
            void applyToURL(URL&) const;
        };

        RefPtr<StringImpl> fragment;
        RefPtr<StringImpl> host;
        RefPtr<StringImpl> password;
        RefPtr<StringImpl> path;
        std::optional<std::optional<uint16_t>> port;
        using QueryTransformVariant = Variant<RefPtr<StringImpl>, QueryTransform>;
        QueryTransformVariant queryTransform;
        RefPtr<StringImpl> scheme;
        RefPtr<StringImpl> username;

        static std::expected<URLTransformAction, std::error_code> parse(const JSON::Object&);
        URLTransformAction isolatedCopy() const &;
        URLTransformAction isolatedCopy() &&;
        friend bool operator==(const URLTransformAction& a, const URLTransformAction& b)
        {
            return contentEquals(a.fragment, b.fragment)
                && contentEquals(a.host, b.host)
                && contentEquals(a.password, b.password)
                && contentEquals(a.path, b.path)
                && a.port == b.port
                && contentEquals(a.queryTransform, b.queryTransform)
                && contentEquals(a.scheme, b.scheme)
                && contentEquals(a.username, b.username);
        }
        void serialize(Vector<uint8_t>&) const;
        static URLTransformAction deserialize(std::span<const uint8_t>);
        NODELETE static size_t serializedLength(std::span<const uint8_t>);
        void applyToURL(URL&) const;
    };
    struct URLAction {
        RefPtr<StringImpl> url;

        URLAction isolatedCopy() const & { return { WTF::isolatedCopy(url) }; }
        URLAction isolatedCopy() && { return { WTF::isolatedCopy(WTF::move(url)) }; }
        friend bool operator==(const URLAction& a, const URLAction& b)
        {
            return contentEquals(a.url, b.url);
        }
    };

    enum class HashTableType : uint8_t { Empty, Deleted, Full } hashTableType;
    using ActionVariant = Variant<ExtensionPathAction, RegexSubstitutionAction, URLTransformAction, URLAction>;
    ActionVariant action;
    uint32_t priority { 0 };

    RedirectAction(ActionVariant&&, uint32_t priority = 0);

    enum EmptyValueTag { EmptyValue };
    enum DeletedValueTag { DeletedValue };
    RedirectAction(EmptyValueTag) : hashTableType(HashTableType::Empty) { }
    RedirectAction(DeletedValueTag) : hashTableType(HashTableType::Deleted) { }
    bool isDeletedValue() const { return hashTableType == HashTableType::Deleted; }
    bool isEmptyValue() const { return hashTableType == HashTableType::Empty; }
    static constexpr bool safeToCompareToHashTableEmptyOrDeletedValue = true;

    static std::expected<RedirectAction, std::error_code> parse(const JSON::Object&, StringImpl* urlFilter);
    RedirectAction isolatedCopy() const &;
    RedirectAction isolatedCopy() &&;
    friend bool operator==(const RedirectAction&, const RedirectAction&) = default;
    void serialize(Vector<uint8_t>&) const;
    static RedirectAction deserialize(std::span<const uint8_t>);
    NODELETE static size_t serializedLength(std::span<const uint8_t>);
    void applyToRequest(ResourceRequest&, const URL&);
    void modifyURL(URL& originalURL, const URL& extensionBaseURL);

    // Whether the redirect only removes query parameters, so it can be applied together with another one.
    bool onlyRemovesQueryParameters() const;
};

struct ReportIdentifierAction : public ActionWithStringMetadata<ReportIdentifierAction> {
    double identifier;

    ReportIdentifierAction(RefPtr<StringImpl> string)
    : ActionWithStringMetadata<ReportIdentifierAction> { { WTF::move(string) } }
    , identifier(0)
    {
    }

    ReportIdentifierAction(RefPtr<StringImpl> string, double identifier)
    : ActionWithStringMetadata<ReportIdentifierAction> { { WTF::move(string) } }
    , identifier(identifier)
    {
    }

    ReportIdentifierAction isolatedCopy() const & { return { WTF::isolatedCopy(string), identifier }; }
    ReportIdentifierAction isolatedCopy() && { return { WTF::isolatedCopy(WTF::move(string)), identifier }; }
    friend bool operator==(const ReportIdentifierAction&, const ReportIdentifierAction&) = default;

    void serialize(Vector<uint8_t>& vector) const
    {
        vector.reserveCapacity(vector.size() + sizeof(identifier));
        vector.append(asByteSpan(identifier));

        ActionWithStringMetadata<ReportIdentifierAction>::serialize(vector);
    }

    static ReportIdentifierAction deserialize(std::span<const uint8_t> span)
    {
        ReportIdentifierAction value = ActionWithStringMetadata<ReportIdentifierAction>::deserialize(span.subspan(sizeof(identifier)));
        value.identifier = reinterpretCastSpanStartTo<double>(span);

        return value;
    }

    static size_t serializedLength(std::span<const uint8_t> span)
    {
        return ActionWithStringMetadata<ReportIdentifierAction>::serializedLength(span.subspan(sizeof(identifier))) + sizeof(identifier);
    }
};

using ActionData = Variant<
    BlockLoadAction,
    BlockCookiesAction,
    CSSDisplayNoneSelectorAction,
    NotifyAction,
    IgnorePreviousRulesAction,
    IgnoreFollowingRulesAction,
    MakeHTTPSAction,
    ModifyHeadersAction,
    RedirectAction,
    ReportIdentifierAction
>;

inline void add(Hasher& hasher, const ModifyHeadersAction::ModifyHeaderInfo::AppendOperation& operation)
{
    add(hasher, operation.header, operation.value);
}

inline void add(Hasher& hasher, const ModifyHeadersAction::ModifyHeaderInfo::SetOperation& operation)
{
    add(hasher, operation.header, operation.value);
}

inline void add(Hasher& hasher, const ModifyHeadersAction::ModifyHeaderInfo::RemoveOperation& operation)
{
    add(hasher, operation.header);
}

inline void add(Hasher& hasher, const ModifyHeadersAction::ModifyHeaderInfo& info)
{
    add(hasher, info.operation);
}

inline void add(Hasher& hasher, const RedirectAction::ExtensionPathAction& action)
{
    add(hasher, action.extensionPath);
}

inline void add(Hasher& hasher, const RedirectAction::RegexSubstitutionAction& action)
{
    add(hasher, action.regexSubstitution, action.regexFilter);
}

inline void add(Hasher& hasher, const RedirectAction::URLTransformAction::QueryTransform::QueryKeyValue& queryKeyValue)
{
    add(hasher, queryKeyValue.key, queryKeyValue.replaceOnly, queryKeyValue.value);
}

inline void add(Hasher& hasher, const RedirectAction::URLTransformAction::QueryTransform& transform)
{
    add(hasher, transform.addOrReplaceParams, transform.removeParams);
}

inline void add(Hasher& hasher, const RedirectAction::URLAction& action)
{
    add(hasher, action.url);
}

inline void add(Hasher& hasher, const RedirectAction::URLTransformAction& action)
{
    add(hasher, action.fragment, action.host, action.password, action.path, action.port, action.queryTransform, action.scheme, action.username);
}

inline void add(Hasher& hasher, const RedirectAction& action)
{
    add(hasher, action.action, action.priority);
}

inline void add(Hasher& hasher, const ModifyHeadersAction& action)
{
    add(hasher, action.requestHeaders, action.responseHeaders, action.priority);
}

} // namespace WebCore::ContentExtensions

namespace WTF {

template<> struct HashTraits<WebCore::ContentExtensions::RedirectAction> : public CustomHashTraits<WebCore::ContentExtensions::RedirectAction> { };
template<> struct HashTraits<WebCore::ContentExtensions::ModifyHeadersAction> : public CustomHashTraits<WebCore::ContentExtensions::ModifyHeadersAction> { };

} // namespace WTF

#endif // ENABLE(CONTENT_EXTENSIONS)
