/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "RuntimeQuirkTable.h"

#include "QuirkBehaviorDeclarations.h"
#include <array>
#include <wtf/EnumTraits.h>
#include <wtf/HashSet.h>
#include <wtf/IndexedRange.h>
#include <wtf/JSONValues.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

bool RuntimeQuirk::appliesTo(const URLMatchContext& topContext, const URLMatchContext& documentContext, IsTopDocument isTopDocument) const
{
    bool isEmbedded = !embeddedMatches.isEmpty();
    if (isEmbedded) {
        if (isTopDocument == IsTopDocument::Yes || !anyPatternMatches(embeddedMatches, documentContext))
            return false;
        if (!matches.isEmpty() && !anyPatternMatches(matches, topContext))
            return false;
    } else if (!anyPatternMatches(matches, topContext))
        return false;

    auto& subject = isEmbedded ? documentContext : topContext;
    if (anyPatternMatches(excludeMatches, subject))
        return false;

    if (!queryContains.isNull() && !subject.url().query().contains(queryContains))
        return false;

    if (!fragmentContains.isNull() && !subject.url().fragmentIdentifier().contains(fragmentContains))
        return false;

    return !environment || evaluateURLEnvironment(*environment);
}

void RuntimeQuirk::apply(QuirksData& quirksData) const
{
    for (auto& behavior : behaviors)
        quirksData.addBehavior(behavior);
}

static constexpr std::array rowFields { "bugs"_s, "comment"_s, "matches"_s, "embeddedMatches"_s, "excludeMatches"_s, "queryContains"_s, "fragmentContains"_s, "environment"_s, "available"_s, "behaviors"_s };
static constexpr std::array behaviorFields { "id"_s, "script"_s, "userAgent"_s, "chromeCompatibilityVersion"_s, "cookieNames"_s, "elementSelector"_s, "documentSelector"_s, "secondaryURL"_s, "bugs"_s, "comment"_s };

static String nonEmptyString(const JSON::Value& value)
{
    auto& string = value.asString();
    return string.isEmpty() ? String { } : string;
}

static std::optional<Vector<String>> nonEmptyStrings(JSON::Value& value)
{
    RefPtr array = value.asArray();
    if (!array || !array->length())
        return std::nullopt;

    Vector<String> strings;
    strings.reserveInitialCapacity(array->length());
    for (auto& element : *array) {
        auto string = nonEmptyString(element);
        if (string.isNull())
            return std::nullopt;
        strings.append(WTF::move(string));
    }
    return strings;
}

template<typename Enum>
static std::optional<Enum> enumeratorFromName(StringView name)
{
    static constexpr auto names = WTF::enumNames<Enum>();
    auto entry = std::ranges::find_if(names, [&](ASCIILiteral candidate) {
        return !candidate.isNull() && name == candidate;
    });
    if (entry == names.end())
        return std::nullopt;
    return static_cast<Enum>(WTF::enumNamesMin<Enum>() + std::distance(names.begin(), entry));
}

static bool isBugReference(StringView bug)
{
    for (auto prefix : { "rdar://"_s, "https://webkit.org/b/"_s }) {
        if (!bug.startsWith(prefix))
            continue;
        auto number = bug.substring(prefix.length());
        return !number.isEmpty() && number.containsOnly<isASCIIDigit>();
    }
    return false;
}

namespace {

template<typename CharacterType>
bool isBuildConditionNameCharacter(CharacterType character)
{
    return isASCIIAlphanumeric(character) || character == '_';
}

template<typename CharacterType>
class BuildConditionExpressionParser {
public:
    explicit BuildConditionExpressionParser(StringParsingBuffer<CharacterType> buffer)
        : m_buffer(buffer)
    {
    }

    std::optional<bool> evaluate()
    {
        auto result = parseOr();
        skipWhile(m_buffer, ' ');
        if (!result || m_buffer.hasCharactersRemaining())
            return std::nullopt;
        return result;
    }

private:
    static constexpr unsigned maximumNestingDepth = 32;

