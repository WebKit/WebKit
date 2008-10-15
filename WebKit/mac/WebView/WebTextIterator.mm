/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#import <WebCore/TextIterator.h>
#import <wtf/Vector.h>

using namespace WebCore;

@interface WebTextIteratorPrivate : NSObject
{
@public
    TextIterator* m_textIterator;
}
@end

@implementation WebTextIteratorPrivate

- (void)dealloc
{
    delete m_textIterator;
    [super dealloc];
}

- (void)finalize
{
    delete m_textIterator;
    [super finalize];
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
    _private->m_textIterator = new TextIterator([range _range], true, false);
    return self;
}

- (void)advance
{
    ASSERT(_private->m_textIterator);
    
    if (_private->m_textIterator->atEnd())
        return;
    
    _private->m_textIterator->advance();
}

- (DOMNode *)currentNode
{
    ASSERT(_private->m_textIterator);
    
    return [DOMNode _wrapNode:_private->m_textIterator->node()];
}

- (NSString *)currentText
{
    ASSERT(_private->m_textIterator);
    
    return [NSString stringWithCharacters:_private->m_textIterator->characters() length:_private->m_textIterator->length()];
}

- (BOOL)atEnd
{
    ASSERT(_private->m_textIterator);
    
    return _private->m_textIterator->atEnd();
}

@end
