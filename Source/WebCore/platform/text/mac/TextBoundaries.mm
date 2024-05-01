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

#import <CoreFoundation/CFStringTokenizer.h>
#import <Foundation/Foundation.h>
#import <unicode/ubrk.h>
#import <unicode/uchar.h>
#import <unicode/ustring.h>
#import <unicode/utypes.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/StringView.h>
#import <wtf/text/TextBreakIterator.h>
#import <wtf/text/TextBreakIteratorInternalICU.h>
#import <wtf/unicode/CharacterNames.h>

@interface NSAttributedString (WebKit)
- (NSRange)doubleClickAtIndex:(NSUInteger)location;
- (NSUInteger)nextWordFromIndex:(NSUInteger)position forward:(BOOL)forward;
@end

namespace WebCore {

void findWordBoundary(StringView text, int position, int* start, int* end)
{

    auto attributedString = adoptNS([[NSAttributedString alloc] initWithString:text.createNSStringWithoutCopying().get()]);
    NSRange range = [attributedString doubleClickAtIndex:std::min<unsigned>(position, text.length() - 1)];
    *start = range.location;
    *end = range.location + range.length;
}

void findEndWordBoundary(StringView text, int position, int* end)
{
    int start;
    findWordBoundary(text, position, &start, end);
}

int findNextWordFromIndex(StringView text, int position, bool forward)
{   

    String textWithoutUnpairedSurrogates;
    if (hasUnpairedSurrogate(text)) {
        textWithoutUnpairedSurrogates = replaceUnpairedSurrogatesWithReplacementCharacter(text.toStringWithoutCopying());
        text = textWithoutUnpairedSurrogates;
    }
    auto attributedString = adoptNS([[NSAttributedString alloc] initWithString:text.createNSStringWithoutCopying().get()]);
    return [attributedString nextWordFromIndex:position forward:forward];
}

}
