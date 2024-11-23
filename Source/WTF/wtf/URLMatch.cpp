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
#include <wtf/URLMatch.h>

#include <wtf/ASCIICType.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WTF {
namespace URLMatch {

static StringView trim(const StringView& str)
{
    return str.trim(isASCIIWhitespace<LChar>);
}

Expected<Vector<Rule>, Parser::Error> Parser::parse(StringView source)
{
    Vector<Rule> rules;
    size_t lineNo = 1;

    while (!source.isEmpty()) {
        auto pos = source.find('\n');
        auto line = (pos == notFound) ? source : source.substring(0, pos);
        source = source.substring(pos + 1);

        auto result = parseLine(line);
        if (result.has_value())
            rules.append(result.value());
        else {
            auto errorCode = result.error();
            if (!canIgnoreError(errorCode))
                return makeUnexpected(Error { errorCode, lineNo });
        }
        lineNo++;
    }

    return rules;
}

Expected<Rule, Parser::Error::Code>
Parser::parseLine(StringView line)
{
    prepareOptionDetailsMap();

    // Strip all leading and trailing whitespaces.
    auto part = trim(line);
    if (part.isEmpty() || part.startsWith('!'))
        return makeUnexpected(Parser::Error::Code::Ignored);

    // Suppose it is a CSS rule if a CSS-selector separator character ('#') is
    // present, followed by '#' or '@'.
    for (auto pos = part.find('#'); pos != notFound; pos = part.find('#', pos + 1)) {
        if (pos + 2 >= part.length())
            break;
        if (auto nextChar = part[pos + 1]; nextChar == '#' || nextChar == '@')
            return makeUnexpected(Parser::Error::Code::NotSupported);
    }

    Rule rule;

    // Check whether it's an allowlist rule.
    if (part.startsWith('@')) {
        if (part.length() < 2 || part[1] != '@')
            return makeUnexpected(Parser::Error::Code::InvalidRule);

        part = part.substring(2);
        if (part.isEmpty())
            return makeUnexpected(Parser::Error::Code::InvalidRule);

        rule.isException = true;
    }

    auto optionPos = part.reverseFind('$');
    // If the URL pattern is a regular expression, |options_start| might be
    // pointing to a character inside the pattern. This can happen for those rules
    // which don't have options at all, e.g., "/.*substring$/". All such rules end
    // with '/', therefore the following code can detect them to work around.
    if (optionPos != notFound && part[part.length() - 1] == '/')
        optionPos = notFound;

    if (optionPos != notFound) {
        auto errorCode = parseOptions(part.substring(optionPos + 1), rule);
        if (errorCode != Error::Code::NoError && errorCode != Error::Code::NotSupported)
            return makeUnexpected(errorCode);

        part = part.substring(0, optionPos);
        if (part.isEmpty())
            return makeUnexpected(Parser::Error::Code::InvalidRule);
    }

    // Check for a left anchor.
    if (part.startsWith('|')) {
        if (part.length() >= 2 && part[1] == '|') {
            part = part.substring(2);
            if (part.isEmpty())
                return makeUnexpected(Parser::Error::Code::InvalidRule);

            rule.anchorLeft = Anchor::Subdomain;
        } else {
            part = part.substring(1);
            if (part.isEmpty())
                return makeUnexpected(Parser::Error::Code::InvalidRule);

            rule.anchorLeft = Anchor::Boundary;
        }
    }

    // Check for a right anchor.
    if (part.endsWith('|')) {
        part = part.substring(0, part.length() - 1);
        if (part.isEmpty())
            return makeUnexpected(Parser::Error::Code::InvalidRule);

        rule.anchorRight = Anchor::Boundary;
    }

    rule.pattern = part.toString();

    return rule;
}

Parser::Error::Code
Parser::parseOptions(StringView part, Rule& rule)
{
    using Type = OptionDetails::Type;

    if (part.isEmpty())
        return Error::Code::NoError;

    bool hasSeenElementOrActivationType = false;

    for (auto piece : part.split(',')) {
        piece = trim(piece);
        if (piece.isEmpty())
            return Error::Code::InvalidOption;

        TriState state = TriState::Yes;
        if (piece.startsWith('~')) {
            state = TriState::No;
            piece = piece.substring(1);
        }

        auto nameEnd = piece.find('=');
        String optionName = piece.substring(0, nameEnd).toString();

        const auto optionDetails = m_optionDetailsMap.get(optionName);
        if (!optionDetails)
            return Error::Code::InvalidOption;

        if (state == TriState::No && !optionDetails->isTriState())
            return Error::Code::NotTriStateOption;

        if (optionDetails->requiresValue() && nameEnd == notFound)
            return Error::Code::NoOptionValueProvided;

        if (optionDetails->isAllowlistOnly() && !rule.isException)
            return Error::Code::AllowlistOnlyOption;

        if (optionDetails->isDeprecated() || optionDetails->isNotSupported())
            return Error::Code::NotSupported;

        switch (optionDetails->type) {
        case Type::ElementType:
            ASSERT(optionDetails->elementType);
            // The sign of the first element type option encountered determines
            // whether the unspecified element types will be included (if the first
            // option is negated) or excluded (otherwise).
            if (state == TriState::Yes) {
                if (!hasSeenElementOrActivationType)
                    rule.elementTypes.clear();
                rule.elementTypes.add(optionDetails->elementType);
            } else
                rule.elementTypes.remove(optionDetails->elementType);
            hasSeenElementOrActivationType = true;
            break;

        case Type::ActivationType:
            if (!hasSeenElementOrActivationType) {
                rule.elementTypes.clear();
                rule.activationTypes.clear();
                hasSeenElementOrActivationType = true;
            }

            ASSERT(optionDetails->activationType);
            rule.activationTypes |= optionDetails->activationType.value();
            break;

        case Type::ThirdParty:
            rule.isThirdParty = state;
            break;

        case Type::MatchCase:
            rule.matchCase = true;
            break;

        case Type::Domain:
            for (auto domain : piece.substring(nameEnd + 1).split('|'))
                rule.domains.append(trim(domain).toString().foldCase());
            break;

        default:
            return Error::Code::NotSupported;
        }
    }
    return Error::Code::NoError;
}

using Type = OptionDetails::Type;
using Flag = OptionDetails::Flag;

constexpr static struct {
    const char* name;
    ElementType type;
} const kElementTypes[] = {
    { "other", ElementType::Other },
    { "script", ElementType::Script },
    { "image", ElementType::Image },
    { "stylesheet", ElementType::Stylesheet },
    { "object", ElementType::Object },
    { "xmlhttprequest", ElementType::XMLHTTPRequest },
    { "subdocument", ElementType::Subdocument },
    { "ping", ElementType::Ping },
    { "media", ElementType::Media },
    { "font", ElementType::Font },
    { "websocket", ElementType::WebSocket },
    { "webrtc", ElementType::WebRTC },
};

// A mapping from deprecated element type names to active element types.
constexpr static struct {
    const char* name;
    ElementType type;
} const kDeprecatedElementTypes[] = {
    { "background",  ElementType::Image },
    { "xbl",  ElementType::Other },
    { "dtd",  ElementType::Other },
};

// A list of items mapping activation type options to their names.
constexpr static struct {
    const char* name;
    ActivationType type;
} const kActivationTypes[] = {
    { "document", ActivationType::Document },
    { "elemhide", ActivationType::ElemHide },
    { "generichide", ActivationType::GenericHide },
    { "genericblock", ActivationType::GenericBlock },
};

constexpr static struct {
    const char* name;
    Type type;
    OptionSet<Flag> flags;
} const kOtherOptions[] = {
    // Tristate options.
    { "third-party", Type::ThirdParty, Flag::TriState },
    { "collapse", Type::Collapse, { Flag::TriState, Flag::IsNotSupported } },
    // Flag options.
    { "match-case", Type::MatchCase, { } },
    { "donottrack", Type::DoNotTrack, Flag::IsNotSupported },
    { "popup", Type::Popup, Flag::IsDeprecated },
    // Value options.
    { "sitekey", Type::SiteKey, { Flag::RequiresValue, Flag::IsNotSupported } },
    { "domain", Type::Domain, Flag::RequiresValue },
};

void Parser::prepareOptionDetailsMap()
{
    if (!m_optionDetailsMap.isEmpty())
        return;

    for (const auto& option : kElementTypes)
        m_optionDetailsMap.add(String::fromLatin1(option.name), makeUnique<OptionDetails>(option.type, OptionSet<Flag>()));

    for (const auto& option : kDeprecatedElementTypes)
        m_optionDetailsMap.add(String::fromLatin1(option.name), makeUnique<OptionDetails>(option.type, Flag::IsDeprecated));

    for (const auto& option : kActivationTypes)
        m_optionDetailsMap.add(String::fromLatin1(option.name), makeUnique<OptionDetails>(option.type));

    for (const auto& option : kOtherOptions)
        m_optionDetailsMap.add(String::fromLatin1(option.name), makeUnique<OptionDetails>(option.type, option.flags));
}

bool Parser::canIgnoreError(Parser::Error::Code errorCode)
{
    switch (errorCode) {
    case Error::Code::NoError:
    case Error::Code::Ignored:
    case Error::Code::NotSupported:
        return true;
    default:
        break;
    }
    return false;
}

String Rule::toString() const
{
    StringBuilder builder;

    if (isException)
        builder.append("@@"_s);

    if (anchorLeft == Anchor::Subdomain)
        builder.append("||"_s);
    else if (anchorLeft == Anchor::Boundary)
        builder.append('|');

    builder.append(pattern);

    if (anchorRight == Anchor::Boundary)
        builder.append('|');

    Vector<String> options;

    if (elementTypes != defaultElementTypes()) {
        auto count = elementTypes.size();
        bool showNegated = count >= (static_cast<size_t>(ElementType::Count) - count);
        for (const auto& option : kElementTypes) {
            bool isSet = elementTypes.contains(option.type);
            auto name = String::fromLatin1(option.name);
            if (showNegated) {
                if (!isSet)
                    options.append(makeString("~"_s, name));
            } else {
                if (isSet)
                    options.append(name);
            }
        }
    }

    if (activationTypes) {
        auto count = activationTypes.size();
        bool showNegated = count >= (static_cast<size_t>(ActivationType::Count) - count);
        for (const auto& option : kActivationTypes) {
            bool isSet = activationTypes.contains(option.type);
            auto name = String::fromLatin1(option.name);
            if (showNegated) {
                if (!isSet)
                    options.append(makeString("~"_s, name));
            } else {
                if (isSet)
                    options.append(name);
            }
        }
    }

    if (isThirdParty != TriState::DontCare)
        options.append(isThirdParty == TriState::No ? "~third-party"_s : "third-party"_s);

    if (matchCase)
        options.append("match-case"_s);

    if (!domains.isEmpty()) {
        StringBuilder domainBuilder;
        domainBuilder.append("domain"_s);

        char separator = '=';
        for (const auto& domain : domains) {
            domainBuilder.append(separator);
            domainBuilder.append(domain);
            if (separator == '=')
                separator = '|';
        }

        options.append(domainBuilder.toString());
    }

    if (!options.isEmpty()) {
        builder.append('$');
        builder.append(options[0]);

        for (size_t i = 1; i < options.size(); ++i) {
            builder.append(',');
            builder.append(options[i]);
        }
    }

    return builder.toString();
}

} // namespace URLMatch
} // namespace WTF
