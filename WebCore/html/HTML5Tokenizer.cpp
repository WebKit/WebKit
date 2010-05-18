/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
#include "HTML5Tokenizer.h"

#include "AtomicString.h"
#include "HTMLNames.h"
#include "NotImplemented.h"
#include <wtf/text/CString.h>
#include <wtf/CurrentTime.h>
#include <wtf/unicode/Unicode.h>

// Use __GNUC__ instead of PLATFORM(GCC) to stay consistent with the gperf generated c file
#ifdef __GNUC__
// The main tokenizer includes this too so we are getting two copies of the data. However, this way the code gets inlined.
#include "HTMLEntityNames.c"
#else
// Not inlined for non-GCC compilers
struct Entity {
    const char* name;
    int code;
};
const struct Entity* findEntity(register const char* str, register unsigned int len);
#endif

using namespace WTF;

namespace WebCore {

using namespace HTMLNames;

HTML5Tokenizer::HTML5Tokenizer()
{
}

HTML5Tokenizer::~HTML5Tokenizer()
{
}

void HTML5Tokenizer::begin() 
{ 
    reset(); 
}

void HTML5Tokenizer::end() 
{
}

void HTML5Tokenizer::reset()
{
    m_source.clear();

    m_state = DataState;
    m_escape = false;
    m_contentModel = PCDATA;
    m_commentPos = 0;

    m_closeTag = false;
    m_tagName.clear();
    m_attributeName.clear();
    m_attributeValue.clear();
    m_lastStartTag = AtomicString();

    m_lastCharacterIndex = 0;
    clearLastCharacters();
}

void HTML5Tokenizer::write(const SegmentedString& source)
{
    tokenize(source);
}

static inline bool isWhitespace(UChar c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

inline void HTML5Tokenizer::clearLastCharacters()
{
    memset(m_lastCharacters, 0, lastCharactersBufferSize * sizeof(UChar));
}

inline void HTML5Tokenizer::rememberCharacter(UChar c)
{
    m_lastCharacterIndex = (m_lastCharacterIndex + 1) % lastCharactersBufferSize;
    m_lastCharacters[m_lastCharacterIndex] = c;
}

inline bool HTML5Tokenizer::lastCharactersMatch(const char* chars, unsigned count) const
{
    unsigned pos = m_lastCharacterIndex;
    while (count) {
        if (chars[count - 1] != m_lastCharacters[pos])
            return false;
        --count;
        if (!pos)
            pos = lastCharactersBufferSize;
        --pos;
    }
    return true;
}
    
static inline unsigned legalEntityFor(unsigned value)
{
    // FIXME There is a table for more exceptions in the HTML5 specification.
    if (value == 0 || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
        return 0xFFFD;
    return value;
}
    
unsigned HTML5Tokenizer::consumeEntity(SegmentedString& source, bool& notEnoughCharacters)
{
    enum EntityState {
        Initial,
        NumberType,
        MaybeHex,
        Hex,
        Decimal,
        Named
    };
    EntityState entityState = Initial;
    unsigned result = 0;
    Vector<UChar, 10> seenChars;
    Vector<char, 10> entityName;

    while (!source.isEmpty()) {
        UChar cc = *source;
        seenChars.append(cc);
        switch (entityState) {
        case Initial:
            if (isWhitespace(cc) || cc == '<' || cc == '&')
                return 0;
            else if (cc == '#') 
                entityState = NumberType;
            else if ((cc >= 'a' && cc <= 'z') || (cc >= 'A' && cc <= 'Z')) {
                entityName.append(cc);
                entityState = Named;
            } else
                return 0;
            break;
        case NumberType:
            if (cc == 'x' || cc == 'X')
                entityState = MaybeHex;
            else if (cc >= '0' && cc <= '9') {
                entityState = Decimal;
                result = cc - '0';
            } else {
                source.push('#');
                return 0;
            }
            break;
        case MaybeHex:
            if (cc >= '0' && cc <= '9')
                result = cc - '0';
            else if (cc >= 'a' && cc <= 'f')
                result = 10 + cc - 'a';
            else if (cc >= 'A' && cc <= 'F')
                result = 10 + cc - 'A';
            else {
                source.push('#');
                source.push(seenChars[1]);
                return 0;
            }
            entityState = Hex;
            break;
        case Hex:
            if (cc >= '0' && cc <= '9')
                result = result * 16 + cc - '0';
            else if (cc >= 'a' && cc <= 'f')
                result = result * 16 + 10 + cc - 'a';
            else if (cc >= 'A' && cc <= 'F')
                result = result * 16 + 10 + cc - 'A';
            else if (cc == ';') {
                source.advance();
                return legalEntityFor(result);
            } else 
                return legalEntityFor(result);
            break;
        case Decimal:
            if (cc >= '0' && cc <= '9')
                result = result * 10 + cc - '0';
            else if (cc == ';') {
                source.advance();
                return legalEntityFor(result);
            } else
                return legalEntityFor(result);
            break;               
        case Named:
            // This is the attribute only version, generic version matches somewhat differently
            while (entityName.size() <= 8) {
                if (cc == ';') {
                    const Entity* entity = findEntity(entityName.data(), entityName.size());
                    if (entity) {
                        source.advance();
                        return entity->code;
                    }
                    break;
                }
                if (!(cc >= 'a' && cc <= 'z') && !(cc >= 'A' && cc <= 'Z') && !(cc >= '0' && cc <= '9')) {
                    const Entity* entity = findEntity(entityName.data(), entityName.size());
                    if (entity)
                        return entity->code;
                    break;
                }
                entityName.append(cc);
                source.advance();
                if (source.isEmpty())
                    goto outOfCharacters;
                cc = *source;
                seenChars.append(cc);
            }
            if (seenChars.size() == 2)
                source.push(seenChars[0]);
            else if (seenChars.size() == 3) {
                source.push(seenChars[0]);
                source.push(seenChars[1]);
            } else
                source.prepend(SegmentedString(String(seenChars.data(), seenChars.size() - 1)));
            return 0;
        }
        source.advance();
    }
outOfCharacters:
    notEnoughCharacters = true;
    source.prepend(SegmentedString(String(seenChars.data(), seenChars.size())));
    return 0;
}

void HTML5Tokenizer::tokenize(const SegmentedString& source)
{
    m_source.append(source);

    // Source: http://www.whatwg.org/specs/web-apps/current-work/#tokenisation0
    while (!m_source.isEmpty()) {
        UChar cc = *m_source;
        switch (m_state) {
        case DataState: {
            if (cc == '&')
                m_state = CharacterReferenceInDataState;
            else if (cc == '<')
                m_state = TagOpenState;
            else
                emitCharacter(cc);
            break;
        }
        case CharacterReferenceInDataState: {
            notImplemented();
            break;
        }
        case RCDATAState: {
            if (cc == '&')
                m_state = CharacterReferenceInRCDATAState;
            else if (cc == '<')
                m_state = RCDATALessThanSignState;
            else
                emitCharacter(cc);
            break;
        }
        case CharacterReferenceInRCDATAState: {
            notImplemented();
            break;
        }
        case RAWTEXTState: {
            if (cc == '<')
                m_state = RAWTEXTLessThanSignState;
            else
                emitCharacter(cc);
            break;
        }
        case ScriptDataState: {
            if (cc == '<')
                m_state = ScriptDataLessThanSignState;
            else
                emitCharacter(cc);
            break;
        }
        case PLAINTEXTState: {
            emitCharacter(cc);
            break;
        }
        case TagOpenState: {
            if (cc == '!')
                m_state = MarkupDeclarationOpenState;
            else if (cc == '/')
                m_state = EndTagOpenState;
            else if (cc >= 'A' && cc <= 'Z') {
                notImplemented();
                m_state = TagNameState;
            } else if (cc >= 'a' && cc <= 'z') {
                notImplemented();
                m_state = TagNameState;
            } else if (cc == '?') {
                emitParseError();
                m_state = BogusCommentState;
            } else {
                emitParseError();
                m_state = DataState;
                emitCharacter('<');
                continue;
            }
            break;
        }
        case EndTagOpenState: {
            if (cc >= 'A' && cc <= 'Z') {
                notImplemented();
                m_state = TagNameState;
            } else if (cc >= 'a' && cc <= 'z') {
                notImplemented();
                m_state = TagNameState;
            } else if (cc == '>') {
                emitParseError();
                m_state = DataState;
            } else {
                emitParseError();
                m_state = DataState;
            }
            // FIXME: Handle EOF properly.
            break;
        }
        case TagNameState: {
            if (cc == '\x09' || cc == '\x0A' || cc == '\x0C' || cc == ' ')
                m_state = BeforeAttributeNameState;
            else if (cc == '/')
                m_state = SelfClosingStartTagState;
            else if (cc == '>')
                m_state = DataState;
            else if (cc >= 'A' && cc <= 'Z')
                notImplemented();
            else
                notImplemented();
            // FIXME: Handle EOF properly.
            break;
        }
        case RCDATALessThanSignState:
        case RCDATAEndTagOpenState:
        case RCDATAEndTagNameState:
        case RAWTEXTLessThanSignState:
        case RAWTEXTEndTagOpenState:
        case RAWTEXTEndTagNameState:
        case ScriptDataLessThanSignState:
        case ScriptDataEndTagOpenState:
        case ScriptDataEndTagNameState:
        case ScriptDataEscapeStartState:
        case ScriptDataEscapeStartDashState:
        case ScriptDataEscapedState:
        case ScriptDataEscapedDashState:
        case ScriptDataEscapedDashDashState:
        case ScriptDataEscapedLessThanSignState:
        case ScriptDataEscapedEndTagOpenState:
        case ScriptDataEscapedEndtagNameState:
        case ScriptDataDoubleEscapeStartState:
        case ScriptDataDoubleEscapedState:
        case ScriptDataDoubleEscapedDashState:
        case ScriptDataDoubleEscapedDashDashState:
        case ScriptDataDoubleEscapedLessThanSignState:
        case ScriptDataDoubleEscapeEndState:
        case BeforeAttributeNameState:
        case AttributeNameState:
        case AfterAttributeNameState:
        case BeforeAttributeValueState:
        case AttributeValueDoubleQuotedState:
        case AttributeValueSingleQuotedState:
        case AttributeValueUnquotedState:
        case CharacterReferenceInAttributeValueState:
        case AfterAttributeValueQuotedState:
        case SelfClosingStartTagState:
        case BogusCommentState:
        case MarkupDeclarationOpenState:
        case CommentStartState:
        case CommentStartDashState:
        case CommentState:
        case CommentEndDashState:
        case CommentEndState:
        case CommentEndBangState:
        case CommentEndSpaceState:
        case DOCTYPEState:
        case BeforeDOCTYPENameState:
        case DOCTYPENameState:
        case AfterDOCTYPENameState:
        case AfterDOCTYPEPublicKeywordState:
        case BeforeDOCTYPEPublicIdentifierState:
        case DOCTYPEPublicIdentifierDoubleQuotedState:
        case DOCTYPEPublicIdentifierSingleQuotedState:
        case AfterDOCTYPEPublicIdentifierState:
        case BetweenDOCTYPEPublicAndSystemIdentifiersState:
        case AfterDOCTYPESystemKeywordState:
        case BeforeDOCTYPESystemIdentifierState:
        case DOCTYPESystemIdentifierDoubleQuotedState:
        case DOCTYPESystemIdentifierSingleQuotedState:
        case AfterDOCTYPESystemIdentifierState:
        case BogusDOCTYPEState:
        case CDATASectionState:
        case TokenizingCharacterReferencesState:
        default:
            notImplemented();
            break;
        }
        m_source.advance();
    }
}

void HTML5Tokenizer::processAttribute()
{
    AtomicString tag = AtomicString(m_tagName.data(), m_tagName.size());
    AtomicString attribute = AtomicString(m_attributeName.data(), m_attributeName.size());

    String value(m_attributeValue.data(), m_attributeValue.size());
}

inline void HTML5Tokenizer::emitCharacter(UChar)
{
}

inline void HTML5Tokenizer::emitParseError()
{
}

void HTML5Tokenizer::emitTag()
{
    if (m_closeTag) {
        m_contentModel = PCDATA;
        clearLastCharacters();
        return;
    }

    AtomicString tag(m_tagName.data(), m_tagName.size());
    m_lastStartTag = tag;

    if (tag == textareaTag || tag == titleTag)
        m_contentModel = RCDATA;
    else if (tag == styleTag || tag == xmpTag || tag == scriptTag || tag == iframeTag || tag == noembedTag || tag == noframesTag)
        m_contentModel = CDATA;
    else if (tag == noscriptTag)
        // we wouldn't be here if scripts were disabled
        m_contentModel = CDATA;
    else if (tag == plaintextTag)
        m_contentModel = PLAINTEXT;
    else
        m_contentModel = PCDATA;
}

}
