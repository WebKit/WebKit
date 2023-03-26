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

enum class CreateDisjunctionPurpose : uint8_t { NotForNextAlternative, ForNextAlternative };

enum class CharacterClassSetOp : uint8_t {
    Default,
    Union,
    Intersection,
    Subtraction
};

// The Parser class should not be used directly - only via the Yarr::parse() method.
template<class Delegate, typename CharType>
class Parser {
private:
    template<class FriendDelegate>
    friend ErrorCode parse(FriendDelegate&, StringView pattern, CompileMode, unsigned backReferenceLimit, bool isNamedForwardReferenceAllowed);

    enum class UnicodeParseContext : uint8_t { PatternCodePoint, GroupName };

    enum class ParseEscapeMode : uint8_t { Normal, CharacterClass, ClassStringDisjunction };

    enum class TokenType : uint8_t {
        NotAtom = 0,
        Atom = 1,
        Lookbehind = 2
    };

    class NamedCaptureGroups {
        typedef HashSet<String> GroupNameHashSet;

    public:
        NamedCaptureGroups()
        {
            m_activeCaptureGroupNames.grow(1);
        }

        bool contains(String name)
        {
            return m_captureGroupNames.contains(name);
        }

        bool isEmpty()
        {
            return m_captureGroupNames.isEmpty();
        }

        void reset()
        {
            m_captureGroupNames.clear();
            m_activeCaptureGroupNames.clear();
            m_activeCaptureGroupNames.grow(1);
        }

        void nextAlternative()
        {
            m_activeCaptureGroupNames.last().clear();

            // For nested parenthesis, we need to seed the new alternative with the already seen
            // named captures for the encompassing alternative.
            if (m_activeCaptureGroupNames.size() > 1)
                m_activeCaptureGroupNames.last().formUnion(m_activeCaptureGroupNames[m_activeCaptureGroupNames.size() - 2]);
        }

        void pushParenthesis()
        {
            auto currentTop = m_activeCaptureGroupNames.last();
            m_activeCaptureGroupNames.append(currentTop);
        }

        void popParenthesis()
        {
            ASSERT(m_activeCaptureGroupNames.size() > 1);
            m_activeCaptureGroupNames.removeLast();
        }

        GroupNameHashSet::AddResult add(String name)
        {
            m_captureGroupNames.add(name);
            return m_activeCaptureGroupNames.last().add(name);
        }

    private:
        GroupNameHashSet m_captureGroupNames;
        Vector<GroupNameHashSet, 1> m_activeCaptureGroupNames;
    };

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
        CharacterClassParserDelegate(Delegate& delegate, ErrorCode& err, CompileMode compileMode)
            : m_delegate(delegate)
            , m_errorCode(err)
            , m_isUnicode(compileMode == CompileMode::Unicode)
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

    /*
     * ClassSetParserDelegate:
     *
     * The class ClassSetParserDelegate is used in the parsing of character
     * classes.  This class handles detection of class set ops and character ranges.
     * This class implements enough of the delegate interface such that it can be passed to
     * parseEscape() as an EscapeDelegate.  This allows parseEscape() to be reused
     * to perform the parsing of escape characters in character sets.
     */
    class ClassSetParserDelegate {
    private:
        struct NestingState {
        public:
            NestingState(CharacterClassSetOp setOp, bool inverted)
                : m_setOp(setOp)
                , m_inverted(inverted)
            { }

            CharacterClassSetOp m_setOp;
            bool m_inverted;
        };

    public:
        ClassSetParserDelegate(Delegate& delegate, ErrorCode& err)
            : m_delegate(delegate)
            , m_errorCode(err)
            , m_state(ClassSetConstructionState::Empty)
            , m_setOp(CharacterClassSetOp::Default)
            , m_inverted(false)
            , m_processingEscape(false)
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
            m_inverted = invert;
            m_delegate.atomCharacterClassBegin(invert);
        }

        void nestedClassBegin()
        {
            m_delegate.atomCharacterClassPushNested();
            nestedParseState.append(NestingState(m_setOp, m_inverted));
            m_setOp = CharacterClassSetOp::Default;
            m_inverted = false;
        }

        bool nestedClassEnd()
        {
            flushCachedCharacterIfNeeded();

            if (nestedParseState.isEmpty()) {
                end();
                return true;
            }

            NestingState lastState = nestedParseState.takeLast();
            m_setOp = lastState.m_setOp;
            m_inverted = lastState.m_inverted;

            m_delegate.atomCharacterClassPopNested();
            return false;
        }

        void setUnionOp()
        {
            if (m_setOp != CharacterClassSetOp::Default && m_setOp != CharacterClassSetOp::Union) {
                m_errorCode = ErrorCode::InvalidClassSetOperation;
                return;
            }

            flushCachedCharacterIfNeeded();
            m_setOp = CharacterClassSetOp::Union;
            m_delegate.atomCharacterClassSetOp(m_setOp);
        }

