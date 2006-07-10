/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

extern NSString * const DOMXPathException;

enum DOMXPathExceptionCode {
    DOM_INVALID_EXPRESSION_ERR = 51,
    DOM_TYPE_ERR = 52
};

@protocol DOMXPathNSResolver <NSObject>
- (NSString *)lookupNamespaceURI:(NSString *)prefix;
@end

enum {
    // XPath result types
    DOM_ANY_TYPE                       = 0,
    DOM_NUMBER_TYPE                    = 1,
    DOM_STRING_TYPE                    = 2,
    DOM_BOOLEAN_TYPE                   = 3,
    DOM_UNORDERED_NODE_ITERATOR_TYPE   = 4,
    DOM_ORDERED_NODE_ITERATOR_TYPE     = 5,
    DOM_UNORDERED_NODE_SNAPSHOT_TYPE   = 6,
    DOM_ORDERED_NODE_SNAPSHOT_TYPE     = 7,
    DOM_ANY_UNORDERED_NODE_TYPE        = 8,
    DOM_FIRST_ORDERED_NODE_TYPE        = 9,
};

@interface DOMXPathResult : DOMObject
- (unsigned short)resultType;
- (double)numberValue;
- (NSString *)stringValue;
- (BOOL)booleanValue;
- (DOMNode *)singleNodeValue;
- (BOOL)invalidIteratorState;
- (unsigned)snapshotLength;
- (DOMNode *)iterateNext;
- (DOMNode *)snapshotItem:(unsigned)index;
@end

@interface DOMXPathExpression : DOMObject
- (DOMXPathResult *)evaluate:(DOMNode *)contextNode :(unsigned short)type :(DOMXPathResult *)result;
@end

@interface DOMDocument (DOMDocumentXPath)
- (DOMXPathExpression *)createExpression:(NSString *)expression :(id <DOMXPathNSResolver>)resolver;
- (id <DOMXPathNSResolver>)createNSResolver:(DOMNode *)nodeResolver;
- (DOMXPathResult *)evaluate:(NSString *)expression :(DOMNode *)contextNode :(id <DOMXPathNSResolver>)resolver :(unsigned short)type :(DOMXPathResult *)result;
@end
