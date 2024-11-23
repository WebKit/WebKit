/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <optional>
#include <wtf/HashMap.h>
#include <wtf/OptionSet.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WTF {
namespace URLMatch {

// The types of subresource requests that a URL rule should be applied to.
enum class ElementType : uint16_t {
    Other = 1 << 0,
    Script = 1 << 1,
    Image = 1 << 2,
    Stylesheet = 1 << 3,
    Object = 1 << 4,
    XMLHTTPRequest = 1 << 5,
    Subdocument = 1 << 6,
    Ping = 1 << 7,
    Media = 1 << 8,
    Font = 1 << 9,
    WebSocket = 1 << 10,
    WebRTC = 1 << 11,

    Count = 12,
    All = (1 << Count) - 1,
    None = 0,
};

// The options controlling whether or not to activate filtering for subresources
// of documents that match the URL pattern of the rule.
// Note: Values are used as flags in a bitmask.
enum class ActivationType : uint8_t {
    // Disable all rules on the page.
    Document = 1 << 0,
    // Disable CSS rules on the page.
    ElemHide = 1 << 1,
    // Disable generic CSS rules on the page.
    GenericHide = 1 << 2,
    // Disable generic URL rules on the page.
    GenericBlock = 1 << 3,

    Count = 4,
    All = (1 << Count) - 1,
    None = 0,
};

enum class Anchor : uint8_t {
    None,
    Boundary,
    Subdomain,
};

enum class TriState : uint8_t {
    DontCare,
    Yes,
    No,
};

struct Rule final {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    String pattern;

    // The list of domains to be included/excluded from the filter's affected set.
    // If a particular string in the list starts with '~' then the respective
    // domain is excluded, otherwise included. The list should be ordered by
    // |CanonicalizeDomainList|.
    Vector<String> domains;

    // A bitmask that reflects what ElementType's to block/allow and what
    // kinds of ActivationType's to associate this rule with.
    OptionSet<ElementType> elementTypes { defaultElementTypes() };
    OptionSet<ActivationType> activationTypes;

    // This is an allowlist rule (aka rule-level exception).
    bool isException { false };
    // An anchor used at the beginning of the URL pattern.
    Anchor anchorLeft { Anchor::None };
    // The same for the end of the pattern. Never equals to ANCHOR_TYPE_SUBDOMAIN.
    Anchor anchorRight { Anchor::None };
    // Apply the filter only to addresses with matching case.
    bool matchCase { false };
    // Restriction to first-party/third-party requests.
    TriState isThirdParty { TriState::DontCare };

    bool isValid() const { return !pattern.isEmpty(); }
    String toString() const;

    constexpr static OptionSet<ElementType> defaultElementTypes()
    {
        return OptionSet<ElementType>::fromRaw(static_cast<uint16_t>(ElementType::All));
    }
};

// Meta-information about an option represented by a certain keyword.
struct OptionDetails final {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    enum class Type : uint8_t {
        Undefined,
        ElementType,
        ActivationType,
        ThirdParty,
        Domain,
        SiteKey,
        MatchCase,
        Collapse,
        DoNotTrack,
        Popup,
    };

    enum class Flag : uint8_t {
        // The option requires a value, e.g. 'domain=example.org'.
        RequiresValue = 1 << 0,
        // The option allows invertion, e.g. 'image' and '~image'.
        TriState = 1 << 1,
        // The option can be used with allowlist rules only.
        AllowlistOnly = 1 << 2,
        // The option is not supposed to be used any more.
        IsDeprecated = 1 << 3,
        // The option is not supported yet.
        IsNotSupported = 1 << 4,
    };

    // Creates an option that defines a filter for the specified |elementType|.
    // In addition to the provided |flags|, FLAG_IS_TRISTATE will always be set
    // by default.
    OptionDetails(ElementType elementType, OptionSet<Flag> flags)
        : type(Type::ElementType)
        , flags(Flag::TriState | flags)
        , elementType(elementType)
    {
    }

    // Creates an ActivationType option.
    explicit OptionDetails(ActivationType activationType)
        : type(Type::ActivationType)
        , flags(Flag::AllowlistOnly)
        , activationType(activationType)
    {
    }

    // Creates a generic option.
    OptionDetails(Type type, OptionSet<Flag> flags)
        : type(type)
        , flags(flags)
    {
        ASSERT(type != Type::ElementType && type != Type::ActivationType);
    }

    bool isTriState() const { return flags.contains(Flag::TriState); }
    bool requiresValue() const { return flags.contains(Flag::RequiresValue); }
    bool isAllowlistOnly() const { return flags.contains(Flag::AllowlistOnly); }
    bool isDeprecated() const { return flags.contains(Flag::IsDeprecated); }
    bool isNotSupported() const { return flags.contains(Flag::IsNotSupported); }

    Type type { Type::Undefined };
    OptionSet<Flag> flags;
    std::optional<ElementType> elementType;
    std::optional<ActivationType> activationType;
};

class Parser final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Parser() = default;
    ~Parser() = default;

    struct Error final {
        enum class Code : uint8_t {
            NoError,
            Ignored,
            NotSupported,
            InvalidRule,
            InvalidOption,
            NotTriStateOption,
            NoOptionValueProvided,
            AllowlistOnlyOption,
        };

        Code code { Code::NoError };
        size_t line { 0 };
    };

    Expected<Vector<Rule>, Error> parse(StringView source);
    Expected<Rule, Error::Code> parseLine(StringView line);
    Error::Code parseOptions(StringView part, Rule&);

private:
    void prepareOptionDetailsMap();

    static bool canIgnoreError(Error::Code);

    HashMap<String, std::unique_ptr<OptionDetails>> m_optionDetailsMap;
};

} // namespace URLMatch
} // namespace WTF

namespace URLMatch = WTF::URLMatch;
