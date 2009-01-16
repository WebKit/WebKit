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

// These error messages match the error messages used by PCRE.
const char* Parser::QuantifierOutOfOrder = "numbers out of order in {} quantifier";
const char* Parser::QuantifierWithoutAtom = "nothing to repeat";
const char* Parser::ParenthesesUnmatched = "unmatched parentheses";
const char* Parser::ParenthesesTypeInvalid = "unrecognized character after (?";
const char* Parser::ParenthesesNotSupported = ""; // Not a user-visible syntax error -- just signals a syntax that WREC doesn't support yet.
const char* Parser::CharacterClassUnmatched = "missing terminating ] for character class";
const char* Parser::CharacterClassOutOfOrder = "range out of order in character class";
const char* Parser::EscapeUnterminated = "\\ at end of pattern";

class PatternCharacterSequence {
typedef Generator::JumpList JumpList;

public:
    PatternCharacterSequence(Generator& generator, JumpList& failures)
        : m_generator(generator)
        , m_failures(failures)
    {
    }
    
    size_t size() { return m_sequence.size(); }
    
    void append(int ch)
    {
        m_sequence.append(ch);
    }
    
    void flush()
    {
        if (!m_sequence.size())
            return;

        m_generator.generatePatternCharacterSequence(m_failures, m_sequence.begin(), m_sequence.size());
        m_sequence.clear();
    }

    void flush(const Quantifier& quantifier)
    {
        if (!m_sequence.size())
            return;

        m_generator.generatePatternCharacterSequence(m_failures, m_sequence.begin(), m_sequence.size() - 1);

        switch (quantifier.type) {
        case Quantifier::None:
        case Quantifier::Error:
            ASSERT_NOT_REACHED();
            break;

        case Quantifier::Greedy: {
            GeneratePatternCharacterFunctor functor(m_sequence.last());
            m_generator.generateGreedyQuantifier(m_failures, functor, quantifier.min, quantifier.max);
            break;
        }
        
        case Quantifier::NonGreedy: {
            GeneratePatternCharacterFunctor functor(m_sequence.last());
            m_generator.generateNonGreedyQuantifier(m_failures, functor, quantifier.min, quantifier.max);
            break;
        }
        }
        
        m_sequence.clear();
    }

private:
    Generator& m_generator;
    JumpList& m_failures;
    Vector<int, 8> m_sequence;
};

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
            SavedState state(*this);
            consume();

            // Accept: {n}, {n,}, {n,m}.
            // Reject: {n,m} where n > m.
            // Ignore: Anything else, such as {n, m}.

            if (!peekIsDigit()) {
                state.restore();
                return Quantifier();
            }

            unsigned min = consumeNumber();
            unsigned max = min;

            if (peek() == ',') {
                consume();
                max = peekIsDigit() ? consumeNumber() : Quantifier::Infinity;
            }

            if (peek() != '}') {
                state.restore();
                return Quantifier();
            }
            consume();
 
            if (min > max) {
                setError(QuantifierOutOfOrder);
                return Quantifier(Quantifier::Error);
            }

            return Quantifier(Quantifier::Greedy, min, max);
         }

         default:
            return Quantifier(); // No quantifier.
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
        case Generator::Assertion: {
            m_generator.generateParenthesesAssertion(failures);

            if (consume() != ')') {
                setError(ParenthesesUnmatched);
                return false;
            }

            Quantifier quantifier = consumeQuantifier();
            if (quantifier.type != Quantifier::None && quantifier.min == 0) {
                setError(ParenthesesNotSupported);
                return false;
            }

            return true;
        }
        case Generator::InvertedAssertion: {
            m_generator.generateParenthesesInvertedAssertion(failures);

            if (consume() != ')') {
                setError(ParenthesesUnmatched);
                return false;
            }

            Quantifier quantifier = consumeQuantifier();
            if (quantifier.type != Quantifier::None && quantifier.min == 0) {
                setError(ParenthesesNotSupported);
                return false;
            }

            return true;
        }
        default:
            setError(ParenthesesNotSupported);
            return false;
    }
}

