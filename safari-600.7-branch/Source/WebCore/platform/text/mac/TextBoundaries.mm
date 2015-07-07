/*
 * Copyright (C) 2004, 2006, 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "TextBoundaries.h"

#import "TextBreakIterator.h"
#import "TextBreakIteratorInternalICU.h"
#import <CoreFoundation/CFStringTokenizer.h>
#import <Foundation/Foundation.h>
#import <unicode/ubrk.h>
#import <unicode/uchar.h>
#import <unicode/ustring.h>
#import <unicode/utypes.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/StringView.h>
#import <wtf/unicode/CharacterNames.h>

namespace WebCore {

#if !USE(APPKIT)

static bool isSkipCharacter(UChar32 c)
{
    return c == 0xA0 || c == '\n' || c == '.' || c == ',' || c == '!'  || c == '?' || c == ';' || c == ':' || u_isspace(c);
}

static bool isWhitespaceCharacter(UChar32 c)
{
    return c == 0xA0 || c == '\n' || u_isspace(c);
}

static bool isWordDelimitingCharacter(UChar32 c)
{
    // Ampersand is an exception added to treat AT&T as a single word (see <rdar://problem/5022264>).
    return !CFCharacterSetIsLongCharacterMember(CFCharacterSetGetPredefined(kCFCharacterSetAlphaNumeric), c) && c != '&';
}

static bool isSymbolCharacter(UChar32 c)
{
    return CFCharacterSetIsLongCharacterMember(CFCharacterSetGetPredefined(kCFCharacterSetSymbol), c);
}

static bool isAmbiguousBoundaryCharacter(UChar32 character)
{
    // These are characters that can behave as word boundaries, but can appear within words.
    return character == '\'' || character == rightSingleQuotationMark || character == hebrewPunctuationGershayim;
}

static CFStringTokenizerRef tokenizerForString(CFStringRef str)
{
    static CFLocaleRef locale = nullptr;
    if (!locale) {
        const char* temp = currentTextBreakLocaleID();
        RetainPtr<CFStringRef> currentLocaleID = adoptCF(CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(temp), strlen(temp), kCFStringEncodingASCII, false, kCFAllocatorNull));
        locale = CFLocaleCreate(kCFAllocatorDefault, currentLocaleID.get());
        if (!locale)
            return nullptr;
    }

    CFRange entireRange = CFRangeMake(0, CFStringGetLength(str));    

    static CFStringTokenizerRef tokenizer = nullptr;
    if (!tokenizer)
        tokenizer = CFStringTokenizerCreate(kCFAllocatorDefault, str, entireRange, kCFStringTokenizerUnitWordBoundary, locale);
    else
        CFStringTokenizerSetString(tokenizer, str, entireRange);
    return tokenizer;
}

// Simple case: A word is a stream of characters delimited by a special set of word-delimiting characters.
static void findSimpleWordBoundary(StringView text, int position, int* start, int* end)
{
    ASSERT(position >= 0);
    ASSERT(static_cast<unsigned>(position) < text.length());

    unsigned startPos = position;
    while (startPos > 0) {
        int i = startPos;
        UChar32 characterBeforeStartPos;
        U16_PREV(text, 0, i, characterBeforeStartPos);
        if (isWordDelimitingCharacter(characterBeforeStartPos)) {
            ASSERT(i >= 0);
            if (!i)
                break;

            if (!isAmbiguousBoundaryCharacter(characterBeforeStartPos))
                break;

            UChar32 characterBeforeBeforeStartPos;
            U16_PREV(text, 0, i, characterBeforeBeforeStartPos);
            if (isWordDelimitingCharacter(characterBeforeBeforeStartPos))
                break;
        }
        U16_BACK_1(text, 0, startPos);
    }
    
    unsigned endPos = position;
    while (endPos < text.length()) {
        UChar32 character;
        U16_GET(text, 0, endPos, text.length(), character);
        if (isWordDelimitingCharacter(character)) {
            unsigned i = endPos;
            U16_FWD_1(text, i, text.length());
            ASSERT(i <= text.length());
            if (i == text.length())
                break;
            UChar32 characterAfterEndPos;
            U16_NEXT(text, i, text.length(), characterAfterEndPos);
            if (!isAmbiguousBoundaryCharacter(character))
                break;
            if (isWordDelimitingCharacter(characterAfterEndPos))
                break;
        }
        U16_FWD_1(text, endPos, text.length());
    }

    // The text may consist of all delimiter characters (e.g. "++++++++" or a series of emoji), and returning an empty range
    // makes no sense (and doesn't match findComplexWordBoundary() behavior).
    if (startPos == endPos && endPos < text.length()) {
        UChar32 character;
        U16_GET(text, 0, endPos, text.length(), character);
        if (isSymbolCharacter(character))
            U16_FWD_1(text, endPos, text.length());
    }

    *start = startPos;
    *end = endPos;
}

// Complex case: use CFStringTokenizer to find word boundary.
static void findComplexWordBoundary(StringView text, int position, int* start, int* end)
{
    RetainPtr<CFStringRef> charString = text.createCFStringWithoutCopying();

    CFStringTokenizerRef tokenizer = tokenizerForString(charString.get());
    if (!tokenizer) {
        // Error creating tokenizer, so just use simple function.
        findSimpleWordBoundary(text, position, start, end);
        return;
    }

    CFStringTokenizerTokenType  token = CFStringTokenizerGoToTokenAtIndex(tokenizer, position);
    if (token == kCFStringTokenizerTokenNone) {
        // No token found: select entire block.
        // NB: I never hit this section in all my testing.
        *start = 0;
        *end = text.length();
        return;
    }

    CFRange result = CFStringTokenizerGetCurrentTokenRange(tokenizer);
    *start = result.location;
    *end = result.location + result.length;
}

#endif

void findWordBoundary(StringView text, int position, int* start, int* end)
{
#if USE(APPKIT)
    NSAttributedString *attributedString = [[NSAttributedString alloc] initWithString:text.createNSStringWithoutCopying().get()];
    NSRange range = [attributedString doubleClickAtIndex:std::min<unsigned>(position, text.length() - 1)];
    [attributedString release];
    *start = range.location;
    *end = range.location + range.length;
#else
    unsigned pos = position;
    if (pos == text.length() && pos)
        --pos;

    // For complex text (Thai, Japanese, Chinese), visible_units will pass the text in as a 
    // single contiguous run of characters, providing as much context as is possible.
    // We only need one character to determine if the text is complex.
    UChar32 ch;
    unsigned i = pos;
    U16_NEXT(text, i, text.length(), ch);
    bool isComplex = requiresContextForWordBoundary(ch);

    // FIXME: This check improves our word boundary behavior, but doesn't actually go far enough.
    // See <rdar://problem/8853951> Take complex word boundary finding path when necessary
    if (!isComplex) {
        // Check again for complex text, at the start of the run.
        i = 0;
        U16_NEXT(text, i, text.length(), ch);
        isComplex = requiresContextForWordBoundary(ch);
    }

    if (isComplex)
        findComplexWordBoundary(text, position, start, end);
    else
        findSimpleWordBoundary(text, position, start, end);

#define LOG_WORD_BREAK 0
#if LOG_WORD_BREAK
    auto uniString = text.createCFStringWithoutCopying();
    auto foundWord = text.substring(*start, *end - *start).createCFStringWithoutCopying();
    NSLog(@"%s_BREAK '%@' (%d,%d) in '%@' (%p) at %d, length=%d", isComplex ? "COMPLEX" : "SIMPLE", foundWord.get(), *start, *end, uniString.get(), uniString.get(), position, text.length());
#endif
    
#endif
}

void findEndWordBoundary(StringView text, int position, int* end)
{
    int start;
    findWordBoundary(text, position, &start, end);
}

int findNextWordFromIndex(StringView text, int position, bool forward)
{   
#if USE(APPKIT)
    NSAttributedString *attributedString = [[NSAttributedString alloc] initWithString:text.createNSStringWithoutCopying().get()];
    int result = [attributedString nextWordFromIndex:position forward:forward];
    [attributedString release];
    return result;
#else
    // This very likely won't behave exactly like the non-iPhone version, but it works
    // for the contexts in which it is used on iPhone, and in the future will be
    // tuned to improve the iPhone-specific behavior for the keyboard and text editing.
    int pos = position;
    TextBreakIterator* boundary = wordBreakIterator(text);
    if (boundary) {
        if (forward) {
            do {
                pos = textBreakFollowing(boundary, pos);
                if (pos == UBRK_DONE)
                    pos = text.length();
            } while (static_cast<unsigned>(pos) < text.length() && (pos == 0 || !isSkipCharacter(text[pos - 1])) && isSkipCharacter(text[pos]));
        }
        else {
            do {
                pos = textBreakPreceding(boundary, pos);
                if (pos == UBRK_DONE)
                    pos = 0;
            } while (pos > 0 && isSkipCharacter(text[pos]) && !isWhitespaceCharacter(text[pos - 1]));
        }
    }
    return pos;
#endif
}

}