    enum class Operator : bool { Or, And };

    std::optional<bool> parseOr()
    {
        auto result = parseAnd();
        while (result && skipOperator(Operator::Or)) {
            auto operand = parseAnd();
            if (!operand)
                return std::nullopt;
            result = *result || *operand;
        }
        return result;
    }

    std::optional<bool> parseAnd()
    {
        auto result = parseUnary();
        while (result && skipOperator(Operator::And)) {
            auto operand = parseUnary();
            if (!operand)
                return std::nullopt;
            result = *result && *operand;
        }
        return result;
    }

    std::optional<bool> parseUnary()
    {
        if (++m_depth > maximumNestingDepth)
            return std::nullopt;
        auto result = parseOperand();
        --m_depth;
        return result;
    }

    std::optional<bool> parseOperand()
    {
        skipWhile(m_buffer, ' ');
        if (skipExactly(m_buffer, '!')) {
            auto operand = parseUnary();
            if (!operand)
                return std::nullopt;
            return !*operand;
        }
        if (skipExactly(m_buffer, '(')) {
            auto inner = parseOr();
            skipWhile(m_buffer, ' ');
            if (!inner || !skipExactly(m_buffer, ')'))
                return std::nullopt;
            return inner;
        }

        auto remaining = m_buffer.span();
        skipWhile<isBuildConditionNameCharacter<CharacterType>>(m_buffer);
        auto condition = enumeratorFromName<BuildConditionID>(remaining.first(remaining.size() - m_buffer.lengthRemaining()));
        if (!condition)
            return std::nullopt;
        return isEnabled(*condition);
    }

    bool skipOperator(Operator op)
    {
        auto character = op == Operator::Or ? '|' : '&';
        skipWhile(m_buffer, ' ');
        constexpr auto operatorLength = 2;
        if (m_buffer.lengthRemaining() < operatorLength || m_buffer[0] != character || m_buffer[1] != character)
            return false;
        m_buffer += operatorLength;
        return true;
    }

    StringParsingBuffer<CharacterType> m_buffer;
    unsigned m_depth { 0 };
};

std::optional<bool> evaluateBuildConditions(StringView expression)
{
    return readCharactersForParsing(expression, [](auto buffer) {
        return BuildConditionExpressionParser { buffer }.evaluate();
    });
}

using ErrorLocation = QuirkTableParseError::Location;

static constexpr std::array parameterFields {
    std::pair { "script"_s, QuirkParametersNeeded::NeedsScript },
    std::pair { "userAgent"_s, QuirkParametersNeeded::NeedsUserAgent },
    std::pair { "chromeCompatibilityVersion"_s, QuirkParametersNeeded::NeedsChromeCompatibilityVersion },
    std::pair { "cookieNames"_s, QuirkParametersNeeded::NeedsCookieNames },
};

static constexpr std::array conditionFields {
    std::pair { "elementSelector"_s, QuirkConditionsSupported::ElementSelector },
    std::pair { "documentSelector"_s, QuirkConditionsSupported::DocumentSelector },
    std::pair { "secondaryURL"_s, QuirkConditionsSupported::SecondaryURL },
};

class QuirkTableParser {
public:
    QuirkTableParseResult parse(StringView);

private:
    using Kind = QuirkTableParseErrorKind;

    class Fields;

    std::optional<RuntimeQuirk> parseRow(JSON::Value&, size_t index);
    Vector<RuntimeQuirkBehavior> parseBehaviors(const JSON::Object& row, const ErrorLocation& rowLocation);
    std::optional<RuntimeQuirkBehavior> parseBehavior(JSON::Value&, const ErrorLocation& rowLocation, size_t index);

    void reject(const ErrorLocation& location, QuirkTableParseErrorKind kind, String field = { }, String value = { })
    {
        m_errors.append({
            .location = location,
            .kind = kind,
            .field = WTF::move(field),
            .value = WTF::move(value),
        });
    }

