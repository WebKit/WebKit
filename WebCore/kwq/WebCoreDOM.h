/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import <Foundation/Foundation.h>

namespace DOM {
    class DOMImplementationImpl;
    class NodeImpl;
    class NodeListImpl;
    class NamedNodeMapImpl;
}

@protocol WebDOMAttr;
@protocol WebDOMCharacterData;
@protocol WebDOMComment;
@protocol WebDOMCDATASection;
@protocol WebDOMDocument;
@protocol WebDOMDocumentFragment;
@protocol WebDOMDocumentType;
@protocol WebDOMElement;
@protocol WebDOMEntityReference;
@protocol WebDOMImplementation;
@protocol WebDOMNamedNodeMap;
@protocol WebDOMNode;
@protocol WebDOMNodeList;
@protocol WebDOMProcessingInstruction;
@protocol WebDOMText;

@interface WebCoreDOMNode : NSObject <WebDOMNode>
{
    DOM::NodeImpl *impl;
}
@end

@interface WebCoreDOMNamedNodeMap : NSObject <WebDOMNamedNodeMap>
{
    DOM::NamedNodeMapImpl *impl;
}
@end

@interface WebCoreDOMNodeList : NSObject <WebDOMNodeList>
{
    DOM::NodeListImpl *impl;
}
@end

@interface WebCoreDOMImplementation : NSObject <WebDOMImplementation>
{
    DOM::DOMImplementationImpl *impl;
}
@end

@interface WebCoreDOMDocumentType : WebCoreDOMNode <WebDOMDocumentType>
@end

@interface WebCoreDOMDocument : WebCoreDOMNode <WebDOMDocument>
@end

@interface WebCoreDOMEntityReference : WebCoreDOMNode <WebDOMEntityReference>
@end

@interface WebCoreDOMElement : WebCoreDOMNode <WebDOMElement>
@end

@interface WebCoreDOMAttr : WebCoreDOMNode <WebDOMAttr> 
@end

@interface WebCoreDOMDocumentFragment : WebCoreDOMNode <WebDOMDocumentFragment>
@end

@interface WebCoreDOMCharacterData : WebCoreDOMNode <WebDOMCharacterData>
@end

@interface WebCoreDOMText : WebCoreDOMCharacterData <WebDOMText>
@end

@interface WebCoreDOMComment : WebCoreDOMCharacterData <WebDOMComment>
@end

@interface WebCoreDOMCDATASection : WebCoreDOMText <WebDOMCDATASection>
@end

@interface WebCoreDOMProcessingInstruction : WebCoreDOMNode <WebDOMProcessingInstruction>
@end