        void switchFromDefaultOpToUnionOpIfNeeded()
        {
            if (m_setOp == CharacterClassSetOp::Default) {
                m_setOp = CharacterClassSetOp::Union;
                m_delegate.atomCharacterClassSetOp(m_setOp);
            }
        }

        void setSubtractOp()
        {
            if (m_setOp != CharacterClassSetOp::Default && m_setOp != CharacterClassSetOp::Subtraction) {
                m_errorCode = ErrorCode::InvalidClassSetOperation;
                return;
            }

            flushCachedCharacterIfNeeded();
            m_setOp = CharacterClassSetOp::Subtraction;
            m_delegate.atomCharacterClassSetOp(m_setOp);
            m_state = ClassSetConstructionState::Empty;
        }

        void setIntersectionOp()
        {
            if (m_setOp != CharacterClassSetOp::Default && m_setOp != CharacterClassSetOp::Intersection) {
                m_errorCode = ErrorCode::InvalidClassSetOperation;
                return;
            }

            flushCachedCharacterIfNeeded();
            m_setOp = CharacterClassSetOp::Intersection;
            m_delegate.atomCharacterClassSetOp(m_setOp);
            m_state = ClassSetConstructionState::Empty;
        }

        void flushCachedCharacterIfNeeded()
        {
            if (m_state == ClassSetConstructionState::CachedCharacter) {
                m_delegate.atomCharacterClassAtom(m_character);
                m_state = ClassSetConstructionState::Empty;
            }
        }

        void afterOperand()
        {
            if (m_setOp == CharacterClassSetOp::Default || m_setOp == CharacterClassSetOp::Union)
                return;

            flushCachedCharacterIfNeeded();

            if (m_state != ClassSetConstructionState::Empty && m_state != ClassSetConstructionState::AfterCharacterClass) {
                m_errorCode = ErrorCode::InvalidClassSetOperation;
                return;
            }

            m_state = ClassSetConstructionState::AfterSetOperand;
        }

        bool canTakeSetOperand()
        {
            bool unionOpActive = m_setOp == CharacterClassSetOp::Default || m_setOp == CharacterClassSetOp::Union;

            switch (m_state) {
            case ClassSetConstructionState::Empty:
                return true;

            case ClassSetConstructionState::CachedCharacter:
                if (!unionOpActive)
                    return false;

                flushCachedCharacterIfNeeded();
                return true;

            case ClassSetConstructionState::CachedCharacterHyphen:
            case ClassSetConstructionState::AfterCharacterClassHyphen:
            case ClassSetConstructionState::AfterCharacterClass:
            case ClassSetConstructionState::AfterSetRange:
            case ClassSetConstructionState::AfterSetOperand:
                return unionOpActive;
            }

            return false;
        }

