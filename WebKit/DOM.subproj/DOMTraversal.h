/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/DOMCore.h>

@class DOMNodeFilter;

enum {
    // Constants returned by acceptNode
    DOM_FILTER_ACCEPT                  = 1,
    DOM_FILTER_REJECT                  = 2,
    DOM_FILTER_SKIP                    = 3,
};

enum {
    // Constants for whatToShow
    DOM_SHOW_ALL                       = 0xFFFFFFFF,
    DOM_SHOW_ELEMENT                   = 0x00000001,
    DOM_SHOW_ATTRIBUTE                 = 0x00000002,
    DOM_SHOW_TEXT                      = 0x00000004,
    DOM_SHOW_CDATA_SECTION             = 0x00000008,
    DOM_SHOW_ENTITY_REFERENCE          = 0x00000010,
    DOM_SHOW_ENTITY                    = 0x00000020,
    DOM_SHOW_PROCESSING_INSTRUCTION    = 0x00000040,
    DOM_SHOW_COMMENT                   = 0x00000080,
    DOM_SHOW_DOCUMENT                  = 0x00000100,
    DOM_SHOW_DOCUMENT_TYPE             = 0x00000200,
    DOM_SHOW_DOCUMENT_FRAGMENT         = 0x00000400,
    DOM_SHOW_NOTATION                  = 0x00000800,
};

@protocol DOMNodeFilter <NSObject>
- (short)acceptNode:(DOMNode *)n;
@end

@interface DOMNodeIterator : DOMObject
{
    id <DOMNodeFilter> m_filter;
}
- (DOMNode *)root;
- (unsigned long)whatToShow;
- (id <DOMNodeFilter>)filter;
- (BOOL)expandEntityReferences;
- (DOMNode *)nextNode;
- (DOMNode *)previousNode;
- (void)detach;
@end

@interface DOMTreeWalker : DOMObject
{
    id <DOMNodeFilter> m_filter;
}
- (DOMNode *)root;
- (unsigned long)whatToShow;
- (id <DOMNodeFilter>)filter;
- (BOOL)expandEntityReferences;
- (DOMNode *)currentNode;
- (void)setCurrentNode:(DOMNode *)currentNode;
- (DOMNode *)parentNode;
- (DOMNode *)firstChild;
- (DOMNode *)lastChild;
- (DOMNode *)previousSibling;
- (DOMNode *)nextSibling;
- (DOMNode *)previousNode;
- (DOMNode *)nextNode;
@end

@interface DOMDocument (DOMDocumentTraversal)
- (DOMNodeIterator *)createNodeIterator:(DOMNode *)root :(unsigned long)whatToShow :(id <DOMNodeFilter>)filter :(BOOL)expandEntityReferences;
- (DOMTreeWalker *)createTreeWalker:(DOMNode *)root :(unsigned long)whatToShow :(id <DOMNodeFilter>)filter :(BOOL)expandEntityReferences;
@end