    Vector<QuirkTableParseError> m_errors;
};

class QuirkTableParser::Fields {
public:
    Fields(QuirkTableParser& parser, const JSON::Object& object, const ErrorLocation& location)
        : m_parser(parser)
        , m_object(object)
        , m_location(location)
    {
    }

    template<size_t size>
    void rejectUnknown(const std::array<ASCIILiteral, size>& knownFields)
    {
        for (auto& key : m_object.keys()) {
            if (std::ranges::none_of(knownFields, [&](ASCIILiteral field) { return key == field; }))
                m_parser.reject(m_location, Kind::UnknownField, key);
        }
    }

    void validateBugsAndComment()
    {
        if (RefPtr comment = value("comment"_s); comment && nonEmptyString(*comment).isNull())
            m_parser.reject(m_location, Kind::InvalidString, "comment"_s);

        RefPtr bugsValue = value("bugs"_s);
        if (!bugsValue)
            return;

        auto bugs = nonEmptyStrings(*bugsValue);
        if (!bugs || !std::ranges::all_of(*bugs, isBugReference)) {
            m_parser.reject(m_location, Kind::InvalidBugs, "bugs"_s);
            return;
        }

        HashSet<String> seen;
        for (auto& bug : *bugs) {
            if (!seen.add(bug).isNewEntry) {
                m_parser.reject(m_location, Kind::DuplicateBug, "bugs"_s, bug);
                return;
            }
        }
    }

    void validatePresence(ASCIILiteral field, bool isAllowed, bool isNeeded)
    {
        bool isPresent = contains(field);
        if (isPresent && !isAllowed) {
            m_parser.reject(m_location, Kind::DisallowedField, field);
            m_disallowedFields.append(field);
        } else if (!isPresent && isNeeded)
            m_parser.reject(m_location, Kind::MissingField, field);
    }

    bool contains(ASCIILiteral field) const { return static_cast<bool>(m_object.getValue(field)); }

    RefPtr<JSON::Value> value(ASCIILiteral field) const
    {
        if (m_disallowedFields.contains(field))
            return nullptr;
        return m_object.getValue(field);
    }

    String string(ASCIILiteral field)
    {
        RefPtr fieldValue = value(field);
        if (!fieldValue)
            return { };
        auto string = nonEmptyString(*fieldValue);
        if (string.isNull())
            m_parser.reject(m_location, Kind::InvalidString, field);
        return string;
    }

    Vector<String> strings(ASCIILiteral field)
    {
        RefPtr fieldValue = value(field);
        if (!fieldValue)
            return { };
        auto strings = nonEmptyStrings(*fieldValue);
        if (!strings) {
            m_parser.reject(m_location, Kind::InvalidStringArray, field);
            return { };
        }
        return WTF::move(*strings);
    }