        void setProcessingEscape()
        {
            m_processingEscape = true;
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
        void atomPatternCharacter(UChar32 ch)
        {
            bool unionOpActive = m_setOp == CharacterClassSetOp::Default || m_setOp == CharacterClassSetOp::Union;
            bool processingEscape = m_processingEscape;
            m_processingEscape = false;

            switch (m_state) {
            case ClassSetConstructionState::AfterCharacterClass:
                // Following a built-in character class we need look out for a hyphen.
                // We're looking for invalid ranges, such as /[\d-x]/ or /[\d-\d]/.
                // If we see a hyphen following a character class then unlike usual
                // we'll report it to the delegate immediately, and put ourself into
                // a poisoned state. In a unicode pattern, any following calls to add
                // another character or character class will result in syntax error.
                // A hypen following a character class is itself valid, but only at
                // the end of a regex.
                if (unionOpActive && ch == '-') {
                    m_delegate.atomCharacterClassAtom('-');
                    m_state = ClassSetConstructionState::AfterCharacterClassHyphen;
                    return;
                }
                // Otherwise just fall through - cached character so treat this as ClassSetConstructionState::Empty.
                FALLTHROUGH;

            case ClassSetConstructionState::AfterSetRange:
                switchFromDefaultOpToUnionOpIfNeeded();

                // Continue processing the current character.
                FALLTHROUGH;

            case ClassSetConstructionState::Empty:
                if (!processingEscape && ch == '-') {
                    m_errorCode = ErrorCode::InvalidClassSetCharacter;
                    return;
                }

                m_character = ch;
                m_state = ClassSetConstructionState::CachedCharacter;
                return;

            case ClassSetConstructionState::CachedCharacter:
                if (!unionOpActive) {
                    m_errorCode = ErrorCode::InvalidClassSetOperation;
                    return;
                }

                if (ch == '-')
                    m_state = ClassSetConstructionState::CachedCharacterHyphen;
                else {
                    m_delegate.atomCharacterClassAtom(m_character);
                    switchFromDefaultOpToUnionOpIfNeeded();
                    m_character = ch;
                }
                return;

            case ClassSetConstructionState::CachedCharacterHyphen:
                if (ch < m_character) {
                    m_errorCode = ErrorCode::CharacterClassRangeOutOfOrder;
                    return;
                }

                m_delegate.atomCharacterClassRange(m_character, ch);
                switchFromDefaultOpToUnionOpIfNeeded();
                m_state = ClassSetConstructionState::AfterSetRange;
                return;

                // If we hit this case, we have an invalid range like /[\d-a]/.
                // See coment in atomBuiltInCharacterClass() below.
            case ClassSetConstructionState::AfterCharacterClassHyphen:
                m_errorCode = ErrorCode::CharacterClassRangeInvalid;
                return;

                // The only thing valid at this point is s repeat of the current operator.
            case ClassSetConstructionState::AfterSetOperand:
                m_errorCode = ErrorCode::InvalidClassSetOperation;
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
            bool unionOpActive = m_setOp == CharacterClassSetOp::Default || m_setOp == CharacterClassSetOp::Union;

            switch (m_state) {
            case ClassSetConstructionState::CachedCharacter:
                if (!unionOpActive) {
                    m_errorCode = ErrorCode::InvalidClassSetOperation;
                    return;
                }

                // Flush the currently cached character, then fall through.
                m_delegate.atomCharacterClassAtom(m_character);

                // Yes, we really want to fall through to the AfterSetRange case to switch from Default to Union op
                // and then handle the built in class by falling through again.
                FALLTHROUGH;

            case ClassSetConstructionState::AfterSetRange:
                switchFromDefaultOpToUnionOpIfNeeded();

                // Continue processing the current character.
                FALLTHROUGH;

            case ClassSetConstructionState::Empty:
            case ClassSetConstructionState::AfterCharacterClass:
                if ((invert || m_inverted) && characterClassMayContainStrings(classID)) {
                    m_errorCode = ErrorCode::NegatedClassSetMayContainStrings;
                    return;
                }
                m_delegate.atomCharacterClassBuiltIn(classID, invert);
                m_state = ClassSetConstructionState::AfterCharacterClass;
                return;

                // If we hit either of these cases, we have an invalid range that
                // looks something like /[a-\d]/ or /[\d-\d]/.
                // Since ES2015, this should be syntax error in a unicode pattern,
                // yet gracefully handled in a regular regex to avoid breaking the web.
                // Effectively we handle the hyphen as if it was (implicitly) escaped,
                // e.g. /[\d-a-z]/ is treated as /[\d\-a\-z]/.
                // See usages of CharacterRangeOrUnion abstract op in
                // https://tc39.es/ecma262/#sec-regular-expression-patterns-semantics
            case ClassSetConstructionState::CachedCharacterHyphen:
                m_delegate.atomCharacterClassAtom(m_character);
                m_delegate.atomCharacterClassAtom('-');
                FALLTHROUGH;
            case ClassSetConstructionState::AfterCharacterClassHyphen:
                m_errorCode = ErrorCode::CharacterClassRangeInvalid;
                return;

                // The only thing valid at this point is s repeat of the current operator.
            case ClassSetConstructionState::AfterSetOperand:
                m_errorCode = ErrorCode::InvalidClassSetOperation;
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
            if (m_state == ClassSetConstructionState::CachedCharacter)
                m_delegate.atomCharacterClassAtom(m_character);
            else if (m_state == ClassSetConstructionState::CachedCharacterHyphen) {
                m_delegate.atomCharacterClassAtom(m_character);
                m_delegate.atomCharacterClassAtom('-');
            }
            m_delegate.atomCharacterClassEnd();
        }

        ErrorCode error() { return m_errorCode; }

        // parseEscape() should never call these delegate methods when
        // invoked with inCharacterClass set.
        NO_RETURN_DUE_TO_ASSERT void assertionWordBoundary(bool) { RELEASE_ASSERT_NOT_REACHED(); }
        NO_RETURN_DUE_TO_ASSERT void atomBackReference(unsigned) { RELEASE_ASSERT_NOT_REACHED(); }
        NO_RETURN_DUE_TO_ASSERT void atomNamedBackReference(const String&) { RELEASE_ASSERT_NOT_REACHED(); }
        NO_RETURN_DUE_TO_ASSERT void atomNamedForwardReference(const String&) { RELEASE_ASSERT_NOT_REACHED(); }

    private:
        Delegate& m_delegate;
        ErrorCode& m_errorCode;
        enum class ClassSetConstructionState {
            Empty,
            CachedCharacter,
            CachedCharacterHyphen,
            AfterCharacterClass,
            AfterCharacterClassHyphen,
            AfterSetRange,
            AfterSetOperand,
        };
        ClassSetConstructionState m_state;
        CharacterClassSetOp m_setOp;
        bool m_inverted;
        bool m_processingEscape;
        UChar32 m_character;
        Vector<NestingState> nestedParseState;
    };

    /*
     * ClassStringDisjunctionParserDelegate:
     *
     * The class ClassStringDisjunctionParserDelegate is used in the parsing of class string disjunctions,
     * e.g \q{...}.  This class builds strings from the alternatives and passes them on to
     * character class delegate.
     */
    class ClassStringDisjunctionParserDelegate {
    public:
        ClassStringDisjunctionParserDelegate(Delegate& delegate, ErrorCode& err)
            : m_delegate(delegate)
            , m_errorCode(err)
        {
        }

        void atomPatternCharacter(UChar32 ch, bool = false)
        {
            m_stringInProgress.append(ch);
        }

        void newAlternative()
        {
            m_strings.append(m_stringInProgress);
            m_stringInProgress.clear();
        }

        /*
         * end():
         *
         * Called at end of construction.
         */
        void end()
        {
            newAlternative();
            m_delegate.atomClassStringDisjunction(m_strings);
        }

        // parseEscape() should never call these delegate methods when parsing a class string disjunction.
        NO_RETURN_DUE_TO_ASSERT void assertionWordBoundary(bool) { RELEASE_ASSERT_NOT_REACHED(); }
        NO_RETURN_DUE_TO_ASSERT void atomBackReference(unsigned) { RELEASE_ASSERT_NOT_REACHED(); }
        NO_RETURN_DUE_TO_ASSERT void atomNamedBackReference(const String&) { RELEASE_ASSERT_NOT_REACHED(); }
        NO_RETURN_DUE_TO_ASSERT void atomNamedForwardReference(const String&) { RELEASE_ASSERT_NOT_REACHED(); }
        NO_RETURN_DUE_TO_ASSERT void atomBuiltInCharacterClass(BuiltInCharacterClassID, bool) { RELEASE_ASSERT_NOT_REACHED(); }

    private:
        Delegate& m_delegate;
        ErrorCode& m_errorCode;
        Vector<UChar32> m_stringInProgress;
        Vector<Vector<UChar32>> m_strings;
    };

    Parser(Delegate& delegate, StringView pattern, CompileMode compileMode, unsigned backReferenceLimit, bool isNamedForwardReferenceAllowed)
        : m_delegate(delegate)
        , m_data(pattern.characters<CharType>())
        , m_size(pattern.length())
        , m_compileMode(compileMode)
        , m_backReferenceLimit(backReferenceLimit)
        , m_isNamedForwardReferenceAllowed(isNamedForwardReferenceAllowed)
    {
    }

    // The handling of IdentityEscapes is different depending on which unicode flag if any is active.
    // For Unicode patterns, IdentityEscapes only include SyntaxCharacters or '/'.
    // For UnicodeSet patterns, adds to the UnicodePatterns IdentityEscapes
    // ClassSetReservedPunctionation which includes &-!#%,:;<=>@`~
    // For non-unicode patterns, most any character can be escaped.
    bool isIdentityEscapeAnError(int ch)
    {
        if (isEitherUnicodeCompilation()
            && ((isASCII(ch) && !strchr(isUnicodeCompilation() ? "^$\\.*+?()[]{}|/" : "^$\\.*+?()[]{}|/&-!#%,:;<=>@`~" , ch)) || !ch)) {
            m_errorCode = ErrorCode::InvalidIdentityEscape;
            return true;
        }

        return false;
    }

    /*
     * parseEscape():
     *
     * Helper for parseTokens(), parseAtomEscape(), parseCharacterClassEscape(),
     * parseClassSetEscape() and parseClassStringDisjunctionEscape().
     *
     * Unlike the other parser methods, this function does not report tokens
     * directly to the member delegate (m_delegate), instead tokens are
     * emitted to the delegate provided as an argument.  In the case of atom
     * escapes, parseTokens() will call parseEscape() passing m_delegate as
     * an argument, and as such the escape will be reported to the delegate.
     *
     * However this method may also be used by parseCharacterClass(), parseClassSet(), or
     * parseClassStringDisjunctionEscape() in which case a CharacterClassParserDelegate,
     * ClassSetParserDelegate or ClassStringDisjunctionParserDelegate respectively will be
     * passed as the delegate that tokens should be added to.  Delegate should have the
     * following methods:
     *
     *   Required methods:
     *    void atomPatternCharacter(UChar32 ch);
     *
     *   Optional methods based on parseEscapeMode:
     *    void assertionWordBoundary(bool invert);
     *    void atomBuiltInCharacterClass(BuiltInCharacterClassID classID, bool invert);
     *    void atomBackReference(unsigned subpatternId);
     *    void atomNamedBackReference(const String& subpatternName);
     *    void atomNamedForwardReference(const String& subpatternName);
     */
    template<ParseEscapeMode parseEscapeMode, class EscapeDelegate>
    TokenType parseEscape(EscapeDelegate& delegate)
    {
        ASSERT(!hasError(m_errorCode));
        ASSERT(peek() == '\\');
        consume();

        if (atEndOfPattern()) {
            m_errorCode = ErrorCode::EscapeUnterminated;
            return TokenType::NotAtom;
        }

        switch (peek()) {
        // Assertions
        case 'b':
            consume();
            if (parseEscapeMode != ParseEscapeMode::Normal)
                delegate.atomPatternCharacter('\b');
            else {
                delegate.assertionWordBoundary(false);
                return TokenType::NotAtom;
            }
            break;
        case 'B':
            consume();
            if (parseEscapeMode != ParseEscapeMode::Normal) {
                if (isIdentityEscapeAnError('B'))
                    break;

                delegate.atomPatternCharacter('B');
            } else {
                delegate.assertionWordBoundary(true);
                return TokenType::NotAtom;
            }
            break;

        // CharacterClassEscape
        case 'd':
            consume();
            if (parseEscapeMode == ParseEscapeMode::ClassStringDisjunction) {
                delegate.atomPatternCharacter('d');
                break;
            }
            delegate.atomBuiltInCharacterClass(BuiltInCharacterClassID::DigitClassID, false);
            break;
        case 's':
            consume();
            if (parseEscapeMode == ParseEscapeMode::ClassStringDisjunction) {
                delegate.atomPatternCharacter('s');
                break;
            }
            delegate.atomBuiltInCharacterClass(BuiltInCharacterClassID::SpaceClassID, false);
            break;
        case 'w':
            consume();
            if (parseEscapeMode == ParseEscapeMode::ClassStringDisjunction) {
                delegate.atomPatternCharacter('w');
                break;
            }
            delegate.atomBuiltInCharacterClass(BuiltInCharacterClassID::WordClassID, false);
            break;
        case 'D':
            consume();
            if (parseEscapeMode == ParseEscapeMode::ClassStringDisjunction) {
                delegate.atomPatternCharacter('D');
                break;
            }
            delegate.atomBuiltInCharacterClass(BuiltInCharacterClassID::DigitClassID, true);
            break;
        case 'S':
            consume();
            if (parseEscapeMode == ParseEscapeMode::ClassStringDisjunction) {
                delegate.atomPatternCharacter('S');
                break;
            }
            delegate.atomBuiltInCharacterClass(BuiltInCharacterClassID::SpaceClassID, true);
            break;
        case 'W':
            consume();
            if (parseEscapeMode == ParseEscapeMode::ClassStringDisjunction) {
                delegate.atomPatternCharacter('W');
                break;
            }
            delegate.atomBuiltInCharacterClass(BuiltInCharacterClassID::WordClassID, true);
            break;

        case '0': {
            consume();

            if (!peekIsDigit()) {
                delegate.atomPatternCharacter(0);
                break;
            }

            if (isEitherUnicodeCompilation()) {
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
            if (parseEscapeMode == ParseEscapeMode::Normal) {
                ParseState state = saveState();

                unsigned backReference = consumeNumber();
                if (backReference <= m_backReferenceLimit) {
                    m_maxSeenBackReference = std::max(m_maxSeenBackReference, backReference);
                    delegate.atomBackReference(backReference);
                    break;
                }

                restoreState(state);
                if (isEitherUnicodeCompilation()) {
                    m_errorCode = ErrorCode::InvalidBackreference;
                    break;
                }
            }

            if (isEitherUnicodeCompilation()) {
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

                if (isEitherUnicodeCompilation()) {
                    m_errorCode = ErrorCode::InvalidControlLetterEscape;
                    break;
                }

                // https://tc39.es/ecma262/#prod-annexB-ClassControlLetter
                if (parseEscapeMode != ParseEscapeMode::Normal && (WTF::isASCIIDigit(control) || control == '_')) {
                    delegate.atomPatternCharacter(control & 0x1f);
                    break;
                }
            }

            if (isEitherUnicodeCompilation()) {
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
            if (parseEscapeMode == ParseEscapeMode::Normal && tryConsume('<')) {
                auto groupName = tryConsumeGroupName();
                if (hasError(m_errorCode))
                    break;

                if (groupName) {
                    if (m_namedCaptureGroups.contains(groupName.value())) {
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

            if (isLegacyCompilation() || parseEscapeMode == ParseEscapeMode::ClassStringDisjunction) {
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

                if (escapeChar == 'P' && characterClassMayContainStrings(optClassID.value())) {
                    m_errorCode = ErrorCode::NegatedClassSetMayContainStrings;
                    break;
                }

                delegate.atomBuiltInCharacterClass(optClassID.value(), escapeChar == 'P');
            } else
                m_errorCode = ErrorCode::InvalidUnicodePropertyExpression;
            break;
        }

        // Class String Disjunction
        case 'q': {
            int escapeChar = consume();

            if (!isUnicodeSetsCompilation() || parseEscapeMode == ParseEscapeMode::ClassStringDisjunction) {
                if (isIdentityEscapeAnError(escapeChar))
                    break;
                delegate.atomPatternCharacter(escapeChar);
                break;
            }

            if (!atEndOfPattern() && peek() == '{')
                parseClassStringDisjunction();
            else
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

            if (ch == '-' && isEitherUnicodeCompilation() && parseEscapeMode != ParseEscapeMode::Normal) {
                // \- is allowed for ClassEscape with unicode flag.
                delegate.atomPatternCharacter(consume());
                break;
            }

            if (isIdentityEscapeAnError(ch))
                break;

            delegate.atomPatternCharacter(consume());
        }
        
        return TokenType::Atom;
    }

    template<UnicodeParseContext context>
    UChar32 consumePossibleSurrogatePair()
    {
        bool unicodePatternOrGroupName = isEitherUnicodeCompilation() || context == UnicodeParseContext::GroupName;

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

    inline UChar32 consumeAndCheckIfValidClassSetCharacter()
    {
        UChar32 ch = consumePossibleSurrogatePair<UnicodeParseContext::PatternCodePoint>();

        if (!ch) {
            m_errorCode = ErrorCode::InvalidClassSetCharacter;
            return -1;
        }

        if (isASCII(ch)) {
            // Check if the character is part of ClassSetSyntaxCharacter.
            // We leave handling of - and \ to the caller.
            if (strchr("()[]{}/|)", ch)) {
                m_errorCode = ErrorCode::InvalidClassSetCharacter;
                return -1;
            }

            // Check if the current character and the next are part of ClassSetReservedDoublePunctuator.
            if (!atEndOfPattern()) {
                UChar32 nextCh = peek();
                if (ch == nextCh && strchr("&!#$%*+,.:;<=>?@^`~", ch)) {
                    m_errorCode = ErrorCode::InvalidClassSetOperation;
                    return -1;
                }
            }
        }

        return ch;
    }

    /*
     * parseAtomEscape(), parseCharacterClassEscape(), parseClassSetEscape() and parseClassStringDisjunctionEscape():
     *
     * These methods alias to parseEscape().
     */
    TokenType parseAtomEscape()
    {
        return parseEscape<ParseEscapeMode::Normal>(m_delegate);
    }

    void parseCharacterClassEscape(CharacterClassParserDelegate& delegate)
    {
        parseEscape<ParseEscapeMode::CharacterClass>(delegate);
    }

    TokenType parseClassSetEscape(ClassSetParserDelegate& delegate)
    {
        return parseEscape<ParseEscapeMode::CharacterClass>(delegate);
    }

    void parseClassStringDisjunctionEscape(ClassStringDisjunctionParserDelegate& delegate)
    {
        parseEscape<ParseEscapeMode::ClassStringDisjunction>(delegate);
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

        CharacterClassParserDelegate characterClassConstructor(m_delegate, m_errorCode, m_compileMode);

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
     * parseClassSet():
     *
     * Helper for parseTokens() calls directly and indirectly (via parseClassSetEscape)
     * to an instance of CharacterClassParserDelegate, to describe the character class to the
     * delegate.
     */
    void parseClassSet()
    {
        ASSERT(!hasError(m_errorCode));
        ASSERT(peek() == '[');
        consume();

        ClassSetParserDelegate classSetConstructor(m_delegate, m_errorCode);

        classSetConstructor.begin(tryConsume('^'));

        auto processCharacterNormally = [&] () {
            UChar32 ch = consumeAndCheckIfValidClassSetCharacter();
            if (ch == -1)
                return;

            classSetConstructor.atomPatternCharacter(ch);
        };

        while (!atEndOfPattern()) {
            switch (peek()) {
            case ']':
                consume();
                if (classSetConstructor.nestedClassEnd())
                    return;
                break;

            case '[': {
                consume();
                classSetConstructor.nestedClassBegin();
                break;
            }

            case '\\':
                if (!classSetConstructor.canTakeSetOperand()) {
                    m_errorCode = ErrorCode::InvalidClassSetOperation;
                    return;
                }

                classSetConstructor.setProcessingEscape();
                parseClassSetEscape(classSetConstructor);
                classSetConstructor.afterOperand();
                break;

            case '-': {
                ParseState state = saveState();
                consume();
                if (peek() == '-') {
                    consume();
                    if (peek() == '-') {
                        m_errorCode = ErrorCode::InvalidClassSetCharacter;
                        return;
                    }
                    classSetConstructor.setSubtractOp();
                    break;
                }
                restoreState(state);
                processCharacterNormally();
                break;
            }

            case '&': {
                ParseState state = saveState();
                consume();
                if (peek() == '&') {
                    consume();
                    if (peek() == '&') {
                        m_errorCode = ErrorCode::InvalidClassSetCharacter;
                        return;
                    }
                    classSetConstructor.setIntersectionOp();
                    break;
                }
                restoreState(state);
                processCharacterNormally();
                break;
            }

            default:
                processCharacterNormally();
                break;
            }

            if (hasError(m_errorCode))
                return;
        }

        m_errorCode = ErrorCode::CharacterClassUnmatched;
    }

    /*
     * parseClassStringDisjunction():
     *
     * Helper for parseTokens() calls directly and indirectly (via parseClassStringDisjunctionEscape)
     * to an instance of ClassStringDisjunctionParserDelegate, to describe the Class String Disjunction to the
     * delegate.
     */
    void parseClassStringDisjunction()
    {
        ASSERT(!hasError(m_errorCode));
        ASSERT(peek() == '{');
        consume();

        ClassStringDisjunctionParserDelegate stringDisjunctionDelegate(m_delegate, m_errorCode);

        while (!atEndOfPattern()) {
            switch (peek()) {
            case '}':
                consume();
                stringDisjunctionDelegate.end();
                return;

            case '\\':
                parseClassStringDisjunctionEscape(stringDisjunctionDelegate);
                break;

            case '|':
                consume();
                stringDisjunctionDelegate.newAlternative();
                break;

            case '-':
                consume();
                m_errorCode = ErrorCode::InvalidClassSetCharacter;
                return;

            default:
                {
                    UChar32 ch = consumeAndCheckIfValidClassSetCharacter();

                    if (ch == -1)
                        return;

                    stringDisjunctionDelegate.atomPatternCharacter(ch);
                }
            }

            if (hasError(m_errorCode))
                return;
        }

        m_errorCode = ErrorCode::ClassStringDIsjunctionUnmatched;
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

                    auto setAddResult = m_namedCaptureGroups.add(groupName.value());
                    if (setAddResult.isNewEntry)
                        m_delegate.atomParenthesesSubpatternBegin(true, groupName);
                    else
                        m_errorCode = ErrorCode::DuplicateGroupName;
                } else {
                    if (tryConsume('=')) {
                        m_delegate.atomParentheticalAssertionBegin(false, Backward);
                        type = ParenthesesType::LookbehindAssertion;
                        break;
                    }

                    if (tryConsume('!')) {
                        m_delegate.atomParentheticalAssertionBegin(true, Backward);
                        type = ParenthesesType::LookbehindAssertion;
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
        m_namedCaptureGroups.pushParenthesis();
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
    TokenType parseParenthesesEnd()
    {
        ASSERT(!hasError(m_errorCode));
        ASSERT(peek() == ')');
        consume();

        if (m_parenthesesStack.isEmpty()) {
            m_errorCode = ErrorCode::ParenthesesUnmatched;
            return TokenType::NotAtom;
        }

        m_delegate.atomParenthesesEnd();

        m_namedCaptureGroups.popParenthesis();

        auto type = m_parenthesesStack.takeLast();
        if (type == ParenthesesType::LookbehindAssertion)
            return TokenType::Lookbehind;

        if (type == ParenthesesType::Subpattern || isLegacyCompilation())
            return TokenType::Atom;

        return TokenType::NotAtom;
    }

    /*
     * parseQuantifier():
     *
     * Helper for parseTokens(); checks for parse errors and non-greedy quantifiers.
     */
    void parseQuantifier(TokenType lastTokenType, unsigned min, unsigned max)
    {
        ASSERT(!hasError(m_errorCode));
        ASSERT(min <= max);

        if (min == UINT_MAX) {
            m_errorCode = ErrorCode::QuantifierTooLarge;
            return;
        }

        if (lastTokenType == TokenType::Atom)
            m_delegate.quantifyAtom(min, max, !tryConsume('?'));
        else if (lastTokenType == TokenType::Lookbehind)
            m_errorCode = ErrorCode::CantQuantifyAtom;
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
        TokenType lastTokenType = TokenType::NotAtom;

        while (!atEndOfPattern()) {
            switch (peek()) {
            case '|':
                consume();
                m_delegate.disjunction(CreateDisjunctionPurpose::ForNextAlternative);
                lastTokenType = TokenType::NotAtom;
                m_namedCaptureGroups.nextAlternative();
                break;

            case '(':
                parseParenthesesBegin();
                lastTokenType = TokenType::NotAtom;
                break;

            case ')':
                lastTokenType = parseParenthesesEnd();
                break;

            case '^':
                consume();
                m_delegate.assertionBOL();
                lastTokenType = TokenType::NotAtom;
                break;

            case '$':
                consume();
                m_delegate.assertionEOL();
                lastTokenType = TokenType::NotAtom;
                break;

            case '.':
                consume();
                m_delegate.atomBuiltInCharacterClass(BuiltInCharacterClassID::DotClassID, false);
                lastTokenType = TokenType::Atom;
                break;

            case '[':
                if (isUnicodeSetsCompilation())
                    parseClassSet();
                else
                    parseCharacterClass();
                lastTokenType = TokenType::Atom;
                break;

            case ']':
            case '}':
                if (isEitherUnicodeCompilation()) {
                    m_errorCode = ErrorCode::BracketUnmatched;
                    break;
                }

                m_delegate.atomPatternCharacter(consume());
                lastTokenType = TokenType::Atom;
                break;

            case '\\':
                lastTokenType = parseAtomEscape();
                break;

            case '*':
                consume();
                parseQuantifier(lastTokenType, 0, quantifyInfinite);
                lastTokenType = TokenType::NotAtom;
                break;

            case '+':
                consume();
                parseQuantifier(lastTokenType, 1, quantifyInfinite);
                lastTokenType = TokenType::NotAtom;
                break;

            case '?':
                consume();
                parseQuantifier(lastTokenType, 0, 1);
                lastTokenType = TokenType::NotAtom;
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
                            parseQuantifier(lastTokenType, min, max);
                        else
                            m_errorCode = ErrorCode::QuantifierOutOfOrder;
                        lastTokenType = TokenType::NotAtom;
                        break;
                    }
                }

                if (isEitherUnicodeCompilation()) {
                    m_errorCode = ErrorCode::QuantifierIncomplete;
                    break;
                }

                restoreState(state);
                // if we did not find a complete quantifer, fall through to the default case.
                FALLTHROUGH;
            }

            default:
                m_delegate.atomPatternCharacter(consumePossibleSurrogatePair<UnicodeParseContext::PatternCodePoint>());
                lastTokenType = TokenType::Atom;
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
            if (isEitherUnicodeCompilation()) {
                m_errorCode = ErrorCode::InvalidBackreference;
                return;
            }

            m_backReferenceLimit = m_numSubpatterns;
            shouldReparse = true;
        }

        if (m_kIdentityEscapeSeen && !m_namedCaptureGroups.isEmpty()) {
            m_errorCode = ErrorCode::InvalidNamedBackReference;
            return;
        }

        if (containsIllegalNamedForwardReference()) {
            // \k<a> is parsed as named reference in Unicode patterns because of strict IdentityEscape grammar.
            // See https://tc39.es/ecma262/#sec-patterns-static-semantics-early-errors
            if (isEitherUnicodeCompilation() || !m_namedCaptureGroups.isEmpty()) {
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

        if (m_namedCaptureGroups.isEmpty())
            return true;

        for (auto& entry : m_forwardReferenceNames) {
            if (!m_namedCaptureGroups.contains(entry))
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
        m_namedCaptureGroups.reset();
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

        bool unicodePatternOrGroupName = isEitherUnicodeCompilation() || context == UnicodeParseContext::GroupName;

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

                auto result = unicodeMatchProperty(expressionBuilder.toString(), m_compileMode);
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

    bool isLegacyCompilation() const { return m_compileMode == CompileMode::Legacy; }
    bool isUnicodeCompilation() const { return m_compileMode == CompileMode::Unicode; }
    bool isUnicodeSetsCompilation() const { return m_compileMode == CompileMode::UnicodeSets; }
    bool isEitherUnicodeCompilation() const { return isUnicodeCompilation() || isUnicodeSetsCompilation(); }

    enum class ParenthesesType : uint8_t { Subpattern, Assertion, LookbehindAssertion };

    Delegate& m_delegate;
    ErrorCode m_errorCode { ErrorCode::NoError };
    const CharType* m_data;
    unsigned m_size;
    unsigned m_index { 0 };
    CompileMode m_compileMode;
    unsigned m_backReferenceLimit;
    unsigned m_numSubpatterns { 0 };
    unsigned m_maxSeenBackReference { 0 };
    bool m_isNamedForwardReferenceAllowed;
    bool m_kIdentityEscapeSeen { false };
    Vector<ParenthesesType, 16> m_parenthesesStack;
    NamedCaptureGroups m_namedCaptureGroups;
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
 *    void atomClassStringDisjunction(Vector<Vector<UChar32>>&)
 *    void atomCharacterClassSetOp(CharacterClassSetOp setOp)
 *    void atomCharacterClassPushNested()
 *    void atomCharacterClassPopNested()
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
 *    void disjunction(CreateDisjunctionPurpose purpose);
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

inline CompileMode compileMode(std::optional<OptionSet<Flags>> flags)
{
    if (flags->contains(Flags::Unicode))
        return CompileMode::Unicode;

    if (flags->contains(Flags::UnicodeSets))
        return CompileMode::UnicodeSets;

    return CompileMode::Legacy;
}

template<class Delegate>
ErrorCode parse(Delegate& delegate, const StringView pattern, CompileMode compileMode, unsigned backReferenceLimit = quantifyInfinite, bool isNamedForwardReferenceAllowed = true)
{
    if (pattern.is8Bit())
        return Parser<Delegate, LChar>(delegate, pattern, compileMode, backReferenceLimit, isNamedForwardReferenceAllowed).parse();
    return Parser<Delegate, UChar>(delegate, pattern, compileMode, backReferenceLimit, isNamedForwardReferenceAllowed).parse();
}

} } // namespace JSC::Yarr
