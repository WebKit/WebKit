/*
 * Copyright (C) 2008-2020 Apple Inc. All Rights Reserved.
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

#import "WebTextIterator.h"

#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "WebTypesInternal.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/Range.h>
#import <WebCore/TextIterator.h>
#import <wtf/MainThread.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>

@interface WebTextIteratorPrivate : NSObject {
@public
    std::unique_ptr<WebCore::TextIterator> _textIterator;
    Vector<unichar> _upconvertedText;
}
@end

@implementation WebTextIteratorPrivate

+ (void)initialize
{
#if !PLATFORM(IOS_FAMILY)
    JSC::initializeThreading();
    WTF::initializeMainThread();
#endif
}

@end

@implementation WebTextIterator

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (id)initWithRange:(DOMRange *)range
{
    self = [super init];
    if (!self)
        return self;
    
    _private = [[WebTextIteratorPrivate alloc] init];
    if (!range)
        return self;

    _private->_textIterator = makeUnique<WebCore::TextIterator>(*core(range));
    return self;
}

- (void)advance
{
    if (_private->_textIterator)
        _private->_textIterator->advance();
    _private->_upconvertedText.shrink(0);
}

- (BOOL)atEnd
{
    return _private->_textIterator && _private->_textIterator->atEnd();
}

- (DOMRange *)currentRange
{
    if (!_private->_textIterator)
        return nil;
    auto& textIterator = *_private->_textIterator;
    if (textIterator.atEnd())
        return nil;
    return kit(createLiveRange(textIterator.range()).ptr());
}

// FIXME: Consider deprecating this method and creating one that does not require copying 8-bit characters.
- (const unichar*)currentTextPointer
{
    if (!_private->_textIterator)
        return nullptr;
    StringView text = _private->_textIterator->text();
    unsigned length = text.length();
    if (!length)
        return nullptr;
    if (!text.is8Bit())
        return reinterpret_cast<const unichar*>(text.characters16());
    if (_private->_upconvertedText.isEmpty())
        _private->_upconvertedText.appendRange(text.characters8(), text.characters8() + length);
    ASSERT(_private->_upconvertedText.size() == text.length());
    return _private->_upconvertedText.data();
}

- (NSUInteger)currentTextLength
{
    return _private->_textIterator ? _private->_textIterator->text().length() : 0;
}

@end

@implementation WebTextIterator (WebTextIteratorDeprecated)

- (DOMNode *)currentNode
{
    return _private->_textIterator ? kit(_private->_textIterator->node()) : nil;
}

- (NSString *)currentText
{
    return _private->_textIterator ? _private->_textIterator->text().createNSString().autorelease() : @"";
}

@end
