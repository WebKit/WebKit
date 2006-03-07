/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#import <Cocoa/Cocoa.h>
#import <unicode/ubrk.h>

namespace WebCore {

void findWordBoundary(const QChar* chars, int len, int position, int* start, int* end)
{
    NSString* string = [[NSString alloc] initWithCharactersNoCopy:const_cast<unichar*>(reinterpret_cast<const unichar*>(chars))
        length:len freeWhenDone:NO];
    NSAttributedString* attr = [[NSAttributedString alloc] initWithString:string];
    NSRange range = [attr doubleClickAtIndex:(position >= len) ? len - 1 : position];
    [attr release];
    [string release];
    *start = range.location;
    *end = range.location + range.length;
}

int findNextWordFromIndex(const QChar* chars, int len, int position, bool forward)
{   
    NSString* string = [[NSString alloc] initWithCharactersNoCopy:const_cast<unichar*>(reinterpret_cast<const unichar*>(chars))
        length:len freeWhenDone:NO];
    NSAttributedString* attr = [[NSAttributedString alloc] initWithString:string];
    int result = [attr nextWordFromIndex:position forward:forward];
    [attr release];
    [string release];
    return result;
}

// This code was swiped from the CarbonCore UnicodeUtilities. One change from that is to use the empty
// string instead of the "old locale model" as the ultimate fallback. This change is per the UnicodeUtilities
// engineer.
//
// NOTE: this abviously could be fairly expensive to do.  If it turns out to be a bottleneck, it might
// help to instead put a call in the iteratory initializer to set the current text break locale.  Unfortunately,
// we can not cache it across calls to our API since the result can change without our knowing (AFAIK
// there are no notifiers for AppleTextBreakLocale and/or AppleLanguages changes).
static char* currentTextBreakLocaleID()
{
    const int localeStringLength = 32;
    static char localeStringBuffer[localeStringLength] = { 0 };
    char* localeString = &localeStringBuffer[0];
    
    // Empty string means "root locale", which what we use if we can't use a pref.
    
    // We get the parts string from AppleTextBreakLocale pref.
    // If that fails then look for the first language in the AppleLanguages pref.
    CFStringRef prefLocaleStr = (CFStringRef)CFPreferencesCopyValue(CFSTR("AppleTextBreakLocale"),
        kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    if (!prefLocaleStr) {
        CFArrayRef appleLangArr = (CFArrayRef)CFPreferencesCopyValue(CFSTR("AppleLanguages"),
            kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
        if (appleLangArr)  {
            // Take the topmost language. Retain so that we can blindly release later.                                                                                                   
            prefLocaleStr = (CFStringRef)CFArrayGetValueAtIndex(appleLangArr, 0);
            if (prefLocaleStr)
                CFRetain(prefLocaleStr); 
            CFRelease(appleLangArr);
        }
    }
    
    if (prefLocaleStr) {
        // Canonicalize pref string in case it is not in the canonical format. This call is only available on Tiger and newer.
        CFStringRef canonLocaleCFStr = CFLocaleCreateCanonicalLanguageIdentifierFromString(kCFAllocatorDefault, prefLocaleStr);
        if (canonLocaleCFStr) {
            CFStringGetCString(canonLocaleCFStr, localeString, localeStringLength, kCFStringEncodingASCII);
            CFRelease(canonLocaleCFStr);
        }
        CFRelease(prefLocaleStr);
    }
   
    return localeString;
}

void findSentenceBoundary(const QChar* chars, int len, int position, int* start, int* end)
{
    int startPos = 0;
    int endPos = 0;

    UErrorCode status = U_ZERO_ERROR;
    UBreakIterator* boundary = ubrk_open(UBRK_SENTENCE, currentTextBreakLocaleID(),
        const_cast<unichar*>(reinterpret_cast<const unichar*>(chars)), len, &status);
    if (boundary && U_SUCCESS(status)) {
        startPos = ubrk_preceding(boundary, position);
        if (startPos == UBRK_DONE) {
            startPos = 0;
        } 
        endPos = ubrk_following(boundary, startPos);
        if (endPos == UBRK_DONE)
            endPos = len;

        ubrk_close(boundary);
    }
    
    *start = startPos;
    *end = endPos;
}

int findNextSentenceFromIndex(const QChar* chars, int len, int position, bool forward)
{
    int pos = 0;
    
    UErrorCode status = U_ZERO_ERROR;
    UBreakIterator* boundary = ubrk_open(UBRK_SENTENCE, currentTextBreakLocaleID(),
        const_cast<unichar*>(reinterpret_cast<const unichar*>(chars)), len, &status);
    if (boundary && U_SUCCESS(status)) {
        if (forward) {
            pos = ubrk_following(boundary, position);
            if (pos == UBRK_DONE)
                pos = len;
        } else {
            pos = ubrk_preceding(boundary, position);
            if (pos == UBRK_DONE)
                pos = 0;
        }
        ubrk_close(boundary);
    }
        
    return pos;
}

}
