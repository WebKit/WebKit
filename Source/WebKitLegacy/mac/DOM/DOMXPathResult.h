/*
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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

#import <WebKitLegacy/DOMObject.h>

@class DOMNode;
@class NSString;

enum {
    DOM_ANY_TYPE = 0,
    DOM_NUMBER_TYPE = 1,
    DOM_STRING_TYPE = 2,
    DOM_BOOLEAN_TYPE = 3,
    DOM_UNORDERED_NODE_ITERATOR_TYPE = 4,
    DOM_ORDERED_NODE_ITERATOR_TYPE = 5,
    DOM_UNORDERED_NODE_SNAPSHOT_TYPE = 6,
    DOM_ORDERED_NODE_SNAPSHOT_TYPE = 7,
    DOM_ANY_UNORDERED_NODE_TYPE = 8,
    DOM_FIRST_ORDERED_NODE_TYPE = 9
} WEBKIT_ENUM_DEPRECATED_MAC(10_5, 10_14);

WEBKIT_CLASS_DEPRECATED_MAC(10_5, 10_14)
@interface DOMXPathResult : DOMObject
@property (readonly) unsigned short resultType;
@property (readonly) double numberValue;
@property (readonly, copy) NSString *stringValue;
@property (readonly) BOOL booleanValue;
@property (readonly, strong) DOMNode *singleNodeValue;
@property (readonly) BOOL invalidIteratorState;
@property (readonly) unsigned snapshotLength;

- (DOMNode *)iterateNext;
- (DOMNode *)snapshotItem:(unsigned)index;
@end