    Vector<QuirkMatchPattern> patterns(ASCIILiteral field)
    {
        return WTF::compactMap(strings(field), [&](const String& string) {
            auto pattern = QuirkMatchPattern::parse(string);
            if (!pattern)
                m_parser.reject(m_location, Kind::InvalidMatchPattern, field, string);
            return pattern;
        });
    }

private:
    QuirkTableParser& m_parser;
    const JSON::Object& m_object;
    const ErrorLocation& m_location;
    Vector<ASCIILiteral, 2> m_disallowedFields;
};

QuirkTableParseResult QuirkTableParser::parse(StringView json)
{
    RefPtr root = JSON::Value::parseJSON(json);
    RefPtr rootObject = root ? root->asObject() : nullptr;
    RefPtr rows = rootObject && rootObject->size() == 1 ? rootObject->getArray("quirks"_s) : nullptr;
    if (!rows) {
        reject({ }, Kind::InvalidDocument);
        return { { }, WTF::move(m_errors) };
    }

    RuntimeQuirkTable table;
    table.quirks.reserveInitialCapacity(rows->length());
    size_t index = 0;
    for (auto& row : *rows) {
        if (auto quirk = parseRow(row, index++))
            table.quirks.append(WTF::move(*quirk));
    }
    table.quirks.shrinkToFit();
    return { WTF::move(table), WTF::move(m_errors) };
}

std::optional<RuntimeQuirk> QuirkTableParser::parseRow(JSON::Value& value, size_t index)
{
    ErrorLocation location;
    location.row = index;

    RefPtr object = value.asObject();
    if (!object) {
        reject(location, Kind::InvalidRow);
        return std::nullopt;
    }

    RefPtr patterns = object->getArray("matches"_s);
    if (!patterns)
        patterns = object->getArray("embeddedMatches"_s);
    if (patterns && patterns->length())
        location.rowPattern = nonEmptyString(patterns->get(0));

    auto errorCount = m_errors.size();
    Fields fields { *this, *object, location };
    fields.rejectUnknown(rowFields);
    fields.validateBugsAndComment();

    RuntimeQuirk quirk;
    quirk.matches = fields.patterns("matches"_s);
    quirk.embeddedMatches = fields.patterns("embeddedMatches"_s);
    quirk.excludeMatches = fields.patterns("excludeMatches"_s);
    quirk.queryContains = fields.string("queryContains"_s);
    quirk.fragmentContains = fields.string("fragmentContains"_s);

    if (!fields.contains("matches"_s) && !fields.contains("embeddedMatches"_s))
        reject(location, Kind::MissingMatches);

    if (RefPtr environment = fields.value("environment"_s)) {
        auto name = nonEmptyString(*environment);
        quirk.environment = enumeratorFromName<URLEnvironment>(name);
        if (!quirk.environment)
            reject(location, Kind::InvalidEnvironment, "environment"_s, WTF::move(name));
    }

    bool isAvailable = true;
    if (RefPtr available = fields.value("available"_s)) {
        auto expression = nonEmptyString(*available);
        auto evaluated = evaluateBuildConditions(expression);
        if (!evaluated)
            reject(location, Kind::InvalidAvailableExpression, "available"_s, WTF::move(expression));
        isAvailable = evaluated.value_or(false);
    }

    quirk.behaviors = parseBehaviors(*object, location);

    const bool hasNewErrors = m_errors.size() != errorCount;
    if (hasNewErrors || !isAvailable || quirk.behaviors.isEmpty())
        return std::nullopt;
    return quirk;
}

Vector<RuntimeQuirkBehavior> QuirkTableParser::parseBehaviors(const JSON::Object& row, const ErrorLocation& rowLocation)
{
    RefPtr values = row.getArray("behaviors"_s);
    if (!values || !values->length()) {
        reject(rowLocation, Kind::InvalidBehaviors, "behaviors"_s);
        return { };
    }

    Vector<RuntimeQuirkBehavior> behaviors;
    behaviors.reserveInitialCapacity(values->length());
    QuirkBitSet seenIDs;
    for (auto [index, value] : WTF::indexedRange(*values)) {
        auto behavior = parseBehavior(value, rowLocation, index);
        if (!behavior)
            continue;

        if (seenIDs.testAndSet(static_cast<size_t>(behavior->id))) {
            reject(rowLocation, Kind::DuplicateBehavior, "behaviors"_s, enumName(behavior->id));
            continue;
        }

        if (quirkBehaviorDeclaration(behavior->id).isAvailable)
            behaviors.append(WTF::move(*behavior));
    }

    behaviors.shrinkToFit();
    return behaviors;
}

std::optional<RuntimeQuirkBehavior> QuirkTableParser::parseBehavior(JSON::Value& value, const ErrorLocation& rowLocation, size_t index)
{
    auto location = rowLocation;
    location.behavior = index;
    RefPtr object = value.asObject();
    if (!object) {
        reject(location, Kind::InvalidBehavior);
        return std::nullopt;
    }

    auto errorCount = m_errors.size();
    Fields fields { *this, *object, location };
    fields.rejectUnknown(behaviorFields);
    fields.validateBugsAndComment();

    fields.validatePresence("id"_s, true, true);
    auto name = fields.string("id"_s);
    if (name.isNull())
        return std::nullopt;
    auto id = enumeratorFromName<QuirkBehaviorID>(name);
    if (!id) {
        reject(location, Kind::UnknownBehavior, "id"_s, WTF::move(name));
        return std::nullopt;
    }
    location.behaviorID = *id;

    auto& declaration = quirkBehaviorDeclaration(*id);
    for (auto [field, parameter] : parameterFields) {
        const bool isDeclared = declaration.quirkParametersNeeded.contains(parameter);
        fields.validatePresence(field, isDeclared, isDeclared);
    }

    for (auto [field, condition] : conditionFields) {
        const bool isSupported = declaration.quirkConditionsSupported.contains(condition);
        const bool isNeeded = declaration.quirkConditionsNeeded.contains(condition);
        fields.validatePresence(field, isSupported, isNeeded);
    }

    RuntimeQuirkBehavior behavior {
        .id = *id,
        .script = fields.string("script"_s),
        .userAgent = fields.string("userAgent"_s),
        .chromeCompatibilityVersion = fields.string("chromeCompatibilityVersion"_s),
        .cookieNames = fields.strings("cookieNames"_s),
        .elementSelector = fields.string("elementSelector"_s),
        .documentSelector = fields.string("documentSelector"_s),
        .secondaryURL = fields.patterns("secondaryURL"_s),
    };

    const bool hasNewErrors = m_errors.size() != errorCount;
    if (hasNewErrors)
        return std::nullopt;
    return behavior;
}

} // namespace

