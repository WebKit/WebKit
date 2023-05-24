/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import <wtf/text/cocoa/ContextualizedNSString.h>

#import <algorithm>
#import <wtf/text/StringView.h>

@implementation WTFContextualizedNSString {
    StringView context;
    StringView contents;
}

- (instancetype)initWithContext:(StringView)context contents:(StringView)contents
{
    if (self = [super init]) {
        self->context = context;
        self->contents = contents;
    }
    return self;
}

- (NSUInteger)length
{
    return context.length() + contents.length();
}

- (unichar)characterAtIndex:(NSUInteger)index
{
    if (index < context.length())
        return context[index];
    return contents[index - context.length()];
}

- (void)getCharacters:(unichar *)buffer range:(NSRange)range
{
    auto contextLow = std::clamp(static_cast<unsigned>(range.location), 0U, context.length());
    auto contextHigh = std::clamp(static_cast<unsigned>(range.location + range.length), 0U, context.length());
    auto contextSubstring = context.substring(contextLow, contextHigh - contextLow);
    auto contentsLow = std::clamp(static_cast<unsigned>(range.location), context.length(), context.length() + contents.length());
    auto contentsHigh = std::clamp(static_cast<unsigned>(range.location + range.length), context.length(), context.length() + contents.length());
    auto contentsSubstring = contents.substring(contentsLow - context.length(), contentsHigh - contentsLow);
    static_assert(std::is_same_v<std::make_unsigned_t<unichar>, std::make_unsigned_t<UChar>>);
    contextSubstring.getCharacters(reinterpret_cast<UChar*>(buffer));
    contentsSubstring.getCharacters(reinterpret_cast<UChar*>(buffer) + contextSubstring.length());
}

@end
