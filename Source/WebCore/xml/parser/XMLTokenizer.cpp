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
#include "XMLTokenizer.h"

#include "MarkupTokenizerInlineMethods.h"
#include "NotImplemented.h"
#include "XMLCharacterReferenceParser.h"
#include "XMLToken.h"
#include <wtf/ASCIICType.h>
#include <wtf/CurrentTime.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

using namespace WTF;

namespace WebCore {

// This has to go in a .cpp file, as the linker doesn't like it being included more than once.
// We don't have an XMLToken.cpp though, so this is the next best place.
template<>
QualifiedName AtomicMarkupTokenBase<XMLToken>::nameForAttribute(const XMLToken::Attribute& attribute) const
{
    return QualifiedName(attribute.m_prefix.isEmpty() ? nullAtom : AtomicString(attribute.m_prefix.data(), attribute.m_prefix.size()), AtomicString(attribute.m_name.data(), attribute.m_name.size()), nullAtom);
}

template<>
bool AtomicMarkupTokenBase<XMLToken>::usesName() const
{
    return m_type == XMLTokenTypes::StartTag || m_type == XMLTokenTypes::EndTag || m_type == XMLTokenTypes::DOCTYPE || m_type == XMLTokenTypes::Entity;
}

template<>
bool AtomicMarkupTokenBase<XMLToken>::usesAttributes() const
{
    return m_type == XMLTokenTypes::StartTag || m_type == XMLTokenTypes::EndTag;
}

namespace {

inline bool isValidNameStart(UChar cc)
{
    if (cc <= 0x40)
        return false;
    if (cc <= 0x5A)
        return true;
    if (cc == 0x5F)
        return true;
    if (cc <= 0x60)
        return false;
    if (cc <= 0x7A)
        return true;
    if (cc < 0xC0)
        return false;
    if (cc <= 0xD6)
        return true;
    if (cc == 0xD8)
        return false;
    if (cc <= 0xF6)
        return true;
    if (cc == 0xF7)
        return false;
    if (cc <= 0x2FF)
        return true;
    if (cc < 0x370)
        return false;
    if (cc <= 0x37D)
        return true;
    if (cc == 0x37E)
        return false;
    if (cc <= 0x1FFF)
        return true;
    if (cc < 0x200C)
        return false;
    if (cc <= 0x200D)
        return true;
    if (cc < 0x2070)
        return false;
    if (cc <= 0x218F)
        return true;
    if (cc < 0x2C00)
        return false;
    if (cc <= 0x2FEF)
        return true;
    if (cc < 0x3001)
        return false;
    if (cc <= 0xD7FF)
        return true;
    if (cc < 0xF900)
        return false;
    if (cc <= 0xFDCF)
        return true;
    if (cc < 0xFDF0)
        return false;
    if (cc <= 0xFFFD)
        return true;

    // FIXME: support non-BMP planes

    return false;
}

inline bool isValidNameChar(UChar cc)
{
    if (isValidNameStart(cc))
        return true;
    if (cc == '-' || cc == '.')
        return true;
    if (cc < 0x30)
        return false;
    if (cc < 0x3A)
        return true;
    if (cc < 0x0300)
        return false;
    if (cc <= 0x036F)
        return true;
    if (cc < 0x203F)
        return false;
    if (cc <= 0x2040)
        return true;

    return false;
}

inline bool isValidLiteralChar(UChar cc)
{
    if (cc == 0xD || cc == 0xA)
        return true;
    if (cc < 0x20)
        return false;
    if (cc == '"' || cc == '&')
        return false;
    if (cc < 0x3C)
        return true;
    if (cc == '<' || cc == '>')
        return false;
    if (cc < 0x5B)
        return true;
    if (cc == '_')
        return true;
    if (cc <= 0x60)
        return false;
    if (cc < 0x7B)
        return true;

    return false;
}

}

#define XML_BEGIN_STATE(stateName) BEGIN_STATE(XMLTokenizerState, stateName)
#define XML_ADVANCE_TO(stateName) ADVANCE_TO(XMLTokenizerState, stateName)
#define XML_SWITCH_TO(stateName) SWITCH_TO(XMLTokenizerState, stateName)

#define EQ_STATE(CurrentState, NextState) \
    XML_BEGIN_STATE(CurrentState) {       \
        if (isTokenizerWhitespace(cc))    \
            XML_ADVANCE_TO(CurrentState); \
        else if (cc == '=')               \
            XML_ADVANCE_TO(NextState);    \
        else {                            \
            parseError();                 \
            return emitEndOfFile(source); \
        }                                 \
    }                                     \
    END_STATE()

#define EQ_BEFORE_VALUE_STATES(EqualsState, BeforeValueState, ValueState) \
    EQ_STATE(EqualsState, BeforeValueState)    \
    XML_BEGIN_STATE(BeforeValueState) {        \
        if (isTokenizerWhitespace(cc))         \
            XML_ADVANCE_TO(BeforeValueState);  \
        else if (cc == '"' || cc == '\'') {    \
            m_additionalAllowedCharacter = cc; \
            XML_ADVANCE_TO(ValueState);        \
        } else {                               \
            parseError();                      \
            return emitEndOfFile(source);      \
        }                                      \
    }                                          \
    END_STATE()

XMLTokenizer::XMLTokenizer()
{
    reset();
}

template<>
inline bool MarkupTokenizerBase<XMLToken, XMLTokenizerState>::shouldSkipNullCharacters() const
{
    return false;
}

bool XMLTokenizer::nextToken(SegmentedString& source, XMLToken& token)
{
    // If we have a token in progress, then we're supposed to be called back
    // with the same token so we can finish it.
    ASSERT(!m_token || m_token == &token || token.type() == XMLTokenTypes::Uninitialized);
    m_token = &token;

    if (source.isEmpty() || !m_inputStreamPreprocessor.peek(source, m_lineNumber))
        return haveBufferedCharacterToken();
    UChar cc = m_inputStreamPreprocessor.nextInputCharacter();

    switch (m_state) {
    XML_BEGIN_STATE(DataState) {
        if (cc == '&')
            XML_ADVANCE_TO(CharacterReferenceStartState);
        else if (cc == '<') {
            if (m_token->type() == XMLTokenTypes::Character) {
                // We have a bunch of character tokens queued up that we
                // are emitting lazily here.
                return true;
            }
            XML_ADVANCE_TO(TagOpenState);
        } else if (cc == InputStreamPreprocessor::endOfFileMarker)
            return emitEndOfFile(source);
        else {
            bufferCharacter(cc);
            XML_ADVANCE_TO(DataState);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(CharacterReferenceStartState) {
        if (cc == '#') {
            bool notEnoughCharacters = false;
            StringBuilder decodedCharacter;
            if (consumeXMLCharacterReference(source, decodedCharacter, notEnoughCharacters)) {
                for (unsigned i = 0; i < decodedCharacter.length(); ++i)
                    bufferCharacter(decodedCharacter[i]);
                XML_SWITCH_TO(DataState);
            } else if (notEnoughCharacters)
                return haveBufferedCharacterToken();
        } else if (isValidNameStart(cc)) {
            if (m_token->type() == XMLTokenTypes::Character)
                return emitAndReconsumeIn(source, XMLTokenizerState::CharacterReferenceStartState);
            m_token->beginEntity(cc);
            XML_ADVANCE_TO(EntityReferenceState);
        }
        parseError();
        return emitEndOfFile(source);
    }
    END_STATE()

    XML_BEGIN_STATE(EntityReferenceState) {
        if (isValidNameChar(cc)) {
            m_token->appendToName(cc);
            XML_ADVANCE_TO(EntityReferenceState);
        } else if (cc == ';')
            return emitAndResumeIn(source, XMLTokenizerState::DataState);
        else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(TagOpenState) {
        if (cc == '!')
            XML_ADVANCE_TO(MarkupDeclarationOpenState);
        else if (cc == '/')
            XML_ADVANCE_TO(EndTagOpenState);
        else if (isValidNameStart(cc)) {
            m_token->beginStartTag(cc);
            XML_ADVANCE_TO(TagNameState);
        } else if (cc == '?')
            XML_ADVANCE_TO(ProcessingInstructionTargetStartState);
        else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(EndTagOpenState) {
        if (isValidNameStart(cc)) {
            m_token->beginEndTag(cc);
            XML_ADVANCE_TO(EndTagNameState);
        } else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(TagNameState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(BeforeAttributeNameState);
        else if (cc == '/')
            XML_ADVANCE_TO(SelfClosingStartTagState);
        else if (cc == '>')
            return emitAndResumeIn(source, XMLTokenizerState::DataState);
        else if (isValidNameChar(cc)) {
            m_token->appendToName(cc);
            XML_ADVANCE_TO(TagNameState);
        } else if (cc == ':' && !m_token->hasPrefix()) {
            m_token->endPrefix();
            XML_ADVANCE_TO(TagNameState);
        } else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(EndTagNameState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(EndTagSpaceState);
        else if (cc == '>')
            return emitAndResumeIn(source, XMLTokenizerState::DataState);
        else if (isValidNameChar(cc)) {
            m_token->appendToName(cc);
            XML_ADVANCE_TO(EndTagNameState);
        } else if (cc == ':' && !m_token->hasPrefix()) {
            m_token->endPrefix();
            XML_ADVANCE_TO(EndTagNameState);
        } else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(EndTagSpaceState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(EndTagSpaceState);
        else if (cc == '>')
            return emitAndResumeIn(source, XMLTokenizerState::DataState);
        else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(BeforeAttributeNameState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(BeforeAttributeNameState);
        else if (cc == '/')
            XML_ADVANCE_TO(SelfClosingStartTagState);
        else if (cc == '>')
            return emitAndResumeIn(source, XMLTokenizerState::DataState);
        else if (isValidNameStart(cc)) {
            m_token->addNewAttribute();
            m_token->beginAttributeName(source.numberOfCharactersConsumed());
            m_token->appendToAttributeName(cc);
            XML_ADVANCE_TO(AttributeNameState);
        } else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(AttributeNameState) {
        if (isTokenizerWhitespace(cc)) {
            m_token->endAttributeName(source.numberOfCharactersConsumed());
            XML_ADVANCE_TO(AfterAttributeNameState);
        } else if (cc == '=') {
            m_token->endAttributeName(source.numberOfCharactersConsumed());
            XML_ADVANCE_TO(BeforeAttributeValueState);
        } else if (isValidNameChar(cc)) {
            m_token->appendToAttributeName(cc);
            XML_ADVANCE_TO(AttributeNameState);
        } else if (cc == ':' && !m_token->attributeHasPrefix()) {
            m_token->endAttributePrefix(source.numberOfCharactersConsumed());
            XML_ADVANCE_TO(AttributeNameState);
        } else {
            parseError();
            m_token->endAttributeName(source.numberOfCharactersConsumed());
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    EQ_STATE(AfterAttributeNameState, BeforeAttributeValueState)

    XML_BEGIN_STATE(BeforeAttributeValueState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(BeforeAttributeValueState);
        else if (cc == '"' || cc == '\'') {
            m_token->beginAttributeValue(source.numberOfCharactersConsumed() + 1);
            m_additionalAllowedCharacter = cc;
            XML_ADVANCE_TO(AttributeValueQuotedState);
        } else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(AttributeValueQuotedState) {
        if (cc == m_additionalAllowedCharacter) {
            m_token->endAttributeValue(source.numberOfCharactersConsumed());
            XML_ADVANCE_TO(AfterAttributeValueQuotedState);
        } else if (cc == '&')
            XML_ADVANCE_TO(CharacterReferenceInAttributeValueState);
        else if (cc == '<' || cc == InputStreamPreprocessor::endOfFileMarker) {
            parseError();
            m_token->endAttributeValue(source.numberOfCharactersConsumed());
            return emitEndOfFile(source);
        } else {
            m_token->appendToAttributeValue(cc);
            XML_ADVANCE_TO(AttributeValueQuotedState);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(CharacterReferenceInAttributeValueState) {
        if (cc == '#') {
            bool notEnoughCharacters = false;
            StringBuilder decodedCharacter;
            source.push(cc);
            if (consumeXMLCharacterReference(source, decodedCharacter, notEnoughCharacters)) {
                for (unsigned i = 0; i < decodedCharacter.length(); ++i)
                    m_token->appendToAttributeValue(decodedCharacter[i]);
                XML_ADVANCE_TO(AttributeValueQuotedState);
            } else if (notEnoughCharacters)
                return haveBufferedCharacterToken();
            else {
                parseError();
                return emitEndOfFile(source);
            }
        } else {
            m_token->appendToAttributeValue('&');
            m_token->appendToAttributeValue(cc);
            XML_ADVANCE_TO(AttributeValueQuotedState);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(AfterAttributeValueQuotedState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(BeforeAttributeNameState);
        else if (cc == '/')
            XML_ADVANCE_TO(SelfClosingStartTagState);
        else if (cc == '>')
            return emitAndResumeIn(source, XMLTokenizerState::DataState);
        else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(SelfClosingStartTagState) {
        if (cc == '>') {
            m_token->setSelfClosing();
            return emitAndResumeIn(source, XMLTokenizerState::DataState);
        }
        parseError();
        return emitEndOfFile(source);
    }
    END_STATE()

    XML_BEGIN_STATE(ProcessingInstructionTargetStartState) {
        DEFINE_STATIC_LOCAL(String, xmlString, ("xml"));
        // FIXME: this probably shouldn't be case-insensitive, but I don't know if people try capitalizing it ever.
        if (cc == 'x' || cc == 'X') {
            SegmentedString::LookAheadResult result = source.lookAheadIgnoringCase(xmlString);
            if (result == SegmentedString::DidMatch) {
                advanceStringAndASSERTIgnoringCase(source, "xml");
                XML_SWITCH_TO(XMLDeclAfterXMLState);
            } else if (result == SegmentedString::NotEnoughCharacters)
                return haveBufferedCharacterToken();
        }
        if (m_token->type() == XMLTokenTypes::ProcessingInstruction && isValidNameChar(cc))
            m_token->appendToProcessingInstructionTarget(cc);
        else if (isValidNameStart(cc))
            m_token->beginProcessingInstruction(cc);
        else {
            parseError();
            return emitEndOfFile(source);
        }
        XML_ADVANCE_TO(ProcessingInstructionTargetState);
    }
    END_STATE()

    XML_BEGIN_STATE(XMLDeclAfterXMLState) {
        if (isTokenizerWhitespace(cc)) {
            m_token->beginXMLDeclaration();
            XML_ADVANCE_TO(XMLDeclBeforeVersionNameState);
        } else if (isValidNameChar(cc)) {
            m_token->beginProcessingInstruction('x');
            m_token->appendToProcessingInstructionTarget('m');
            m_token->appendToProcessingInstructionTarget('l');
            m_token->appendToProcessingInstructionTarget(cc);
            XML_ADVANCE_TO(ProcessingInstructionTargetState);
        }
        else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(XMLDeclBeforeVersionNameState) {
        DEFINE_STATIC_LOCAL(String, versionString, ("version"));
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(XMLDeclBeforeVersionNameState);
        else {
            SegmentedString::LookAheadResult result = source.lookAhead(versionString);
            if (result == SegmentedString::DidMatch) {
                advanceStringAndASSERT(source, "version");
                XML_SWITCH_TO(XMLDeclAfterVersionNameState);
            } else if (result == SegmentedString::NotEnoughCharacters)
                return haveBufferedCharacterToken();
        }
        parseError();
        return emitEndOfFile(source);
    }
    END_STATE()

    EQ_BEFORE_VALUE_STATES(XMLDeclAfterVersionNameState, XMLDeclBeforeVersionValueState, XMLDeclBeforeVersionOnePointState)

    XML_BEGIN_STATE(XMLDeclBeforeVersionOnePointState) {
        DEFINE_STATIC_LOCAL(String, onePointString, ("1."));
        SegmentedString::LookAheadResult result = source.lookAhead(onePointString);
        if (result == SegmentedString::DidMatch) {
            source.advanceAndASSERT('1');
            source.advanceAndASSERT('.');
            m_token->appendToXMLVersion('1');
            m_token->appendToXMLVersion('.');
            XML_SWITCH_TO(XMLDeclVersionValueQuotedState);
        } else if (result == SegmentedString::NotEnoughCharacters)
            return haveBufferedCharacterToken();
        parseError();
        return emitEndOfFile(source);
    }
    END_STATE()

    XML_BEGIN_STATE(XMLDeclVersionValueQuotedState) {
        if (cc == m_additionalAllowedCharacter) {
            XML_ADVANCE_TO(XMLDeclAfterVersionState);
        } else if (isASCIIDigit(cc)) {
            m_token->appendToXMLVersion(cc);
            XML_ADVANCE_TO(XMLDeclVersionValueQuotedState);
        } else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(XMLDeclAfterVersionState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(XMLDeclBeforeEncodingNameState);
        else if (cc == '?')
            XML_ADVANCE_TO(XMLDeclCloseState);
        else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(XMLDeclBeforeEncodingNameState) {
        DEFINE_STATIC_LOCAL(String, encodingString, ("encoding"));
        DEFINE_STATIC_LOCAL(String, standaloneString, ("standalone"));
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(XMLDeclBeforeEncodingNameState);
        else if (cc == 'e') {
            SegmentedString::LookAheadResult result = source.lookAhead(encodingString);
            if (result == SegmentedString::DidMatch) {
                advanceStringAndASSERT(source, "encoding");
                XML_SWITCH_TO(XMLDeclAfterEncodingNameState);
            } else if (result == SegmentedString::NotEnoughCharacters)
                return haveBufferedCharacterToken();
        } else if (cc == 's') {
            SegmentedString::LookAheadResult result = source.lookAhead(standaloneString);
            if (result == SegmentedString::DidMatch) {
                advanceStringAndASSERT(source, "standalone");
                XML_SWITCH_TO(XMLDeclAfterStandaloneNameState);
            } else if (result == SegmentedString::NotEnoughCharacters)
                return haveBufferedCharacterToken();
        } else if (cc == '?')
            XML_ADVANCE_TO(XMLDeclCloseState);
        parseError();
        return emitEndOfFile(source);
    }
    END_STATE()

    EQ_BEFORE_VALUE_STATES(XMLDeclAfterEncodingNameState, XMLDeclBeforeEncodingValueState, XMLDeclEncodingValueStartQuotedState)

    XML_BEGIN_STATE(XMLDeclEncodingValueStartQuotedState) {
        if (isASCIIAlpha(cc)) {
            m_token->beginXMLEncoding(cc);
            XML_ADVANCE_TO(XMLDeclEncodingValueQuotedState);
        }
        parseError();
        return emitEndOfFile(source);
    }
    END_STATE()

    XML_BEGIN_STATE(XMLDeclEncodingValueQuotedState) {
        if (cc == m_additionalAllowedCharacter) {
            XML_ADVANCE_TO(XMLDeclAfterEncodingState);
        } else if (isASCIIAlphanumeric(cc) || cc == '-') {
            m_token->appendToXMLEncoding(cc);
            XML_ADVANCE_TO(XMLDeclEncodingValueQuotedState);
        } else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(XMLDeclAfterEncodingState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(XMLDeclBeforeStandaloneNameState);
        else if (cc == '?')
            XML_ADVANCE_TO(XMLDeclCloseState);
        else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(XMLDeclBeforeStandaloneNameState) {
        DEFINE_STATIC_LOCAL(String, standaloneString, ("standalone"));
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(XMLDeclBeforeStandaloneNameState);
        else if (cc == 's') {
            SegmentedString::LookAheadResult result = source.lookAhead(standaloneString);
            if (result == SegmentedString::DidMatch) {
                advanceStringAndASSERT(source, "standalone");
                XML_SWITCH_TO(XMLDeclAfterStandaloneNameState);
            } else if (result == SegmentedString::NotEnoughCharacters)
                return haveBufferedCharacterToken();
        } else if (cc == '?')
            XML_ADVANCE_TO(XMLDeclCloseState);
        parseError();
        return emitEndOfFile(source);
    }
    END_STATE()

    EQ_BEFORE_VALUE_STATES(XMLDeclAfterStandaloneNameState, XMLDeclBeforeStandaloneValueState, XMLDeclStandaloneValueQuotedState)

    XML_BEGIN_STATE(XMLDeclStandaloneValueQuotedState) {
        DEFINE_STATIC_LOCAL(String, yesString, ("yes\""));
        DEFINE_STATIC_LOCAL(String, noString, ("no\""));
        if (cc == 'y') {
            SegmentedString::LookAheadResult result = source.lookAhead(yesString);
            if (result == SegmentedString::DidMatch) {
                advanceStringAndASSERT(source, "yes\"");
                m_token->setXMLStandalone(true);
                XML_SWITCH_TO(XMLDeclAfterStandaloneState);
            } else if (result == SegmentedString::NotEnoughCharacters)
                return haveBufferedCharacterToken();
        } else if (cc == 'n') {
            SegmentedString::LookAheadResult result = source.lookAhead(noString);
            if (result == SegmentedString::DidMatch) {
                advanceStringAndASSERT(source, "no\"");
                m_token->setXMLStandalone(false);
                XML_SWITCH_TO(XMLDeclAfterStandaloneState);
            } else if (result == SegmentedString::NotEnoughCharacters)
                return haveBufferedCharacterToken();
        }
        parseError();
        return emitEndOfFile(source);
    }
    END_STATE()

    XML_BEGIN_STATE(XMLDeclAfterStandaloneState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(XMLDeclAfterStandaloneState);
        if (cc == '?')
            XML_ADVANCE_TO(XMLDeclCloseState);
        else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(XMLDeclCloseState) {
        if (cc == '>')
            return emitAndResumeIn(source, XMLTokenizerState::DataState);
        parseError();
        return emitEndOfFile(source);
    }
    END_STATE()

    XML_BEGIN_STATE(ProcessingInstructionTargetState) {
        if (isTokenizerWhitespace(cc)) {
            XML_ADVANCE_TO(ProcessingInstructionAfterTargetState);
        } else if (isValidNameChar(cc)) {
            m_token->appendToProcessingInstructionTarget(cc);
            XML_ADVANCE_TO(ProcessingInstructionTargetState);
        } else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(ProcessingInstructionAfterTargetState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(ProcessingInstructionAfterTargetState);
        else if (cc == '?')
            XML_ADVANCE_TO(ProcessingInstructionCloseState);
        else if (cc == InputStreamPreprocessor::endOfFileMarker) {
            parseError();
            return emitEndOfFile(source);
        } else {
            m_token->appendToProcessingInstructionData(cc);
            XML_ADVANCE_TO(ProcessingInstructionDataState);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(ProcessingInstructionDataState) {
        if (cc == '?')
            XML_ADVANCE_TO(ProcessingInstructionCloseState);
        else if (cc == InputStreamPreprocessor::endOfFileMarker) {
            parseError();
            return emitEndOfFile(source);
        } else {
            m_token->appendToProcessingInstructionData(cc);
            XML_ADVANCE_TO(ProcessingInstructionDataState);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(ProcessingInstructionCloseState) {
        if (cc == '>')
            return emitAndResumeIn(source, XMLTokenizerState::DataState);
        if (cc == InputStreamPreprocessor::endOfFileMarker) {
            parseError();
            return emitEndOfFile(source);
        }
        m_token->appendToProcessingInstructionData('?');
        m_token->appendToProcessingInstructionData(cc);
        XML_ADVANCE_TO(ProcessingInstructionDataState);
    }
    END_STATE()

    XML_BEGIN_STATE(MarkupDeclarationOpenState) {
        DEFINE_STATIC_LOCAL(String, dashDashString, ("--"));
        DEFINE_STATIC_LOCAL(String, doctypeString, ("doctype"));
        DEFINE_STATIC_LOCAL(String, cdataString, ("[CDATA["));
        if (cc == '-') {
            SegmentedString::LookAheadResult result = source.lookAhead(dashDashString);
            if (result == SegmentedString::DidMatch) {
                source.advanceAndASSERT('-');
                source.advanceAndASSERT('-');
                m_token->beginComment();
                XML_SWITCH_TO(CommentState);
            } else if (result == SegmentedString::NotEnoughCharacters)
                return haveBufferedCharacterToken();
        } else if (cc == 'D' || cc == 'd') {
            SegmentedString::LookAheadResult result = source.lookAheadIgnoringCase(doctypeString);
            if (result == SegmentedString::DidMatch) {
                advanceStringAndASSERTIgnoringCase(source, "doctype");
                XML_SWITCH_TO(BeforeDOCTYPENameState);
            } else if (result == SegmentedString::NotEnoughCharacters)
                return haveBufferedCharacterToken();
        } else if (cc == '[') {
            SegmentedString::LookAheadResult result = source.lookAhead(cdataString);
            if (result == SegmentedString::DidMatch) {
                advanceStringAndASSERT(source, "[CDATA[");
                m_token->beginCDATA();
                XML_SWITCH_TO(CDATASectionState);
            } else if (result == SegmentedString::NotEnoughCharacters)
                return haveBufferedCharacterToken();
        }
        parseError();
        return emitEndOfFile(source);
    }
    END_STATE()

    XML_BEGIN_STATE(CommentState) {
        if (cc == '-')
            XML_ADVANCE_TO(CommentDashState);
        else if (cc == InputStreamPreprocessor::endOfFileMarker) {
            parseError();
            return emitEndOfFile(source);
        } else {
            m_token->appendToComment(cc);
            XML_ADVANCE_TO(CommentState);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(CommentDashState) {
        if (cc == '-')
            XML_ADVANCE_TO(CommentEndState);
        else if (cc == InputStreamPreprocessor::endOfFileMarker) {
            parseError();
            return emitEndOfFile(source);
        } else {
            m_token->appendToComment('-');
            m_token->appendToComment(cc);
            XML_ADVANCE_TO(CommentState);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(CommentEndState) {
        if (cc == '>')
            return emitAndResumeIn(source, XMLTokenizerState::DataState);
        if (cc == '-') {
            parseError();
            return emitEndOfFile(source);
        }
        parseError();
        return emitAndReconsumeIn(source, XMLTokenizerState::DataState);
    }
    END_STATE()

    XML_BEGIN_STATE(BeforeDOCTYPENameState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(BeforeDOCTYPENameState);
        else if (isValidNameStart(cc)) {
            m_token->beginDOCTYPE(cc);
            XML_ADVANCE_TO(DOCTYPENameState);
        } else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(DOCTYPENameState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(AfterDOCTYPENameState);
        else if (cc == '>')
            return emitAndResumeIn(source, XMLTokenizerState::DataState);
        else if (isValidNameChar(cc)) {
            m_token->appendToName(cc);
            XML_ADVANCE_TO(DOCTYPENameState);
        } else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(AfterDOCTYPENameState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(AfterDOCTYPENameState);
        if (cc == '>')
            return emitAndResumeIn(source, XMLTokenizerState::DataState);
        if (cc == InputStreamPreprocessor::endOfFileMarker) {
            parseError();
            return emitEndOfFile(source);
        }
        DEFINE_STATIC_LOCAL(String, publicString, ("public"));
        DEFINE_STATIC_LOCAL(String, systemString, ("system"));
        if (cc == 'P' || cc == 'p') {
            SegmentedString::LookAheadResult result = source.lookAheadIgnoringCase(publicString);
            if (result == SegmentedString::DidMatch) {
                advanceStringAndASSERTIgnoringCase(source, "public");
                XML_SWITCH_TO(AfterDOCTYPEPublicKeywordState);
            } else if (result == SegmentedString::NotEnoughCharacters)
                return haveBufferedCharacterToken();
        } else if (cc == 'S' || cc == 's') {
            SegmentedString::LookAheadResult result = source.lookAheadIgnoringCase(systemString);
            if (result == SegmentedString::DidMatch) {
                advanceStringAndASSERTIgnoringCase(source, "system");
                XML_SWITCH_TO(AfterDOCTYPESystemKeywordState);
            } else if (result == SegmentedString::NotEnoughCharacters)
                return haveBufferedCharacterToken();
        } else if (cc == '[')
            XML_ADVANCE_TO(BeforeDOCTYPEInternalSubsetState);
        else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(AfterDOCTYPEPublicKeywordState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(BeforeDOCTYPEPublicIdentifierState);
        else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(BeforeDOCTYPEPublicIdentifierState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(BeforeDOCTYPEPublicIdentifierState);
        else if (cc == '"' || cc == '\'') {
            m_token->setPublicIdentifierToEmptyString();
            m_additionalAllowedCharacter = cc;
            XML_ADVANCE_TO(DOCTYPEPublicIdentifierQuotedState);
        } else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(DOCTYPEPublicIdentifierQuotedState) {
        if (cc == m_additionalAllowedCharacter)
            XML_ADVANCE_TO(AfterDOCTYPEPublicIdentifierState);
        else if (cc == InputStreamPreprocessor::endOfFileMarker) {
            parseError();
            return emitEndOfFile(source);
        } else {
            m_token->appendToPublicIdentifier(cc);
            XML_ADVANCE_TO(DOCTYPEPublicIdentifierQuotedState);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(AfterDOCTYPEPublicIdentifierState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(BeforeDOCTYPESystemIdentifierState);
        else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(AfterDOCTYPESystemKeywordState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(BeforeDOCTYPESystemIdentifierState);
        else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(BeforeDOCTYPESystemIdentifierState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(BeforeDOCTYPESystemIdentifierState);
        else if (cc == '"' || cc == '\'') {
            m_token->setSystemIdentifierToEmptyString();
            m_additionalAllowedCharacter = cc;
            XML_ADVANCE_TO(DOCTYPESystemIdentifierQuotedState);
        } else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(DOCTYPESystemIdentifierQuotedState) {
        if (cc == m_additionalAllowedCharacter)
            XML_ADVANCE_TO(AfterDOCTYPESystemIdentifierState);
        else if (isValidLiteralChar(cc)) {
            m_token->appendToSystemIdentifier(cc);
            XML_ADVANCE_TO(DOCTYPESystemIdentifierQuotedState);
        } else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(AfterDOCTYPESystemIdentifierState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(AfterDOCTYPESystemIdentifierState);
        else if (cc == '>')
            return emitAndResumeIn(source, XMLTokenizerState::DataState);
        else if (cc == '[')
            XML_ADVANCE_TO(BeforeDOCTYPEInternalSubsetState);
        else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(BeforeDOCTYPEInternalSubsetState) {
        if (cc == ']')
            XML_ADVANCE_TO(AfterDOCTYPEInternalSubsetState);
        else {
            // FIXME implement internal subset
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(AfterDOCTYPEInternalSubsetState) {
        if (isTokenizerWhitespace(cc))
            XML_ADVANCE_TO(AfterDOCTYPEInternalSubsetState);
        else if (cc == '>')
            return emitAndResumeIn(source, XMLTokenizerState::DataState);
        else {
            parseError();
            return emitEndOfFile(source);
        }
    }
    END_STATE()

    XML_BEGIN_STATE(CDATASectionState) {
        DEFINE_STATIC_LOCAL(String, closeString, ("]]>"));
        if (cc == ']') {
            SegmentedString::LookAheadResult result = source.lookAhead(closeString);
            if (result == SegmentedString::DidMatch) {
                advanceStringAndASSERT(source, "]]>");
                return emitAndReconsumeIn(source, XMLTokenizerState::DataState);
            }
            if (result == SegmentedString::NotEnoughCharacters)
                return haveBufferedCharacterToken();
        }
        if (cc == InputStreamPreprocessor::endOfFileMarker) {
            parseError();
            return emitEndOfFile(source);
        }
        m_token->appendToCDATA(cc);
        XML_ADVANCE_TO(CDATASectionState);
    }
    END_STATE()

    }

    ASSERT_NOT_REACHED();
    return false;
}

inline void XMLTokenizer::bufferCharacter(UChar character)
{
    ASSERT(character != InputStreamPreprocessor::endOfFileMarker);
    m_token->ensureIsCharacterToken();
    m_token->appendToCharacter(character);
}

inline void XMLTokenizer::parseError()
{
    m_errorDuringParsing = true;
}

}
