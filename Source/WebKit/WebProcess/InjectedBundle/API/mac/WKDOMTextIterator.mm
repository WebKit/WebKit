/*
 * Copyright (C) 2012, 2014 Apple Inc. All rights reserved.
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
#import "WKDOMTextIterator.h"

#import "WKDOMInternals.h"
#import "WKDOMRange.h"
#import <WebCore/TextIterator.h>

@interface WKDOMTextIterator () {
@private
    std::unique_ptr<WebCore::TextIterator> _textIterator;
    Vector<unichar> _upconvertedText;
}
@end

@implementation WKDOMTextIterator

- (id)initWithRange:(WKDOMRange *)range
{
    self = [super init];
    if (!self)
        return nil;

    _textIterator = std::make_unique<WebCore::TextIterator>(WebKit::toWebCoreRange(range));
    return self;
}

- (void)advance
{
    _textIterator->advance();
    _upconvertedText.shrink(0);
}

- (BOOL)atEnd
{
    return _textIterator->atEnd();
}

- (WKDOMRange *)currentRange
{
    return WebKit::toWKDOMRange(_textIterator->range().ptr());
}

// FIXME: Consider deprecating this method and creating one that does not require copying 8-bit characters.
- (const unichar*)currentTextPointer
{
    StringView text = _textIterator->text();
    unsigned length = text.length();
    if (!length)
        return nullptr;
    if (!text.is8Bit())
        return text.characters16();
    if (_upconvertedText.isEmpty())
        _upconvertedText.appendRange(text.characters8(), text.characters8() + length);
    ASSERT(_upconvertedText.size() == text.length());
    return _upconvertedText.data();
}

- (NSUInteger)currentTextLength
{
    return _textIterator->text().length();
}

@end
