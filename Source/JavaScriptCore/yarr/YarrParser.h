/*
 * Copyright (C) 2009-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Alexey Shvayka <shvaikalesh@gmail.com>.
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

#include "Yarr.h"
#include "YarrPattern.h"
#include "YarrUnicodeProperties.h"
#include <wtf/ASCIICType.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace JSC { namespace Yarr {

// The Parser class should not be used directly - only via the Yarr::parse() method.
template<class Delegate, typename CharType>
class Parser {
private:
    template<class FriendDelegate>
    friend ErrorCode parse(FriendDelegate&, StringView pattern, bool isUnicode, unsigned backReferenceLimit, bool isNamedForwardReferenceAllowed);

    enum class UnicodeParseContext : uint8_t { PatternCodePoint, GroupName };

    /*
     * CharacterClassParserDelegate:
     *
     * The class CharacterClassParserDelegate is used in the parsing of character
     * classes.  This class handles detection of character ranges.  This class
     * implements enough of the delegate interface such that it can be passed to
     * parseEscape() as an EscapeDelegate.  This allows parseEscape() to be reused
     * to perform the parsing of escape characters in character sets.
     */
    class CharacterClassParserDelegate {
    public:
        CharacterClassParserDelegate(Delegate& delegate, ErrorCode& err, bool isUnicode)
            : m_delegate(delegate)
            , m_errorCode(err)
            , m_isUnicode(isUnicode)
            , m_state(CharacterClassConstructionState::Empty)
            , m_character(0)
        {
        }

        /*
         * begin():
         *
         * Called at beginning of construction.
         */
        void begin(bool invert)
        {
            m_delegate.atomCharacterClassBegin(invert);
        }

        /*
         * atomPatternCharacter():
         *
         * This method is called either from parseCharacterClass() (for an unescaped
         * character in a character class), or from parseEscape(). In the former case
         * the value true will be passed for the argument 'hyphenIsRange', and in this
         * mode we will allow a hypen to be treated as indicating a range (i.e. /[a-z]/
         * is different to /[a\-z]/).
         */
        void atomPatternCharacter(UChar32 ch, bool hyphenIsRange = false)
        {
            switch (m_state) {
            case CharacterClassConstructionState::AfterCharacterClass:
                // Following a built-in character class we need look out for a hyphen.
                // We're looking for invalid ranges, such as /[\d-x]/ or /[\d-\d]/.
                // If we see a hyphen following a character class then unlike usual
                // we'll report it to the delegate immediately, and put ourself into
                // a poisoned state. In a unicode pattern, any following calls to add
                // another character or character class will result in syntax error.
                // A hypen following a character class is itself valid, but only at
                // the end of a regex.
                if (hyphenIsRange && ch == '-') {
                    m_delegate.atomCharacterClassAtom('-');
                    m_state = CharacterClassConstructionState::AfterCharacterClassHyphen;
                    return;
                }
                // Otherwise just fall through - cached character so treat this as CharacterClassConstructionState::Empty.
                FALLTHROUGH;

            case CharacterClassConstructionState::Empty:
                m_character = ch;
                m_state = CharacterClassConstructionState::CachedCharacter;
                return;

            case CharacterClassConstructionState::CachedCharacter:
                if (hyphenIsRange && ch == '-')
                    m_state = CharacterClassConstructionState::CachedCharacterHyphen;
                else {
                    m_delegate.atomCharacterClassAtom(m_character);
                    m_character = ch;
                }
                return;

            case CharacterClassConstructionState::CachedCharacterHyphen:
                if (ch < m_character) {
                    m_errorCode = ErrorCode::CharacterClassRangeOutOfOrder;
                    return;
                }
                m_delegate.atomCharacterClassRange(m_character, ch);
                m_state = CharacterClassConstructionState::Empty;
                return;

                // If we hit this case, we have an invalid range like /[\d-a]/.
                // See coment in atomBuiltInCharacterClass() below.
            case CharacterClassConstructionState::AfterCharacterClassHyphen:
                if (m_isUnicode) {
                    m_errorCode = ErrorCode::CharacterClassRangeInvalid;
                    return;
                }
                m_delegate.atomCharacterClassAtom(ch);
                m_state = CharacterClassConstructionState::Empty;
                return;
            }
        }

        /*
         * atomBuiltInCharacterClass():
         *
         * Adds a built-in character class, called by parseEscape().
         */
        void atomBuiltInCharacterClass(BuiltInCharacterClassID classID, bool invert)
        {
            switch (m_state) {
            case CharacterClassConstructionState::CachedCharacter:
                // Flush the currently cached character, then fall through.
                m_delegate.atomCharacterClassAtom(m_character);
                FALLTHROUGH;
            case CharacterClassConstructionState::Empty:
            case CharacterClassConstructionState::AfterCharacterClass:
                m_delegate.atomCharacterClassBuiltIn(classID, invert);
                m_state = CharacterClassConstructionState::AfterCharacterClass;
                return;

                // If we hit either of these cases, we have an invalid range that
                // looks something like /[a-\d]/ or /[\d-\d]/.
                // Since ES2015, this should be syntax error in a unicode pattern,
                // yet gracefully handled in a regular regex to avoid breaking the web.
                // Effectively we handle the hyphen as if it was (implicitly) escaped,
                // e.g. /[\d-a-z]/ is treated as /[\d\-a\-z]/.
                // See usages of CharacterRangeOrUnion abstract op in
                // https://tc39.es/ecma262/#sec-regular-expression-patterns-semantics
            case CharacterClassConstructionState::CachedCharacterHyphen:
                m_delegate.atomCharacterClassAtom(m_character);
                m_delegate.atomCharacterClassAtom('-');
                FALLTHROUGH;
            case CharacterClassConstructionState::AfterCharacterClassHyphen:
                if (m_isUnicode) {
                    m_errorCode = ErrorCode::CharacterClassRangeInvalid;
                    return;
                }
                m_delegate.atomCharacterClassBuiltIn(classID, invert);
                m_state = CharacterClassConstructionState::Empty;
                return;
            }
        }

        /*
         * end():
         *
         * Called at end of construction.
         */
        void end()
        {
            if (m_state == CharacterClassConstructionState::CachedCharacter)
                m_delegate.atomCharacterClassAtom(m_character);
            else if (m_state == CharacterClassConstructionState::CachedCharacterHyphen) {
                m_delegate.atomCharacterClassAtom(m_character);
                m_delegate.atomCharacterClassAtom('-');
            }
            m_delegate.atomCharacterClassEnd();
        }

        // parseEscape() should never call these delegate methods when
        // invoked with inCharacterClass set.
        NO_RETURN_DUE_TO_ASSERT void assertionWordBoundary(bool) { RELEASE_ASSERT_NOT_REACHED(); }
        NO_RETURN_DUE_TO_ASSERT void atomBackReference(unsigned) { RELEASE_ASSERT_NOT_REACHED(); }
        NO_RETURN_DUE_TO_ASSERT void atomNamedBackReference(const String&) { RELEASE_ASSERT_NOT_REACHED(); }
        NO_RETURN_DUE_TO_ASSERT void atomNamedForwardReference(const String&) { RELEASE_ASSERT_NOT_REACHED(); }

    private:
        Delegate& m_delegate;
        ErrorCode& m_errorCode;
        bool m_isUnicode;
        enum class CharacterClassConstructionState {
            Empty,
            CachedCharacter,
            CachedCharacterHyphen,
            AfterCharacterClass,
            AfterCharacterClassHyphen,
        };
        CharacterClassConstructionState m_state;
        UChar32 m_character;
    };

    Parser(Delegate& delegate, StringView pattern, bool isUnicode, unsigned backReferenceLimit, bool isNamedForwardReferenceAllowed)
        : m_delegate(delegate)
        , m_data(pattern.characters<CharType>())
        , m_size(pattern.length())
        , m_isUnicode(isUnicode)
        , m_backReferenceLimit(backReferenceLimit)
        , m_isNamedForwardReferenceAllowed(isNamedForwardReferenceAllowed)
    {
    }

    // The handling of IdentityEscapes is different depending on the unicode flag.
    // For Unicode patterns, IdentityEscapes only include SyntaxCharacters or '/'.
    // For non-unicode patterns, most any character can be escaped.
    bool isIdentityEscapeAnError(int ch)
    {
        if (m_isUnicode && (!strchr("^$\\.*+?()[]{}|/", ch) || !ch)) {
            m_errorCode = ErrorCode::InvalidIdentityEscape;
            return true;
        }

        return false;
    }

    /*
     * parseEscape():
     *
     * Helper for parseTokens() AND parseCharacterClass().
     * Unlike the other parser methods, this function does not report tokens
     * directly to the member delegate (m_delegate), instead tokens are
     * emitted to the delegate provided as an argument.  In the case of atom
     * escapes, parseTokens() will call parseEscape() passing m_delegate as
     * an argument, and as such the escape will be reported to the delegate.
     *
     * However this method may also be used by parseCharacterClass(), in which
     * case a CharacterClassParserDelegate will be passed as the delegate that
     * tokens should be added to.  A boolean flag is also provided to indicate
     * whether that an escape in a CharacterClass is being parsed (some parsing
     * rules change in this context).
     *
     * The boolean value returned by this method indicates whether the token
     * parsed was an atom (outside of a characted class \b and \B will be
     * interpreted as assertions).
     */
    template<bool inCharacterClass, class EscapeDelegate>
    bool parseEscape(EscapeDelegate& delegate)
    {
        ASSERT(!hasError(m_errorCode));
        ASSERT(peek() == '\\');
        consume();

        if (atEndOfPattern()) {
            m_errorCode = ErrorCode::EscapeUnterminated;
            return false;
        }

        switch (peek()) {
        // Assertions
        case 'b':
            consume();
            if (inCharacterClass)
                delegate.atomPatternCharacter('\b');
            else {
                delegate.assertionWordBoundary(false);
                return false;
            }
            break;
        case 'B':
            consume();
            if (inCharacterClass) {
                if (isIdentityEscapeAnError('B'))
                    break;

                delegate.atomPatternCharacter('B');
            } else {
                delegate.assertionWordBoundary(true);
                return false;
            }
            break;

        // CharacterClassEscape
        case 'd':
            consume();
            delegate.atomBuiltInCharacterClass(BuiltInCharacterClassID::DigitClassID, false);
            break;
        case 's':
            consume();
            delegate.atomBuiltInCharacterClass(BuiltInCharacterClassID::SpaceClassID, false);
            break;
        case 'w':
            consume();
            delegate.atomBuiltInCharacterClass(BuiltInCharacterClassID::WordClassID, false);
            break;
        case 'D':
            consume();
            delegate.atomBuiltInCharacterClass(BuiltInCharacterClassID::DigitClassID, true);
            break;
        case 'S':
            consume();
            delegate.atomBuiltInCharacterClass(BuiltInCharacterClassID::SpaceClassID, true);
            break;
        case 'W':
            consume();
            delegate.atomBuiltInCharacterClass(BuiltInCharacterClassID::WordClassID, true);
            break;

        case '0': {
            consume();

            if (!peekIsDigit()) {
                delegate.atomPatternCharacter(0);
                break;
            }

            if (m_isUnicode) {
                m_errorCode = ErrorCode::InvalidOctalEscape;
                break;
            }

            delegate.atomPatternCharacter(consumeOctal(2));
            break;
        }

        // DecimalEscape
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
            // For non-Unicode patterns, invalid backreferences are parsed as octal or decimal escapes.
            // First, try to parse this as backreference.
            if (!inCharacterClass) {
                ParseState state = saveState();

                unsigned backReference = consumeNumber();
                if (backReference <= m_backReferenceLimit) {
                    m_maxSeenBackReference = std::max(m_maxSeenBackReference, backReference);
                    delegate.atomBackReference(backReference);
                    break;
                }

                restoreState(state);
                if (m_isUnicode) {
                    m_errorCode = ErrorCode::InvalidBackreference;
                    break;
                }
            }

            if (m_isUnicode) {
                m_errorCode = ErrorCode::InvalidOctalEscape;
                break;
            }

            delegate.atomPatternCharacter(peek() < '8' ? consumeOctal(3) : consume());
            break;
        }

        // ControlEscape
        case 'f':
            consume();
            delegate.atomPatternCharacter('\f');
            break;
        case 'n':
            consume();
            delegate.atomPatternCharacter('\n');
            break;
        case 'r':
            consume();
            delegate.atomPatternCharacter('\r');
            break;
        case 't':
            consume();
            delegate.atomPatternCharacter('\t');
            break;
        case 'v':
            consume();
            delegate.atomPatternCharacter('\v');
            break;

        // ControlLetter
        case 'c': {
            ParseState state = saveState();
            consume();
            if (!atEndOfPattern()) {
                int control = consume();

                if (WTF::isASCIIAlpha(control)) {
                    delegate.atomPatternCharacter(control & 0x1f);
                    break;
                }

                if (m_isUnicode) {
                    m_errorCode = ErrorCode::InvalidControlLetterEscape;
                    break;
                }

                // https://tc39.es/ecma262/#prod-annexB-ClassControlLetter
                if (inCharacterClass && (WTF::isASCIIDigit(control) || control == '_')) {
                    delegate.atomPatternCharacter(control & 0x1f);
                    break;
                }
            }

            if (m_isUnicode) {
                m_errorCode = ErrorCode::InvalidIdentityEscape;
                break;
            }

            restoreState(state);
            delegate.atomPatternCharacter('\\');
            break;
        }

        // HexEscape
        case 'x': {
            consume();
            int x = tryConsumeHex(2);
            if (x == -1) {
                if (isIdentityEscapeAnError('x'))
                    break;

                delegate.atomPatternCharacter('x');
            } else
                delegate.atomPatternCharacter(x);
            break;
        }

        // Named backreference
        case 'k': {
            consume();
            ParseState state = saveState();
            if (!inCharacterClass && tryConsume('<')) {
                auto groupName = tryConsumeGroupName();
                if (hasError(m_errorCode))
                    break;

                if (groupName) {
                    if (m_captureGroupNames.contains(groupName.value())) {
                        delegate.atomNamedBackReference(groupName.value());
                        break;
                    }

                    if (m_isNamedForwardReferenceAllowed) {
                        m_forwardReferenceNames.add(groupName.value());
                        delegate.atomNamedForwardReference(groupName.value());
                        break;
                    }
                }
            }

            restoreState(state);
            if (!isIdentityEscapeAnError('k')) {
                delegate.atomPatternCharacter('k');
                m_kIdentityEscapeSeen = true; 
            }
            break;
        }

        // Unicode property escapes
        case 'p':
        case 'P': {
            int escapeChar = consume();

            if (!m_isUnicode) {
                if (isIdentityEscapeAnError(escapeChar))
                    break;
                delegate.atomPatternCharacter(escapeChar);
                break;
            }

            if (!atEndOfPattern() && peek() == '{') {
                consume();
                auto optClassID = tryConsumeUnicodePropertyExpression();
                if (!optClassID) {
                    // tryConsumeUnicodePropertyExpression() will set m_errorCode for a malformed property expression
                    break;
                }
                delegate.atomBuiltInCharacterClass(optClassID.value(), escapeChar == 'P');
            } else
                m_errorCode = ErrorCode::InvalidUnicodePropertyExpression;
            break;
        }

        // UnicodeEscape
        case 'u': {
            int codePoint = tryConsumeUnicodeEscape<UnicodeParseContext::PatternCodePoint>();
            if (hasError(m_errorCode))
                break;

            delegate.atomPatternCharacter(codePoint == -1 ? 'u' : codePoint);
            break;
        }

        // IdentityEscape
        default:
            int ch = peek();

            if (ch == '-' && m_isUnicode && inCharacterClass) {
                // \- is allowed for ClassEscape with unicode flag.
                delegate.atomPatternCharacter(consume());
                break;
            }

            if (isIdentityEscapeAnError(ch))
                break;

            delegate.atomPatternCharacter(consume());
        }
        
        return true;
    }

    template<UnicodeParseContext context>
    UChar32 consumePossibleSurrogatePair()
    {
        bool unicodePatternOrGroupName = m_isUnicode || context == UnicodeParseContext::GroupName;

        UChar32 ch = consume();
        if (U16_IS_LEAD(ch) && unicodePatternOrGroupName && !atEndOfPattern()) {
            ParseState state = saveState();

            UChar32 surrogate2 = consume();
            if (U16_IS_TRAIL(surrogate2))
                ch = U16_GET_SUPPLEMENTARY(ch, surrogate2);
            else
                restoreState(state);
        }

        return ch;
    }

    /*
     * parseAtomEscape(), parseCharacterClassEscape():
     *
     * These methods alias to parseEscape().
     */
    bool parseAtomEscape()
    {
        return parseEscape<false>(m_delegate);
    }
    void parseCharacterClassEscape(CharacterClassParserDelegate& delegate)
    {
        parseEscape<true>(delegate);
    }

    /*
     * parseCharacterClass():
     *
     * Helper for parseTokens(); calls directly and indirectly (via parseCharacterClassEscape)
     * to an instance of CharacterClassParserDelegate, to describe the character class to the
     * delegate.
     */
    void parseCharacterClass()
    {
        ASSERT(!hasError(m_errorCode));
        ASSERT(peek() == '[');
        consume();

        CharacterClassParserDelegate characterClassConstructor(m_delegate, m_errorCode, m_isUnicode);

        characterClassConstructor.begin(tryConsume('^'));

        while (!atEndOfPattern()) {
            switch (peek()) {
            case ']':
                consume();
                characterClassConstructor.end();
                return;

            case '\\':
                parseCharacterClassEscape(characterClassConstructor);
                break;

            default:
                characterClassConstructor.atomPatternCharacter(consumePossibleSurrogatePair<UnicodeParseContext::PatternCodePoint>(), true);
            }

            if (hasError(m_errorCode))
                return;
        }

        m_errorCode = ErrorCode::CharacterClassUnmatched;
    }

    /*
     * parseParenthesesBegin():
     *
     * Helper for parseTokens(); checks for parentheses types other than regular capturing subpatterns.
     */
    void parseParenthesesBegin()
    {
        ASSERT(!hasError(m_errorCode));
        ASSERT(peek() == '(');
        consume();

        auto type = ParenthesesType::Subpattern;

        if (tryConsume('?')) {
            if (atEndOfPattern()) {
                m_errorCode = ErrorCode::ParenthesesTypeInvalid;
                return;
            }

            switch (consume()) {
            case ':':
                m_delegate.atomParenthesesSubpatternBegin(false);
                break;
            
            case '=':
                m_delegate.atomParentheticalAssertionBegin(false, Forward);
                type = ParenthesesType::Assertion;
                break;

            case '!':
                m_delegate.atomParentheticalAssertionBegin(true, Forward);
                type = ParenthesesType::Assertion;
                break;

            case '<': {
                auto groupName = tryConsumeGroupName();
                if (hasError(m_errorCode))
                    break;

                if (groupName) {
                    if (m_kIdentityEscapeSeen) {
                        m_errorCode = ErrorCode::InvalidNamedBackReference;
                        break;
                    }

                    auto setAddResult = m_captureGroupNames.add(groupName.value());
                    if (setAddResult.isNewEntry)
                        m_delegate.atomParenthesesSubpatternBegin(true, groupName);
                    else
                        m_errorCode = ErrorCode::DuplicateGroupName;
                } else {
                    if (tryConsume('=')) {
                        m_delegate.atomParentheticalAssertionBegin(false, Backward);
                        type = ParenthesesType::Assertion;
                        break;
                    }

                    if (tryConsume('!')) {
                        m_delegate.atomParentheticalAssertionBegin(true, Backward);
                        type = ParenthesesType::Assertion;
                        break;
                    }
                    m_errorCode = ErrorCode::InvalidGroupName;
                }

                break;
            }

            default:
                m_errorCode = ErrorCode::ParenthesesTypeInvalid;
            }
        } else
            m_delegate.atomParenthesesSubpatternBegin();

        if (type == ParenthesesType::Subpattern)
            ++m_numSubpatterns;

        m_parenthesesStack.append(type);
    }

    /*
     * parseParenthesesEnd():
     *
     * Helper for parseTokens(); checks for parse errors (due to unmatched parentheses).
     *
     * The boolean value returned by this method indicates whether the token parsed
     * was either an Atom or, for web compatibility reasons, QuantifiableAssertion
     * in non-Unicode pattern.
     */
    bool parseParenthesesEnd()
    {
        ASSERT(!hasError(m_errorCode));
        ASSERT(peek() == ')');
        consume();

        if (m_parenthesesStack.isEmpty()) {
            m_errorCode = ErrorCode::ParenthesesUnmatched;
            return false;
        }

        m_delegate.atomParenthesesEnd();
        auto type = m_parenthesesStack.takeLast();
        return type == ParenthesesType::Subpattern || !m_isUnicode;
    }

    /*
     * parseQuantifier():
     *
     * Helper for parseTokens(); checks for parse errors and non-greedy quantifiers.
     */
    void parseQuantifier(bool lastTokenWasAnAtom, unsigned min, unsigned max)
    {
        ASSERT(!hasError(m_errorCode));
        ASSERT(min <= max);

        if (min == UINT_MAX) {
            m_errorCode = ErrorCode::QuantifierTooLarge;
            return;
        }

        if (lastTokenWasAnAtom)
            m_delegate.quantifyAtom(min, max, !tryConsume('?'));
        else
            m_errorCode = ErrorCode::QuantifierWithoutAtom;
    }

    /*
     * parseTokens():
     *
     * This method loops over the input pattern reporting tokens to the delegate.
     * The method returns when a parse error is detected, or the end of the pattern
     * is reached.  One piece of state is tracked around the loop, which is whether
     * the last token passed to the delegate was an atom (this is necessary to detect
     * a parse error when a quantifier provided without an atom to quantify).
     */
    void parseTokens()
    {
        bool lastTokenWasAnAtom = false;

        while (!atEndOfPattern()) {
            switch (peek()) {
            case '|':
                consume();
                m_delegate.disjunction();
                lastTokenWasAnAtom = false;
                break;

            case '(':
                parseParenthesesBegin();
                lastTokenWasAnAtom = false;
                break;

            case ')':
                lastTokenWasAnAtom = parseParenthesesEnd();
                break;

            case '^':
                consume();
                m_delegate.assertionBOL();
                lastTokenWasAnAtom = false;
                break;

            case '$':
                consume();
                m_delegate.assertionEOL();
                lastTokenWasAnAtom = false;
                break;

            case '.':
                consume();
                m_delegate.atomBuiltInCharacterClass(BuiltInCharacterClassID::DotClassID, false);
                lastTokenWasAnAtom = true;
                break;

            case '[':
                parseCharacterClass();
                lastTokenWasAnAtom = true;
                break;

            case ']':
            case '}':
                if (m_isUnicode) {
                    m_errorCode = ErrorCode::BracketUnmatched;
                    break;
                }

                m_delegate.atomPatternCharacter(consume());
                lastTokenWasAnAtom = true;
                break;

            case '\\':
                lastTokenWasAnAtom = parseAtomEscape();
                break;

            case '*':
                consume();
                parseQuantifier(lastTokenWasAnAtom, 0, quantifyInfinite);
                lastTokenWasAnAtom = false;
                break;

            case '+':
                consume();
                parseQuantifier(lastTokenWasAnAtom, 1, quantifyInfinite);
                lastTokenWasAnAtom = false;
                break;

            case '?':
                consume();
                parseQuantifier(lastTokenWasAnAtom, 0, 1);
                lastTokenWasAnAtom = false;
                break;

            case '{': {
                ParseState state = saveState();

                consume();
                if (peekIsDigit()) {
                    unsigned min = consumeNumber();
                    unsigned max = min;
                    
                    if (tryConsume(','))
                        max = peekIsDigit() ? consumeNumber() : quantifyInfinite;

                    if (tryConsume('}')) {
                        if (min <= max)
                            parseQuantifier(lastTokenWasAnAtom, min, max);
                        else
                            m_errorCode = ErrorCode::QuantifierOutOfOrder;
                        lastTokenWasAnAtom = false;
                        break;
                    }
                }

                if (m_isUnicode) {
                    m_errorCode = ErrorCode::QuantifierIncomplete;
                    break;
                }

                restoreState(state);
                // if we did not find a complete quantifer, fall through to the default case.
                FALLTHROUGH;
            }

            default:
                m_delegate.atomPatternCharacter(consumePossibleSurrogatePair<UnicodeParseContext::PatternCodePoint>());
                lastTokenWasAnAtom = true;
            }

            if (hasError(m_errorCode))
                return;
        }

        if (!m_parenthesesStack.isEmpty())
            m_errorCode = ErrorCode::MissingParentheses;
    }

    /*
     * parse():
     *
     * This method calls parseTokens() to parse over the input and returns error code for a result.
     */
    ErrorCode parse()
    {
        if (m_size > MAX_PATTERN_SIZE)
            return ErrorCode::PatternTooLarge;

        parseTokens();

        if (!hasError(m_errorCode)) {
            ASSERT(atEndOfPattern());
            handleIllegalReferences();
            ASSERT(atEndOfPattern());
        }

        return m_errorCode;
    }

    void handleIllegalReferences()
    {
        bool shouldReparse = false;

        if (m_maxSeenBackReference > m_numSubpatterns) {
            // Contains illegal numeric backreference. See https://tc39.es/ecma262/#prod-annexB-AtomEscape
            if (m_isUnicode) {
                m_errorCode = ErrorCode::InvalidBackreference;
                return;
            }

            m_backReferenceLimit = m_numSubpatterns;
            shouldReparse = true;
        }

        if (m_kIdentityEscapeSeen && !m_captureGroupNames.isEmpty()) {
            m_errorCode = ErrorCode::InvalidNamedBackReference;
            return;
        }

        if (containsIllegalNamedForwardReference()) {
            // \k<a> is parsed as named reference in Unicode patterns because of strict IdentityEscape grammar.
            // See https://tc39.es/ecma262/#sec-patterns-static-semantics-early-errors
            if (m_isUnicode || !m_captureGroupNames.isEmpty()) {
                m_errorCode = ErrorCode::InvalidNamedBackReference;
                return;
            }

            m_isNamedForwardReferenceAllowed = false;
            shouldReparse = true;
        }

        if (shouldReparse) {
            resetForReparsing();
            parseTokens();
        }
    }

    bool containsIllegalNamedForwardReference()
    {
        if (m_forwardReferenceNames.isEmpty())
            return false;

        if (m_captureGroupNames.isEmpty())
            return true;

        for (auto& entry : m_forwardReferenceNames) {
            if (!m_captureGroupNames.contains(entry))
                return true;
        }

        return false;
    }

    void resetForReparsing()
    {
        ASSERT(!hasError(m_errorCode));

        m_delegate.resetForReparsing();
        m_index = 0;
        m_numSubpatterns = 0;
        m_maxSeenBackReference = 0;
        m_kIdentityEscapeSeen = false;
        m_parenthesesStack.clear();
        m_captureGroupNames.clear();
        m_forwardReferenceNames.clear();
    }

    // Misc helper functions:

    typedef unsigned ParseState;
    
    ParseState saveState()
    {
        return m_index;
    }

    void restoreState(ParseState state)
    {
        m_index = state;
    }

    bool atEndOfPattern()
    {
        ASSERT(m_index <= m_size);
        return m_index == m_size;
    }

    unsigned patternRemaining()
    {
        ASSERT(m_index <= m_size);
        return m_size - m_index;
    }

    int peek()
    {
        ASSERT(m_index < m_size);
        return m_data[m_index];
    }

    bool peekIsDigit()
    {
        return !atEndOfPattern() && WTF::isASCIIDigit(peek());
    }

    unsigned peekDigit()
    {
        ASSERT(peekIsDigit());
        return peek() - '0';
    }

    template<UnicodeParseContext context>
    int tryConsumeUnicodeEscape()
    {
        ASSERT(!hasError(m_errorCode));

        bool unicodePatternOrGroupName = m_isUnicode || context == UnicodeParseContext::GroupName;

        if (!tryConsume('u') || atEndOfPattern()) {
            if (unicodePatternOrGroupName)
                m_errorCode = ErrorCode::InvalidUnicodeEscape;
            return -1;
        }

        if (unicodePatternOrGroupName && tryConsume('{')) {
            int codePoint = 0;
            do {
                if (atEndOfPattern() || !isASCIIHexDigit(peek())) {
                    m_errorCode = ErrorCode::InvalidUnicodeCodePointEscape;
                    return -1;
                }

                codePoint = (codePoint << 4) | toASCIIHexValue(consume());

                if (codePoint > UCHAR_MAX_VALUE) {
                    m_errorCode = ErrorCode::InvalidUnicodeCodePointEscape;
                    return -1;
                }
            } while (!atEndOfPattern() && peek() != '}');

            if (!tryConsume('}')) {
                m_errorCode = ErrorCode::InvalidUnicodeCodePointEscape; 
                return -1;
            }

            return codePoint;
        }

        int codeUnit = tryConsumeHex(4);
        if (codeUnit == -1) {
            if (unicodePatternOrGroupName)
                m_errorCode = ErrorCode::InvalidUnicodeEscape;
            return -1;
        }

        // If we have the first of a surrogate pair, look for the second.
        if (U16_IS_LEAD(codeUnit) && unicodePatternOrGroupName && patternRemaining() >= 6 && peek() == '\\') {
            ParseState state = saveState();
            consume();

            if (tryConsume('u')) {
                int surrogate2 = tryConsumeHex(4);
                if (U16_IS_TRAIL(surrogate2))
                    return U16_GET_SUPPLEMENTARY(codeUnit, surrogate2);
            }

            restoreState(state);
        }

        return codeUnit;
    }

    int tryConsumeIdentifierCharacter()
    {
        if (tryConsume('\\'))
            return tryConsumeUnicodeEscape<UnicodeParseContext::GroupName>();

        return consumePossibleSurrogatePair<UnicodeParseContext::GroupName>();
    }

    bool isIdentifierStart(int ch)
    {
        return (WTF::isASCII(ch) && (WTF::isASCIIAlpha(ch) || ch == '_' || ch == '$')) || (U_GET_GC_MASK(ch) & U_GC_L_MASK);
    }

    bool isIdentifierPart(int ch)
    {
        return (WTF::isASCII(ch) && (WTF::isASCIIAlpha(ch) || ch == '_' || ch == '$')) || (U_GET_GC_MASK(ch) & (U_GC_L_MASK | U_GC_MN_MASK | U_GC_MC_MASK | U_GC_ND_MASK | U_GC_PC_MASK)) || ch == 0x200C || ch == 0x200D;
    }

    bool isUnicodePropertyValueExpressionChar(int ch)
    {
        return WTF::isASCIIAlphanumeric(ch) || ch == '_' || ch == '=';
    }

    int consume()
    {
        ASSERT(m_index < m_size);
        return m_data[m_index++];
    }

    unsigned consumeDigit()
    {
        ASSERT(peekIsDigit());
        return consume() - '0';
    }

    unsigned consumeNumber()
    {
        CheckedUint32 n = consumeDigit();
        while (peekIsDigit())
            n = n * 10 + consumeDigit();
        return n.hasOverflowed() ? quantifyInfinite : n.value();
    }

    // https://tc39.es/ecma262/#prod-annexB-LegacyOctalEscapeSequence
    unsigned consumeOctal(unsigned count)
    {
        unsigned octal = 0;
        while (count-- && octal < 32 && !atEndOfPattern() && WTF::isASCIIOctalDigit(peek()))
            octal = octal * 8 + consumeDigit();
        return octal;
    }

    bool tryConsume(UChar ch)
    {
        if (atEndOfPattern() || (m_data[m_index] != ch))
            return false;
        ++m_index;
        return true;
    }

    int tryConsumeHex(int count)
    {
        ParseState state = saveState();

        int n = 0;
        while (count--) {
            if (atEndOfPattern() || !WTF::isASCIIHexDigit(peek())) {
                restoreState(state);
                return -1;
            }
            n = (n << 4) | WTF::toASCIIHexValue(consume());
        }
        return n;
    }

    std::optional<String> tryConsumeGroupName()
    {
        if (atEndOfPattern())
            return std::nullopt;

        ParseState state = saveState();
        
        int ch = tryConsumeIdentifierCharacter();

        if (isIdentifierStart(ch)) {
            StringBuilder identifierBuilder;
            identifierBuilder.appendCharacter(ch);

            while (!atEndOfPattern()) {
                ch = tryConsumeIdentifierCharacter();
                if (ch == '>')
                    return std::optional<String>(identifierBuilder.toString());

                if (!isIdentifierPart(ch))
                    break;

                identifierBuilder.appendCharacter(ch);
            }
        }

        restoreState(state);

        return std::nullopt;
    }

    std::optional<BuiltInCharacterClassID> tryConsumeUnicodePropertyExpression()
    {
        if (atEndOfPattern() || !isUnicodePropertyValueExpressionChar(peek())) {
            m_errorCode = ErrorCode::InvalidUnicodePropertyExpression;
            return std::nullopt;
        }

        StringBuilder expressionBuilder;
        String unicodePropertyName;
        bool foundEquals = false;
        unsigned errors = 0;

        expressionBuilder.appendCharacter(consume());

        while (!atEndOfPattern()) {
            int ch = peek();
            if (ch == '}') {
                consume();
                if (errors) {
                    m_errorCode = ErrorCode::InvalidUnicodePropertyExpression;
                    return std::nullopt;
                }

                if (foundEquals) {
                    auto result = unicodeMatchPropertyValue(unicodePropertyName, expressionBuilder.toString());
                    if (!result)
                        m_errorCode = ErrorCode::InvalidUnicodePropertyExpression;
                    return result;
                }

                auto result = unicodeMatchProperty(expressionBuilder.toString());
                if (!result)
                    m_errorCode = ErrorCode::InvalidUnicodePropertyExpression;
                return result;
            }

            consume();
            if (ch == '=') {
                if (!foundEquals) {
                    foundEquals = true;
                    unicodePropertyName = expressionBuilder.toString();
                    expressionBuilder.clear();
                } else
                    errors++;
            } else if (!isUnicodePropertyValueExpressionChar(ch))
                errors++;
            else
                expressionBuilder.appendCharacter(ch);
        }

        m_errorCode = ErrorCode::InvalidUnicodePropertyExpression;
        return std::nullopt;
    }

    enum class ParenthesesType : uint8_t { Subpattern, Assertion };

    Delegate& m_delegate;
    ErrorCode m_errorCode { ErrorCode::NoError };
    const CharType* m_data;
    unsigned m_size;
    unsigned m_index { 0 };
    bool m_isUnicode;
    unsigned m_backReferenceLimit;
    unsigned m_numSubpatterns { 0 };
    unsigned m_maxSeenBackReference { 0 };
    bool m_isNamedForwardReferenceAllowed;
    bool m_kIdentityEscapeSeen { false };
    Vector<ParenthesesType, 16> m_parenthesesStack;
    HashSet<String> m_captureGroupNames;
    HashSet<String> m_forwardReferenceNames;

    // Derived by empirical testing of compile time in PCRE and WREC.
    static constexpr unsigned MAX_PATTERN_SIZE = 1024 * 1024;
};

