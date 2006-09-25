/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#import <WebCore/DOMCore.h>
#import <WebCore/DOMDocument.h>
#import <WebCore/DOMObject.h>
#import <WebCore/DOMRangeException.h>

// DOM Range comparison codes
enum {
    DOM_START_TO_START                = 0,
    DOM_START_TO_END                  = 1,
    DOM_END_TO_END                    = 2,
    DOM_END_TO_START                  = 3
};

@interface DOMRange : DOMObject
#ifndef BUILDING_ON_TIGER
@property(readonly) DOMNode *startContainer;
@property(readonly) int startOffset;
@property(readonly) DOMNode *endContainer;
@property(readonly) int endOffset;
@property(readonly) BOOL collapsed;
@property(readonly) DOMNode *commonAncestorContainer;
#else
- (DOMNode *)startContainer;
- (int)startOffset;
- (DOMNode *)endContainer;
- (int)endOffset;
- (BOOL)collapsed;
- (DOMNode *)commonAncestorContainer;
#endif
- (void)setStart:(DOMNode *)refNode offset:(int)offset;
- (void)setEnd:(DOMNode *)refNode offset:(int)offset;
- (void)setStartBefore:(DOMNode *)refNode;
- (void)setStartAfter:(DOMNode *)refNode;
- (void)setEndBefore:(DOMNode *)refNode;
- (void)setEndAfter:(DOMNode *)refNode;
- (void)collapse:(BOOL)toStart;
- (void)selectNode:(DOMNode *)refNode;
- (void)selectNodeContents:(DOMNode *)refNode;
- (short)compareBoundaryPoints:(unsigned short)how sourceRange:(DOMRange *)sourceRange;
- (void)deleteContents;
- (DOMDocumentFragment *)extractContents;
- (DOMDocumentFragment *)cloneContents;
- (void)insertNode:(DOMNode *)newNode;
- (void)surroundContents:(DOMNode *)newParent;
- (DOMRange *)cloneRange;
- (NSString *)toString;
- (void)detach;
@end

@interface DOMRange (DOMRangeDeprecated)
#ifndef BUILDING_ON_TIGER
- (void)setStart:(DOMNode *)refNode :(int)offset DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
- (void)setEnd:(DOMNode *)refNode :(int)offset DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
- (short)compareBoundaryPoints:(unsigned short)how :(DOMRange *)sourceRange DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;
#else
- (void)setStart:(DOMNode *)refNode :(int)offset;
- (void)setEnd:(DOMNode *)refNode :(int)offset;
- (short)compareBoundaryPoints:(unsigned short)how :(DOMRange *)sourceRange;
#endif
@end
