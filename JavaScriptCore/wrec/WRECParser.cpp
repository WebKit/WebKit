/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WRECParser.h"

#if ENABLE(WREC)

#include "CharacterClassConstructor.h"
#include "WRECFunctors.h"

using namespace WTF;

namespace JSC { namespace WREC {

ALWAYS_INLINE Quantifier Parser::consumeGreedyQuantifier()
{
    switch (peek()) {
        case '?':
            consume();
            return Quantifier(Quantifier::Greedy, 0, 1);

        case '*':
            consume();
            return Quantifier(Quantifier::Greedy, 0);

        case '+':
            consume();
            return Quantifier(Quantifier::Greedy, 1);

        case '{': {
            consume();
            // a numeric quantifier should always have a lower bound
            if (!peekIsDigit()) {
                m_error = MalformedQuantifier;
                return Quantifier(Quantifier::Error);
            }
            int min = consumeNumber();
            
            // this should either be a , or a }
            switch (peek()) {
            case '}':
                // {n} - exactly n times. (technically I think a '?' is valid in the bnf - bit meaningless).
                consume();
                return Quantifier(Quantifier::Greedy, min, min);

            case ',':
                consume();
                switch (peek()) {
                case '}':
                    // {n,} - n to inf times.
                    consume();
                    return Quantifier(Quantifier::Greedy, min);

                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': {
                    // {n,m} - n to m times.
                    int max = consumeNumber();
                    
                    if (peek() != '}') {
                        m_error = MalformedQuantifier;
                        return Quantifier(Quantifier::Error);
                    }
                    consume();

                    return Quantifier(Quantifier::Greedy, min, max);
                }

                default:
                    m_error = MalformedQuantifier;
                    return Quantifier(Quantifier::Error);
                }

            default:
                m_error = MalformedQuantifier;
                return Quantifier(Quantifier::Error);
            }
        }
        // None of the above - no quantifier
        default:
            return Quantifier();
    }
}

Quantifier Parser::consumeQuantifier()
{
    Quantifier q = consumeGreedyQuantifier();
    
    if ((q.type == Quantifier::Greedy) && (peek() == '?')) {
        consume();
        q.type = Quantifier::NonGreedy;
    }
    
    return q;
}

bool Parser::parsePatternCharacterSequence(JumpList& failures, int ch)
{
    Vector<int, 8> sequence;
    sequence.append(ch);
    Quantifier quantifier;
    Escape escape;

    bool done = false;
    while (!done) {
        switch (peek()) {
            case EndOfPattern:
            case '^':
            case '$':
            case '.':
            case '(':
            case ')':
            case '[':
            case ']':
            case '}':
            case '|': {
                done = true;
                continue;
            }
            case '*': 
            case '+': 
            case '?': 
            case '{': {
                quantifier = consumeQuantifier();
                done = true;
                continue;
            }
            case '\\': {
                consume();
                escape = consumeEscape(false);
                if (escape.type() == Escape::PatternCharacter) {
                    sequence.append(PatternCharacterEscape::cast(escape).character());
                    continue;
                }

                done = true;
                continue;
            }
            default: {
                // Anything else is a PatternCharacter.
                sequence.append(consume());
                continue;
            }
        }
    }

    // Generate the character sequence. If the last character was quantified,
    // it needs to be parsed separately, in the context of its quantifier.
    size_t count = quantifier.type == Quantifier::None ? sequence.size() : sequence.size() - 1;
    m_generator.generatePatternCharacterSequence(failures, sequence.begin(), count);

    switch (quantifier.type) {
        case Quantifier::None: {
            break;
        }
        case Quantifier::Error: {
            return false;
        }
        case Quantifier::Greedy: {
            GeneratePatternCharacterFunctor functor(sequence.last());
            m_generator.generateGreedyQuantifier(failures, functor, quantifier.min, quantifier.max);
            return true;
        }
        case Quantifier::NonGreedy: {
            GeneratePatternCharacterFunctor functor(sequence.last());
            m_generator.generateNonGreedyQuantifier(failures, functor, quantifier.min, quantifier.max);
            return true;
        }
    }

    // If the last token was not a quantifier, it might have been a non-character
    // escape. We need to parse it now, since we've consumed it.
    if (escape.type() != Escape::None && escape.type() != Escape::PatternCharacter)
        return parseEscape(failures, escape);

    return true;
}

bool Parser::parseCharacterClassQuantifier(JumpList& failures, const CharacterClass& charClass, bool invert)
{
    Quantifier q = consumeQuantifier();

    switch (q.type) {
    case Quantifier::None: {
        m_generator.generateCharacterClass(failures, charClass, invert);
        break;
    }

    case Quantifier::Greedy: {
        GenerateCharacterClassFunctor functor(&charClass, invert);
        m_generator.generateGreedyQuantifier(failures, functor, q.min, q.max);
        break;
    }

    case Quantifier::NonGreedy: {
        GenerateCharacterClassFunctor functor(&charClass, invert);
        m_generator.generateNonGreedyQuantifier(failures, functor, q.min, q.max);
        break;
    }

    case Quantifier::Error:
        return false;
    }
    
    return true;
}

bool Parser::parseBackreferenceQuantifier(JumpList& failures, unsigned subpatternId)
{
    Quantifier q = consumeQuantifier();

    switch (q.type) {
    case Quantifier::None: {
        m_generator.generateBackreference(failures, subpatternId);
        break;
    }

    case Quantifier::Greedy:
    case Quantifier::NonGreedy:
        m_generator.generateBackreferenceQuantifier(failures, q.type, subpatternId, q.min, q.max);
        return true;

    case Quantifier::Error:
        return false;
    }
    
    return true;
}

bool Parser::parseParentheses(JumpList& failures)
{
    ParenthesesType type = consumeParenthesesType();

    // FIXME: WREC originally failed to backtrack correctly in cases such as
    // "c".match(/(.*)c/). Now, most parentheses handling is disabled. For
    // unsupported parentheses, we fall back on PCRE.

    switch (type) {
        case Generator::Assertion:
            m_generator.generateParenthesesAssertion(failures);
            break;

        case Generator::InvertedAssertion:
            m_generator.generateParenthesesInvertedAssertion(failures);
            break;

        default:
            m_error = UnsupportedParentheses;
            return false;
    }

    if (consume() != ')') {
        m_error = MalformedParentheses;
        return false;
    }

    Quantifier q = consumeQuantifier();

    switch (q.type) {
        case Quantifier::None:
            return true;

        case Quantifier::Greedy:
            m_error = UnsupportedParentheses;
            return false;

        case Quantifier::NonGreedy:
            m_error = UnsupportedParentheses;
            return false;

        case Quantifier::Error:
            return false;
    }
    
    ASSERT_NOT_REACHED();
    return false;
}

bool Parser::parseCharacterClass(JumpList& failures)
{
    bool invert = false;
    if (peek() == '^') {
        consume();
        invert = true;
    }

    CharacterClassConstructor constructor(m_ignoreCase);

    UChar ch;
    while ((ch = peek()) != ']') {
        switch (ch) {
        case EndOfPattern:
            m_error = MalformedCharacterClass;
            return false;

        case '\\': {
            consume();
            Escape escape = consumeEscape(true);

            switch (escape.type()) {
                case Escape::PatternCharacter: {
                    int character = PatternCharacterEscape::cast(escape).character();
                    if (character == '-')
                        constructor.flushBeforeEscapedHyphen();
                    constructor.put(character);
                    break;
                }
                case Escape::CharacterClass: {
                    const CharacterClassEscape& characterClassEscape = CharacterClassEscape::cast(escape);
                    ASSERT(!characterClassEscape.invert());
                    constructor.append(characterClassEscape.characterClass());
                    break;
                }
                case Escape::Error: {
                    m_error = MalformedEscape;
                    return false;
                }
                case Escape::None:
                case Escape::Backreference:
                case Escape::WordBoundaryAssertion: {
                    ASSERT_NOT_REACHED();
                    break;
                }
            }
            break;
        }

        default:
            consume();
            constructor.put(ch);
        }
    }
    consume();

    // lazily catch reversed ranges ([z-a])in character classes
    if (constructor.isUpsideDown()) {
        m_error = MalformedCharacterClass;
        return false;
    }

    constructor.flush();
    CharacterClass charClass = constructor.charClass();
    return parseCharacterClassQuantifier(failures, charClass, invert);
}

bool Parser::parseEscape(JumpList& failures, const Escape& escape)
{
    switch (escape.type()) {
        case Escape::PatternCharacter: {
            return parsePatternCharacterSequence(failures, PatternCharacterEscape::cast(escape).character());
        }
        case Escape::CharacterClass: {
            return parseCharacterClassQuantifier(failures, CharacterClassEscape::cast(escape).characterClass(), CharacterClassEscape::cast(escape).invert());
        }
        case Escape::Backreference: {
            return parseBackreferenceQuantifier(failures, BackreferenceEscape::cast(escape).subpatternId());
        }
        case Escape::WordBoundaryAssertion: {
            m_generator.generateAssertionWordBoundary(failures, WordBoundaryAssertionEscape::cast(escape).invert());
            return true;
        }
        case Escape::Error: {
            m_error = MalformedEscape;
            return false;
        }
        case Escape::None: {
            return false;
        }
    }
    
    ASSERT_NOT_REACHED();
    return false;
}

Escape Parser::consumeEscape(bool inCharacterClass)
{
    switch (peek()) {
    case EndOfPattern:
        return Escape(Escape::Error);

    // Assertions
    case 'b':
        consume();
        if (inCharacterClass)
            return PatternCharacterEscape('\b');
        return WordBoundaryAssertionEscape(false); // do not invert
    case 'B':
        consume();
        if (inCharacterClass)
            return Escape(Escape::Error);
        return WordBoundaryAssertionEscape(true); // invert

    // CharacterClassEscape
    case 'd':
        consume();
        return CharacterClassEscape(CharacterClass::digits(), false);
    case 's':
        consume();
        return CharacterClassEscape(CharacterClass::spaces(), false);
    case 'w':
        consume();
        return CharacterClassEscape(CharacterClass::wordchar(), false);
    case 'D':
        consume();
        return inCharacterClass
            ? CharacterClassEscape(CharacterClass::nondigits(), false)
            : CharacterClassEscape(CharacterClass::digits(), true);
    case 'S':
        consume();
        return inCharacterClass
            ? CharacterClassEscape(CharacterClass::nonspaces(), false)
            : CharacterClassEscape(CharacterClass::spaces(), true);
    case 'W':
        consume();
        return inCharacterClass
            ? CharacterClassEscape(CharacterClass::nonwordchar(), false)
            : CharacterClassEscape(CharacterClass::wordchar(), true);

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
        if (peekDigit() > m_numSubpatterns || inCharacterClass) {
            // To match Firefox, we parse an invalid backreference in the range [1-7]
            // as an octal escape.
            return peekDigit() > 7 ? Escape(Escape::Error) : PatternCharacterEscape(consumeOctal());
        }

        int value = 0;
        do {
            unsigned newValue = value * 10 + peekDigit();
            if (newValue > m_numSubpatterns)
                break;
            value = newValue;
            consume();
        } while (peekIsDigit());

        return BackreferenceEscape(value);
    }

    // Octal escape
    case '0':
        consume();
        return PatternCharacterEscape(consumeOctal());

    // ControlEscape
    case 'f':
        consume();
        return PatternCharacterEscape('\f');
    case 'n':
        consume();
        return PatternCharacterEscape('\n');
    case 'r':
        consume();
        return PatternCharacterEscape('\r');
    case 't':
        consume();
        return PatternCharacterEscape('\t');
    case 'v':
        consume();
        return PatternCharacterEscape('\v');

    // ControlLetter
    case 'c': {
        consume();
        int control = consume();
        if (!isASCIIAlpha(control))
            return Escape(Escape::Error);
        return PatternCharacterEscape(control & 31);
    }

    // HexEscape
    case 'x': {
        consume();
        int x = consumeHex(2);
        if (x == -1)
            return Escape(Escape::Error);
        return PatternCharacterEscape(x);
    }

    // UnicodeEscape
    case 'u': {
        consume();
        int x = consumeHex(4);
        if (x == -1)
            return Escape(Escape::Error);
        return PatternCharacterEscape(x);
    }

    // IdentityEscape
    default: {
        // TODO: check this test for IdentifierPart.
        int ch = consume();
        if (isASCIIAlphanumeric(ch) || (ch == '_'))
            return Escape(Escape::Error);
        return PatternCharacterEscape(ch);
    }
    }
}