/*
 * Yarr::parse():
 *
 * The parse method is passed a pattern to be parsed and a delegate upon which
 * callbacks will be made to record the parsed tokens forming the regex.
 * Yarr::parse() returns null on success, or a const C string providing an error
 * message where a parse error occurs.
 *
 * The Delegate must implement the following interface:
 *
 *    void assertionBOL();
 *    void assertionEOL();
 *    void assertionWordBoundary(bool invert);
 *
 *    void atomPatternCharacter(UChar32 ch);
 *    void atomBuiltInCharacterClass(BuiltInCharacterClassID classID, bool invert);
 *    void atomCharacterClassBegin(bool invert)
 *    void atomCharacterClassAtom(UChar32 ch)
 *    void atomCharacterClassRange(UChar32 begin, UChar32 end)
 *    void atomCharacterClassBuiltIn(BuiltInCharacterClassID classID, bool invert)
 *    void atomCharacterClassEnd()
 *    void atomParenthesesSubpatternBegin(bool capture = true, std::optional<String> groupName);
 *    void atomParentheticalAssertionBegin(bool invert, MatchDirection matchDirection);
 *    void atomParenthesesEnd();
 *    void atomBackReference(unsigned subpatternId);
 *    void atomNamedBackReference(const String& subpatternName);
 *    void atomNamedForwardReference(const String& subpatternName);
 *
 *    void quantifyAtom(unsigned min, unsigned max, bool greedy);
 *
 *    void disjunction();
 *
 *    void resetForReparsing();
 *
 * The regular expression is described by a sequence of assertion*() and atom*()
 * callbacks to the delegate, describing the terms in the regular expression.
 * Following an atom a quantifyAtom() call may occur to indicate that the previous
 * atom should be quantified.  In the case of atoms described across multiple
 * calls (parentheses and character classes) the call to quantifyAtom() will come
 * after the call to the atom*End() method, never after atom*Begin().
 *
 * Character classes may either be described by a single call to
 * atomBuiltInCharacterClass(), or by a sequence of atomCharacterClass*() calls.
 * In the latter case, ...Begin() will be called, followed by a sequence of
 * calls to ...Atom(), ...Range(), and ...BuiltIn(), followed by a call to ...End().
 *
 * Sequences of atoms and assertions are broken into alternatives via calls to
 * disjunction().  Assertions, atoms, and disjunctions emitted between calls to
 * atomParenthesesBegin() and atomParenthesesEnd() form the body of a subpattern.
 * atomParenthesesBegin() is passed a subpatternId.  In the case of a regular
 * capturing subpattern, this will be the subpatternId associated with these
 * parentheses, and will also by definition be the lowest subpatternId of these
 * parentheses and of any nested paretheses.  The atomParenthesesEnd() method
 * is passed the subpatternId of the last capturing subexpression nested within
 * these paretheses.  In the case of a capturing subpattern with no nested
 * capturing subpatterns, the same subpatternId will be passed to the begin and
 * end functions.  In the case of non-capturing subpatterns the subpatternId
 * passed to the begin method is also the first possible subpatternId that might
 * be nested within these paretheses.  If a set of non-capturing parentheses does
 * not contain any capturing subpatterns, then the subpatternId passed to begin
 * will be greater than the subpatternId passed to end.
 */

template<class Delegate>
ErrorCode parse(Delegate& delegate, const StringView pattern, bool isUnicode, unsigned backReferenceLimit = quantifyInfinite, bool isNamedForwardReferenceAllowed = true)
{
    if (pattern.is8Bit())
        return Parser<Delegate, LChar>(delegate, pattern, isUnicode, backReferenceLimit, isNamedForwardReferenceAllowed).parse();
    return Parser<Delegate, UChar>(delegate, pattern, isUnicode, backReferenceLimit, isNamedForwardReferenceAllowed).parse();
}

} } // namespace JSC::Yarr
