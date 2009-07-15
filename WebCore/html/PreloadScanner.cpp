/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
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
#include "PreloadScanner.h"

#include "AtomicString.h"
#include "CachedCSSStyleSheet.h"
#include "CachedImage.h"
#include "CachedResource.h"
#include "CachedResourceClient.h"
#include "CachedScript.h"
#include "CSSHelper.h"
#include "CString.h"
#include "DocLoader.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
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

#define PRELOAD_DEBUG 0

using namespace WTF;

namespace WebCore {
    
using namespace HTMLNames;
    
PreloadScanner::PreloadScanner(Document* doc)
    : m_inProgress(false)
    , m_timeUsed(0)
    , m_bodySeen(false)
    , m_document(doc)
{
#if PRELOAD_DEBUG
    printf("CREATING PRELOAD SCANNER FOR %s\n", m_document->url().string().latin1().data());
#endif
}
    
PreloadScanner::~PreloadScanner()
{
#if PRELOAD_DEBUG
    printf("DELETING PRELOAD SCANNER FOR %s\n", m_document->url().string().latin1().data());
    printf("TOTAL TIME USED %.4fs\n", m_timeUsed);
#endif
}
    
void PreloadScanner::begin() 
{ 
    ASSERT(!m_inProgress); 
    reset(); 
    m_inProgress = true; 
}
    
void PreloadScanner::end() 
{ 
    ASSERT(m_inProgress); 
    m_inProgress = false; 
}

void PreloadScanner::reset()
{
    m_source.clear();
    
    m_state = Data;
    m_escape = false;
    m_contentModel = PCDATA;
    m_commentPos = 0;

    m_closeTag = false;
    m_tagName.clear();
    m_attributeName.clear();
    m_attributeValue.clear();
    m_lastStartTag = AtomicString();
    
    m_urlToLoad = String();
    m_charset = String();
    m_linkIsStyleSheet = false;
    m_lastCharacterIndex = 0;
    clearLastCharacters();
    
    m_cssState = CSSInitial;
    m_cssRule.clear();
    m_cssRuleValue.clear();
}
    
bool PreloadScanner::scanningBody() const
{
    return m_document->body() || m_bodySeen;
}
    
void PreloadScanner::write(const SegmentedString& source)
{
#if PRELOAD_DEBUG
    double startTime = currentTime();
#endif
    tokenize(source);
#if PRELOAD_DEBUG
    m_timeUsed += currentTime() - startTime;
#endif
}
    
static inline bool isWhitespace(UChar c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}
    
inline void PreloadScanner::clearLastCharacters()
{
    memset(m_lastCharacters, 0, lastCharactersBufferSize * sizeof(UChar));
}
    
inline void PreloadScanner::rememberCharacter(UChar c)
{
    m_lastCharacterIndex = (m_lastCharacterIndex + 1) % lastCharactersBufferSize;
    m_lastCharacters[m_lastCharacterIndex] = c;
}
    
inline bool PreloadScanner::lastCharactersMatch(const char* chars, unsigned count) const
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
    
unsigned PreloadScanner::consumeEntity(SegmentedString& source, bool& notEnoughCharacters)
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

void PreloadScanner::tokenize(const SegmentedString& source)
{
    ASSERT(m_inProgress);
    
    m_source.append(source);

    // This is a simplified HTML5 Tokenizer
    // http://www.whatwg.org/specs/web-apps/current-work/#tokenisation0
    while (!m_source.isEmpty()) {
        UChar cc = *m_source;
        switch (m_state) {
        case Data:
            while (1) {
                rememberCharacter(cc);
                if (cc == '&') {
                    if (m_contentModel == PCDATA || m_contentModel == RCDATA) {
                        m_state = EntityData;
                        break;
                    }
                } else if (cc == '-') {
                    if ((m_contentModel == RCDATA || m_contentModel == CDATA) && !m_escape) {
                        if (lastCharactersMatch("<!--", 4))
                            m_escape = true;
                    }
                } else if (cc == '<') {
                    if (m_contentModel == PCDATA || ((m_contentModel == RCDATA || m_contentModel == CDATA) && !m_escape)) {
                        m_state = TagOpen;
                        break;
                    }
                } else if (cc == '>') {
                     if ((m_contentModel == RCDATA || m_contentModel == CDATA) && m_escape) {
                         if (lastCharactersMatch("-->", 3))
                             m_escape = false;
                     }
                }
                emitCharacter(cc);
                m_source.advance();
                if (m_source.isEmpty())
                     return;
                cc = *m_source;
            }
            break;
        case EntityData:
            // should try to consume the entity but we only care about entities in attributes
            m_state = Data;
            break;
        case TagOpen:
            if (m_contentModel == RCDATA || m_contentModel == CDATA) {
                if (cc == '/')
                    m_state = CloseTagOpen;
                else {
                    m_state = Data;
                    continue;
                }
            } else if (m_contentModel == PCDATA) {
                if (cc == '!')
                    m_state = MarkupDeclarationOpen;
                else if (cc == '/')
                    m_state = CloseTagOpen;
                else if (cc >= 'A' && cc <= 'Z') {
                    m_tagName.clear();
                    m_charset = String();
                    m_tagName.append(cc + 0x20);
                    m_closeTag = false;
                    m_state = TagName;
                } else if (cc >= 'a' && cc <= 'z') {
                    m_tagName.clear();
                    m_charset = String();
                    m_tagName.append(cc);
                    m_closeTag = false;
                    m_state = TagName;
                } else if (cc == '>') {
                    m_state = Data;
                } else if (cc == '?') {
                    m_state = BogusComment;
                } else {
                    m_state = Data;
                    continue;
                }
            }
            break;
        case CloseTagOpen:
            if (m_contentModel == RCDATA || m_contentModel == CDATA) {
                if (!m_lastStartTag.length()) {
                    m_state = Data;
                    continue;
                }
                if (m_source.length() < m_lastStartTag.length() + 1)
                    return;
                Vector<UChar> tmpString;
                UChar tmpChar = 0;
                bool match = true;
                for (unsigned n = 0; n < m_lastStartTag.length() + 1; n++) {
                    tmpChar = Unicode::toLower(*m_source);
                    if (n < m_lastStartTag.length() && tmpChar != m_lastStartTag[n])
                        match = false;
                    tmpString.append(tmpChar);
                    m_source.advance();
                }
                m_source.prepend(SegmentedString(String(tmpString.data(), tmpString.size())));
                if (!match || (!isWhitespace(tmpChar) && tmpChar != '>' && tmpChar != '/')) {
                    m_state = Data;
                    continue;
                }
            }
            if (cc >= 'A' && cc <= 'Z') {
                m_tagName.clear();
                m_charset = String();
                m_tagName.append(cc + 0x20);
                m_closeTag = true;
                m_state = TagName;
            } else if (cc >= 'a' && cc <= 'z') {
                m_tagName.clear();
                m_charset = String();
                m_tagName.append(cc);
                m_closeTag = true;
                m_state = TagName;
            } else if (cc == '>') {
                m_state = Data;
            } else
                m_state = BogusComment;
            break;
        case TagName:
            while (1) {
                if (isWhitespace(cc)) {
                    m_state = BeforeAttributeName;
                    break;
                }
                if (cc == '>') {
                    emitTag();
                    m_state = Data;
                    break;
                }
                if (cc == '/') {
                    m_state = BeforeAttributeName;
                    break;
                }
                if (cc >= 'A' && cc <= 'Z')
                    m_tagName.append(cc + 0x20);
                else
                    m_tagName.append(cc);
                m_source.advance();
                if (m_source.isEmpty())
                    return;
                cc = *m_source;
            }
            break;
        case BeforeAttributeName:
            if (isWhitespace(cc))
                ;
            else if (cc == '>') {
                emitTag();
                m_state = Data;
            } else if (cc >= 'A' && cc <= 'Z') {
                m_attributeName.clear();
                m_attributeValue.clear();
                m_attributeName.append(cc + 0x20);
                m_state = AttributeName;
            } else if (cc == '/')
                ;
            else {
                m_attributeName.clear();
                m_attributeValue.clear();
                m_attributeName.append(cc);
                m_state = AttributeName;
            }
            break;
        case AttributeName:
            while (1) {
                if (isWhitespace(cc)) {
                    m_state = AfterAttributeName;
                    break;
                }
                if (cc == '=') {
                    m_state = BeforeAttributeValue;
                    break;
                }
                if (cc == '>') {
                    emitTag();
                    m_state = Data;
                    break;
                } 
                if (cc == '/') {
                    m_state = BeforeAttributeName;
                    break;
                }
                if (cc >= 'A' && cc <= 'Z')
                    m_attributeName.append(cc + 0x20);
                else
                    m_attributeName.append(cc);
                m_source.advance();
                if (m_source.isEmpty())
                    return;
                cc = *m_source;
            }
            break;
        case AfterAttributeName:
            if (isWhitespace(cc))
                ;
            else if (cc == '=')
                m_state = BeforeAttributeValue; 
            else if (cc == '>') {
                emitTag();
                m_state = Data;
            } else if (cc >= 'A' && cc <= 'Z') {
                m_attributeName.clear();
                m_attributeValue.clear();
                m_attributeName.append(cc + 0x20);
                m_state = AttributeName;
            } else if (cc == '/')
                m_state = BeforeAttributeName;
            else {
                m_attributeName.clear();
                m_attributeValue.clear();
                m_attributeName.append(cc);
                m_state = AttributeName;
            }
            break;
        case BeforeAttributeValue:
            if (isWhitespace(cc))
                ;
            else if (cc == '"')
                m_state = AttributeValueDoubleQuoted;
            else if (cc == '&') {
                m_state = AttributeValueUnquoted;
                continue;
            } else if (cc == '\'')
                m_state = AttributeValueSingleQuoted;
            else if (cc == '>') {
                emitTag();
                m_state = Data;
            } else {
                m_attributeValue.append(cc);
                m_state = AttributeValueUnquoted;
            }
            break;
        case AttributeValueDoubleQuoted:
            while (1) {
                if (cc == '"') {
                    processAttribute();
                    m_state = BeforeAttributeName;
                    break;
                }
                if (cc == '&') {
                    m_stateBeforeEntityInAttributeValue = m_state;
                    m_state = EntityInAttributeValue;
                    break;
                } 
                m_attributeValue.append(cc);
                m_source.advance();
                if (m_source.isEmpty())
                    return;
                cc = *m_source;
            }
            break;
        case AttributeValueSingleQuoted:
            while (1) {
                if (cc == '\'') {
                    processAttribute();
                    m_state = BeforeAttributeName;
                    break;
                }
                if (cc == '&') {
                    m_stateBeforeEntityInAttributeValue = m_state;
                    m_state = EntityInAttributeValue;
                    break;
                } 
                m_attributeValue.append(cc);
                m_source.advance();
                if (m_source.isEmpty())
                    return;
                cc = *m_source;
            }
            break;
        case AttributeValueUnquoted:
            while (1) {
                if (isWhitespace(cc)) {
                    processAttribute();
                    m_state = BeforeAttributeName;
                    break;
                }
                if (cc == '&') {
                    m_stateBeforeEntityInAttributeValue = m_state;
                    m_state = EntityInAttributeValue;
                    break;
                }
                if (cc == '>') {
                    processAttribute();
                    emitTag();
                    m_state = Data;
                    break;
                }
                m_attributeValue.append(cc);
                m_source.advance();
                if (m_source.isEmpty())
                    return;
                cc = *m_source;
            }
            break;
        case EntityInAttributeValue: 
            {
                bool notEnoughCharacters = false; 
                unsigned entity = consumeEntity(m_source, notEnoughCharacters);
                if (notEnoughCharacters)
                    return;
                if (entity > 0xFFFF) {
                    m_attributeValue.append(U16_LEAD(entity));
                    m_attributeValue.append(U16_TRAIL(entity));
                } else if (entity)
                    m_attributeValue.append(entity);
                else
                    m_attributeValue.append('&');
            }
            m_state = m_stateBeforeEntityInAttributeValue;
            continue;
        case BogusComment:
            while (1) {
                if (cc == '>') {
                    m_state = Data;
                    break;
                }
                m_source.advance();
                if (m_source.isEmpty())
                    return;
                cc = *m_source;
            }
            break;
        case MarkupDeclarationOpen: {
            if (cc == '-') {
                if (m_source.length() < 2)
                    return;
                m_source.advance();
                cc = *m_source;
                if (cc == '-')
                    m_state = CommentStart;
                else {
                    m_state = BogusComment;
                    continue;
                }
            // If we cared about the DOCTYPE we would test to enter those states here
            } else {
                m_state = BogusComment;
                continue;
            }
            break;
        }
        case CommentStart:
            if (cc == '-')
                m_state = CommentStartDash;
            else if (cc == '>')
                m_state = Data;
            else
                m_state = Comment;
            break;
        case CommentStartDash:
            if (cc == '-')
                m_state = CommentEnd;
            else if (cc == '>')
                m_state = Data;
            else
                m_state = Comment;
            break;
        case Comment:
            while (1) {
                if (cc == '-') {
                    m_state = CommentEndDash;
                    break;
                }
                m_source.advance();
                if (m_source.isEmpty())
                    return;
                cc = *m_source;
            }
            break;
        case CommentEndDash:
            if (cc == '-')
                m_state = CommentEnd;
            else 
                m_state = Comment;
            break;
        case CommentEnd:
            if (cc == '>')
                m_state = Data;
            else if (cc == '-')
                ;
            else 
                m_state = Comment;
            break;
        }
        m_source.advance();
    }
}
    
void PreloadScanner::processAttribute()
{
    AtomicString tag = AtomicString(m_tagName.data(), m_tagName.size());
    AtomicString attribute = AtomicString(m_attributeName.data(), m_attributeName.size());
    
    String value(m_attributeValue.data(), m_attributeValue.size());
    if (tag == scriptTag || tag == imgTag) {
        if (attribute == srcAttr && m_urlToLoad.isEmpty())
            m_urlToLoad = deprecatedParseURL(value);
        else if (attribute == charsetAttr)
            m_charset = value;
    } else if (tag == linkTag) {
        if (attribute == hrefAttr && m_urlToLoad.isEmpty())
            m_urlToLoad = deprecatedParseURL(value);
        else if (attribute == relAttr) {
            bool styleSheet = false;
            bool alternate = false;
            bool icon = false;
            bool dnsPrefetch = false;
            HTMLLinkElement::tokenizeRelAttribute(value, styleSheet, alternate, icon, dnsPrefetch);
            m_linkIsStyleSheet = styleSheet && !alternate && !icon && !dnsPrefetch;
        } else if (attribute == charsetAttr)
            m_charset = value;
    }
}
    
inline void PreloadScanner::emitCharacter(UChar c)
{
    if (m_contentModel == CDATA && m_lastStartTag == styleTag) 
        tokenizeCSS(c);
}
    
inline void PreloadScanner::tokenizeCSS(UChar c)
{    
    // We are just interested in @import rules, no need for real tokenization here
    // Searching for other types of resources is probably low payoff
    switch (m_cssState) {
    case CSSInitial:
        if (c == '@')
            m_cssState = CSSRuleStart;
        else if (c == '/')
            m_cssState = CSSMaybeComment;
        break;
    case CSSMaybeComment:
        if (c == '*')
            m_cssState = CSSComment;
        else
            m_cssState = CSSInitial;
        break;
    case CSSComment:
        if (c == '*')
            m_cssState = CSSMaybeCommentEnd;
        break;
    case CSSMaybeCommentEnd:
        if (c == '/')
            m_cssState = CSSInitial;
        else if (c == '*')
            ;
        else
            m_cssState = CSSComment;
        break;
    case CSSRuleStart:
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            m_cssRule.clear();
            m_cssRuleValue.clear();
            m_cssRule.append(c);
            m_cssState = CSSRule;
        } else
            m_cssState = CSSInitial;
        break;
    case CSSRule:
        if (isWhitespace(c))
            m_cssState = CSSAfterRule;
        else if (c == ';')
            m_cssState = CSSInitial;
        else
            m_cssRule.append(c);
        break;
    case CSSAfterRule:
        if (isWhitespace(c))
            ;
        else if (c == ';')
            m_cssState = CSSInitial;
        else {
            m_cssState = CSSRuleValue;
            m_cssRuleValue.append(c);
        }
        break;
    case CSSRuleValue:
        if (isWhitespace(c))
            m_cssState = CSSAfterRuleValue;
        else if (c == ';') {
            emitCSSRule();
            m_cssState = CSSInitial;
        } else 
            m_cssRuleValue.append(c);
        break;
    case CSSAfterRuleValue:
        if (isWhitespace(c))
            ;
        else if (c == ';') {
            emitCSSRule();
            m_cssState = CSSInitial;
        } else {
            // FIXME media rules
             m_cssState = CSSInitial;
        }
        break;
    }
}
    