bool Parser::parseTerm(JumpList& failures)
{
    switch (peek()) {
    case EndOfPattern:
    case '*':
    case '+':
    case '?':
    case ')':
    case ']':
    case '{':
    case '}':
    case '|':
        return false;
        
    case '^':
        consume();
        m_generator.generateAssertionBOL(failures);
        return true;
    
    case '$':
        consume();
        m_generator.generateAssertionEOL(failures);
        return true;

    case '\\':
        consume();
        return parseEscape(failures, consumeEscape(false));

    case '.':
        consume();
        return parseCharacterClassQuantifier(failures, CharacterClass::newline(), true);

    case '[':
        consume();
        return parseCharacterClass(failures);

    case '(':
        consume();
        return parseParentheses(failures);

    default:
        // Anything else is a PatternCharacter.
        return parsePatternCharacterSequence(failures, consume());
    }
}

/*
  TOS holds index.
*/
void Parser::parseDisjunction(JumpList& failures)
{
    parseAlternative(failures);
    if (peek() != '|')
        return;

    JumpList successes;
    do {
        consume();
        m_generator.terminateAlternative(successes, failures);
        parseAlternative(failures);
    } while (peek() == '|');

    m_generator.terminateDisjunction(successes);
}

Generator::ParenthesesType Parser::consumeParenthesesType()
{
    if (peek() != '?')
        return Generator::Capturing;
    consume();

    switch (consume()) {
    case ':':
        return Generator::NonCapturing;
    
    case '=':
        return Generator::Assertion;

    case '!':
        return Generator::InvertedAssertion;

    default:
        m_error = MalformedParentheses;
        return Generator::Error;
    }
}

} } // namespace JSC::WREC

#endif // ENABLE(WREC)
