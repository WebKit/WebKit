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

ALWAYS_INLINE Quantifier Parser::parseGreedyQuantifier()
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

Quantifier Parser::parseQuantifier()
{
    Quantifier q = parseGreedyQuantifier();
    
    if ((q.type == Quantifier::Greedy) && (peek() == '?')) {
        consume();
        q.type = Quantifier::NonGreedy;
    }
    
    return q;
}

bool Parser::parsePatternCharacterQualifier(JmpSrcVector& failures, int ch)
{
    Quantifier q = parseQuantifier();

    switch (q.type) {
    case Quantifier::None: {
        m_generator.generatePatternCharacter(failures, ch);
        break;
    }

    case Quantifier::Greedy: {
        GeneratePatternCharacterFunctor functor(ch);
        m_generator.generateGreedyQuantifier(failures, functor, q.min, q.max);
        break;
    }

    case Quantifier::NonGreedy: {
        GeneratePatternCharacterFunctor functor(ch);
        m_generator.generateNonGreedyQuantifier(failures, functor, q.min, q.max);
        break;
    }

    case Quantifier::Error:
        return false;
    }
    
    return true;
}

bool Parser::parseCharacterClassQuantifier(JmpSrcVector& failures, const CharacterClass& charClass, bool invert)
{
    Quantifier q = parseQuantifier();

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

bool Parser::parseBackreferenceQuantifier(JmpSrcVector& failures, unsigned subpatternId)
{
    Quantifier q = parseQuantifier();

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

bool Parser::parseParentheses(JmpSrcVector&)
{
    // FIXME: We don't currently backtrack correctly within parentheses in cases such as
    // "c".match(/(.*)c/) so we fall back to PCRE for any regexp containing parentheses.

    m_error = UnsupportedParentheses;
    return false;
}

bool Parser::parseCharacterClass(JmpSrcVector& failures)
{
    bool invert = false;
    if (peek() == '^') {
        consume();
        invert = true;
    }

    CharacterClassConstructor charClassConstructor(m_ignoreCase);

    UChar ch;
    while ((ch = peek()) != ']') {
        switch (ch) {
        case EndOfPattern:
            m_error = MalformedCharacterClass;
            return false;
            
        case '\\':
            consume();
            switch (ch = peek()) {
            case EndOfPattern:
                m_error = MalformedEscape;
                return false;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                charClassConstructor.put(consumeOctal());
                break;

            // ControlEscape
            case 'b':
                consume();
                charClassConstructor.put('\b');
                break;
            case 'f':
                consume();
                charClassConstructor.put('\f');
                break;
            case 'n':
                consume();
                charClassConstructor.put('\n');
                break;
            case 'r':
                consume();
                charClassConstructor.put('\r');
                break;
            case 't':
                consume();
                charClassConstructor.put('\t');
                break;
            case 'v':
                consume();
                charClassConstructor.put('\v');
                break;

            // ControlLetter
            case 'c': {
                consume();
                int control = consume();
                if (!isASCIIAlpha(control)) {
                    m_error = MalformedEscape;
                    return false;
                }
                charClassConstructor.put(control&31);
                break;
            }

            // HexEscape
            case 'x': {
                consume();
                int x = consumeHex(2);
                if (x == -1) {
                    m_error = MalformedEscape;
                    return false;
                }
                charClassConstructor.put(x);
                break;
            }

            // UnicodeEscape
            case 'u': {
                consume();
                int x = consumeHex(4);
                if (x == -1) {
                    m_error = MalformedEscape;
                    return false;
                }
                charClassConstructor.put(x);
                break;
            }
            
            // CharacterClassEscape
            case 'd':
                consume();
                charClassConstructor.append(CharacterClass::digits());
                break;
            case 's':
                consume();
                charClassConstructor.append(CharacterClass::spaces());
                break;
            case 'w':
                consume();
                charClassConstructor.append(CharacterClass::wordchar());
                break;
            case 'D':
                consume();
                charClassConstructor.append(CharacterClass::nondigits());
                break;
            case 'S':
                consume();
                charClassConstructor.append(CharacterClass::nonspaces());
                break;
            case 'W':
                consume();
                charClassConstructor.append(CharacterClass::nonwordchar());
                break;

            case '-':
                consume();
                charClassConstructor.flushBeforeEscapedHyphen();
                charClassConstructor.put(ch);
                break;

            // IdentityEscape
            default: {
                // TODO: check this test for IdentifierPart.
                int ch = consume();
                if (isASCIIAlphanumeric(ch) || (ch == '_')) {
                    m_error = MalformedEscape;
                    return false;
                }
                charClassConstructor.put(ch);
                break;
            }
            }
            break;
            
        default:
            consume();
            charClassConstructor.put(ch);
        }
    }
    consume();

    // lazily catch reversed ranges ([z-a])in character classes
    if (charClassConstructor.isUpsideDown()) {
        m_error = MalformedCharacterClass;
        return false;
    }

    charClassConstructor.flush();
    CharacterClass charClass = charClassConstructor.charClass();
    return parseCharacterClassQuantifier(failures, charClass, invert);
}

bool Parser::parseOctalEscape(JmpSrcVector& failures)
{
    return parsePatternCharacterQualifier(failures, consumeOctal());
}

bool Parser::parseEscape(JmpSrcVector& failures)
{
    switch (peek()) {
    case EndOfPattern:
        m_error = MalformedEscape;
        return false;

    // Assertions
    case 'b':
        consume();
        m_generator.generateAssertionWordBoundary(failures, false);
        return true;

    case 'B':
        consume();
        m_generator.generateAssertionWordBoundary(failures, true);
        return true;

    // Octal escape
    case '0':
        consume();
        return parseOctalEscape(failures);

    // DecimalEscape
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7': {
        // FIXME: This does not support forward references. It's not clear whether they
        // should be valid.
        unsigned value = peekDigit();
        if (value > m_numSubpatterns)
            return parseOctalEscape(failures);
    }
    case '8':
    case '9': {
        unsigned value = peekDigit();
        if (value > m_numSubpatterns) {
            consume();
            m_error = MalformedEscape;
            return false;
        }
        consume();

        while (peekIsDigit()) {
            unsigned newValue = value * 10 + peekDigit();
            if (newValue > m_numSubpatterns)
                break;
            value = newValue;
            consume();
        }

        parseBackreferenceQuantifier(failures, value);
        return true;
    }
    
    // ControlEscape
    case 'f':
        consume();
        return parsePatternCharacterQualifier(failures, '\f');
    case 'n':
        consume();
        return parsePatternCharacterQualifier(failures, '\n');
    case 'r':
        consume();
        return parsePatternCharacterQualifier(failures, '\r');
    case 't':
        consume();
        return parsePatternCharacterQualifier(failures, '\t');
    case 'v':
        consume();
        return parsePatternCharacterQualifier(failures, '\v');

    // ControlLetter
    case 'c': {
        consume();
        int control = consume();
        if (!isASCIIAlpha(control)) {
            m_error = MalformedEscape;
            return false;
        }
        return parsePatternCharacterQualifier(failures, control&31);
    }

    // HexEscape
    case 'x': {
        consume();
        int x = consumeHex(2);
        if (x == -1) {
            m_error = MalformedEscape;
            return false;
        }
        return parsePatternCharacterQualifier(failures, x);
    }

    // UnicodeEscape
    case 'u': {
        consume();
        int x = consumeHex(4);
        if (x == -1) {
            m_error = MalformedEscape;
            return false;
        }
        return parsePatternCharacterQualifier(failures, x);
    }

    // CharacterClassEscape
    case 'd':
        consume();
        return parseCharacterClassQuantifier(failures, CharacterClass::digits(), false);
    case 's':
        consume();
        return parseCharacterClassQuantifier(failures, CharacterClass::spaces(), false);
    case 'w':
        consume();
        return parseCharacterClassQuantifier(failures, CharacterClass::wordchar(), false);
    case 'D':
        consume();
        return parseCharacterClassQuantifier(failures, CharacterClass::digits(), true);
    case 'S':
        consume();
        return parseCharacterClassQuantifier(failures, CharacterClass::spaces(), true);
    case 'W':
        consume();
        return parseCharacterClassQuantifier(failures, CharacterClass::wordchar(), true);

    // IdentityEscape
    default: {
        // TODO: check this test for IdentifierPart.
        int ch = consume();
        if (isASCIIAlphanumeric(ch) || (ch == '_')) {
            m_error = MalformedEscape;
            return false;
        }
        return parsePatternCharacterQualifier(failures, ch);
    }
    }
}

bool Parser::parseTerm(JmpSrcVector& failures)
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
        // Not allowed in a Term!
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
        // b & B are also assertions...
        consume();
        return parseEscape(failures);

    case '.':
        consume();
        return parseCharacterClassQuantifier(failures, CharacterClass::newline(), true);

    case '[':
        // CharacterClass
        consume();
        return parseCharacterClass(failures);

    case '(':
        consume();
        return parseParentheses(failures);

    default:
        // Anything else is a PatternCharacter
        return parsePatternCharacterQualifier(failures, consume());
    }
}

/*
  TOS holds index at the start of a disjunction.
*/
void Parser::parseDisjunction(JmpSrcVector& failures)
{
    parseAlternative(failures);
    if (peek() != '|')
        return;

    JmpSrcVector successes;
    do {
        consume();
        m_generator.terminateAlternative(successes, failures);
        parseAlternative(failures);
    } while (peek() == '|');

    m_generator.terminateDisjunction(successes);
}

} } // namespace JSC::WREC

#endif // ENABLE(WREC)