QuirkTableParseResult parseQuirkTable(StringView json)
{
    return QuirkTableParser { }.parse(json);
}

static String message(const QuirkTableParseError& error)
{
    using enum QuirkTableParseErrorKind;

    switch (error.kind) {
    case InvalidDocument:
        return "the top level must be an object whose only key is \"quirks\", an array of rows"_s;
    case InvalidRow:
        return "a row must be an object"_s;
    case InvalidBehavior:
        return "a behavior must be an object"_s;
    case UnknownField:
        return makeString("unknown field \""_s, error.field, '"');
    case MissingField:
        return makeString("needs \""_s, error.field, '"');
    case DisallowedField:
        return makeString("does not take \""_s, error.field, '"');
    case MissingMatches:
        return "a row must have \"matches\" or \"embeddedMatches\""_s;
    case InvalidString:
        return makeString('"', error.field, "\" must be a non-empty string"_s);
    case InvalidStringArray:
        return makeString('"', error.field, "\" must be a non-empty array of strings"_s);
    case InvalidMatchPattern:
        return makeString("invalid match pattern \""_s, error.value, "\" in \""_s, error.field, '"');
    case InvalidEnvironment:
        return "\"environment\" must name a URLEnvironment"_s;
    case InvalidAvailableExpression:
        return "\"available\" must be build condition names combined with !, &&, || and parentheses"_s;
    case InvalidBehaviors:
        return "\"behaviors\" must be a non-empty array"_s;
    case UnknownBehavior:
        return makeString("\"id\" \""_s, error.value, "\" is not a behavior in QuirkBehaviors.yaml"_s);
    case DuplicateBehavior:
        return makeString("\"behaviors\" lists "_s, error.value, " more than once"_s);
    case InvalidBugs:
        return "\"bugs\" must be a non-empty array of rdar://N or https://webkit.org/b/N references"_s;
    case DuplicateBug:
        return makeString("\"bugs\" lists "_s, error.value, " more than once"_s);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

String QuirkTableParseError::description() const
{
    if (!location.row)
        return message(*this);

    StringBuilder builder;
    builder.append("quirks["_s, *location.row, ']');

    if (!location.rowPattern.isNull())
        builder.append(" ("_s, location.rowPattern, ')');

    if (location.behavior) {
        builder.append(".behaviors["_s, *location.behavior, ']');
        if (location.behaviorID)
            builder.append(' ', enumName(*location.behaviorID));
    }

    builder.append(": "_s, message(*this));
    return builder.toString();
}

} // namespace WebCore