void PreloadScanner::emitTag()
{
    if (m_closeTag) {
        m_contentModel = PCDATA;
        m_cssState = CSSInitial;
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
    
    if (tag == bodyTag)
        m_bodySeen = true;
    
    if (m_urlToLoad.isEmpty()) {
        m_linkIsStyleSheet = false;
        return;
    }
    
    if (tag == scriptTag)
        m_document->docLoader()->preload(CachedResource::Script, m_urlToLoad, m_charset, scanningBody());
    else if (tag == imgTag) 
        m_document->docLoader()->preload(CachedResource::ImageResource, m_urlToLoad, String(), scanningBody());
    else if (tag == linkTag && m_linkIsStyleSheet) 
        m_document->docLoader()->preload(CachedResource::CSSStyleSheet, m_urlToLoad, m_charset, scanningBody());

    m_urlToLoad = String();
    m_charset = String();
    m_linkIsStyleSheet = false;
}
    
void PreloadScanner::emitCSSRule()
{
    String rule(m_cssRule.data(), m_cssRule.size());
    if (equalIgnoringCase(rule, "import") && !m_cssRuleValue.isEmpty()) {
        String value(m_cssRuleValue.data(), m_cssRuleValue.size());
        String url = deprecatedParseURL(value);
        if (!url.isEmpty())
            m_document->docLoader()->preload(CachedResource::CSSStyleSheet, url, String(), scanningBody());
    }
    m_cssRule.clear();
    m_cssRuleValue.clear();
}
                
}