bool Parser::parseCharacterClass(JumpList& failures)
{
    bool invert = false;
    if (peek() == '^') {
        consume();
        invert = true;
    }

    CharacterClassConstructor constructor(m_ignoreCase);

    int ch;
    while ((ch = peek()) != ']') {
        switch (ch) {
        case EndOfPattern:
            setError(CharacterClassUnmatched);
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
                case Escape::Error:
                    return false;
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
        setError(CharacterClassOutOfOrder);
        return false;
    }

    constructor.flush();
    CharacterClass charClass = constructor.charClass();
    return parseCharacterClassQuantifier(failures, charClass, invert);
}

bool Parser::parseNonCharacterEscape(JumpList& failures, const Escape& escape)
{
    switch (escape.type()) {
        case Escape::PatternCharacter:
            ASSERT_NOT_REACHED();
            return false;

        case Escape::CharacterClass:
            return parseCharacterClassQuantifier(failures, CharacterClassEscape::cast(escape).characterClass(), CharacterClassEscape::cast(escape).invert());

        case Escape::Backreference:
            return parseBackreferenceQuantifier(failures, BackreferenceEscape::cast(escape).subpatternId());

        case Escape::WordBoundaryAssertion:
            m_generator.generateAssertionWordBoundary(failures, WordBoundaryAssertionEscape::cast(escape).invert());
            return true;

        case Escape::Error:
            return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

Escape Parser::consumeEscape(bool inCharacterClass)
{
    switch (peek()) {
    case EndOfPattern:
        setError(EscapeUnterminated);
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
            return PatternCharacterEscape('B');
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
            return peekDigit() > 7 ? PatternCharacterEscape('\\') : PatternCharacterEscape(consumeOctal());
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
        SavedState state(*this);
        consume();
        
        int control = consume();
        // To match Firefox, inside a character class, we also accept numbers
        // and '_' as control characters.
        if ((!inCharacterClass && !isASCIIAlpha(control)) || (!isASCIIAlphanumeric(control) && control != '_')) {
            state.restore();
            return PatternCharacterEscape('\\');
        }
        return PatternCharacterEscape(control & 31);
    }

    // HexEscape
    case 'x': {
        consume();

        SavedState state(*this);
        int x = consumeHex(2);
        if (x == -1) {
            state.restore();
            return PatternCharacterEscape('x');
        }
        return PatternCharacterEscape(x);
    }

    // UnicodeEscape
    case 'u': {
        consume();

        SavedState state(*this);
        int x = consumeHex(4);
        if (x == -1) {
            state.restore();
            return PatternCharacterEscape('u');
        }
        return PatternCharacterEscape(x);
    }

    // IdentityEscape
    default:
        return PatternCharacterEscape(consume());
    }
}

void Parser::parseAlternative(JumpList& failures)
{
    PatternCharacterSequence sequence(m_generator, failures);

    while (1) {
        switch (peek()) {
        case EndOfPattern:
        case '|':
        case ')':
            sequence.flush();
            return;

        case '*':
        case '+':
        case '?':
        case '{': {
            Quantifier q = consumeQuantifier();

            if (q.type == Quantifier::None) {
                sequence.append(consume());
                continue;
            }

            if (q.type == Quantifier::Error)
                return;

            if (!sequence.size()) {
                setError(QuantifierWithoutAtom);
                return;
            }

            sequence.flush(q);
            continue;
        }

        case '^':
            consume();

            sequence.flush();
            m_generator.generateAssertionBOL(failures);
            continue;

        case '$':
            consume();

            sequence.flush();
            m_generator.generateAssertionEOL(failures);
            continue;

        case '.':
            consume();

            sequence.flush();
            if (!parseCharacterClassQuantifier(failures, CharacterClass::newline(), true))
                return;
            continue;

        case '[':
            consume();

            sequence.flush();
            if (!parseCharacterClass(failures))
                return;
            continue;

        case '(':
            consume();

            sequence.flush();
            if (!parseParentheses(failures))
                return;
            continue;

        case '\\': {
            consume();

            Escape escape = consumeEscape(false);
            if (escape.type() == Escape::PatternCharacter) {
                sequence.append(PatternCharacterEscape::cast(escape).character());
                continue;
            }

            sequence.flush();
            if (!parseNonCharacterEscape(failures, escape))
                return;
            continue;
        }

        default:
            sequence.append(consume());
            continue;
        }
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
        setError(ParenthesesTypeInvalid);
        return Generator::Error;
    }
}

} } // namespace JSC::WREC

#endif // ENABLE(WREC)
