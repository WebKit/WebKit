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

#import "dom_element.h"
#import "dom_node.h"
#import "dom_string.h"
#import "dom_text.h"
#import "dom_xml.h"
#import "dom_textimpl.h"
#import "dom_xmlimpl.h"
#import "dom2_rangeimpl.h"
#import "dom2_eventsimpl.h"
#import "xml_tokenizer.h"
#import "dom_docimpl.h"
#import "dom_nodeimpl.h"

#import "WebCoreDOM.h"

extern NSString *domStringToNSString(DOM::DOMString &aString);
extern DOM::DOMString NSStringToDOMString(NSString *aString);

@interface WebCoreDOMDocumentType (CorePrivate)
+ (WebCoreDOMDocumentType *)documentTypeWithImpl:(DOM::DocumentTypeImpl *)impl;
- (DOM::DocumentTypeImpl *)documentTypeImpl;
@end

@interface WebCoreDOMImplementation (CorePrivate)
+ (WebCoreDOMImplementation *)implementationWithImpl:(DOM::DOMImplementationImpl *)impl;
- initWithImpl:(DOM::DOMImplementationImpl *)coreImpl;
- (DOM::DOMImplementationImpl *)DOMImplementationImpl;
@end

@interface WebCoreDOMDocument (CorePrivate)
+ (WebCoreDOMDocument *)documentWithImpl:(DOM::DocumentImpl *)impl;
- (DOM::DocumentImpl *)documentImpl;
@end

@interface WebCoreDOMNamedNodeMap (CorePrivate)
+ (WebCoreDOMNamedNodeMap *)namedNodeMapWithImpl:(DOM::NamedNodeMapImpl *)impl;
- initWithImpl:(DOM::NamedNodeMapImpl *)coreImpl;
- (DOM::NamedNodeMapImpl *)impl;
@end

@interface WebCoreDOMElement (CorePrivate)
+ (WebCoreDOMElement *)elementWithImpl:(DOM::ElementImpl *)impl;
- (DOM::ElementImpl *)elementImpl;
@end

@interface WebCoreDOMAttr (CorePrivate)
+ (WebCoreDOMAttr *)attrWithImpl:(DOM::AttrImpl *)impl;
- (DOM::AttrImpl *)attrImpl;
@end

@interface WebCoreDOMDocumentFragment (CorePrivate)
+ (WebCoreDOMDocumentFragment *)documentFragmentWithImpl:(DOM::DocumentFragmentImpl *)impl;
- (DOM::DocumentFragmentImpl *)documentFragmentImpl;
@end

@interface WebCoreDOMCharacterData (CorePrivate)
+ (WebCoreDOMCharacterData *)characterDataWithImpl:(DOM::CharacterDataImpl *)impl;
- initWithImpl:(DOM::CharacterDataImpl *)coreImpl;
- (DOM::CharacterDataImpl *)impl;
@end

@interface WebCoreDOMText (CorePrivate)
+ (WebCoreDOMText *)textWithImpl:(DOM::TextImpl *)impl;
- (DOM::TextImpl *)textImpl;
@end

@interface WebCoreDOMComment (CorePrivate)
+ (WebCoreDOMComment *)commentWithImpl:(DOM::CommentImpl *)impl;
- (DOM::CommentImpl *)commentImpl;
@end

@interface WebCoreDOMCDATASection (CorePrivate)
+ (WebCoreDOMCDATASection *)CDATASectionWithImpl:(DOM::CDATASectionImpl *)impl;
- (DOM::CDATASectionImpl *)CDATASectionImpl;
@end

@interface WebCoreDOMProcessingInstruction (CorePrivate)
+ (WebCoreDOMProcessingInstruction *)processingInstructionWithImpl:(DOM::ProcessingInstructionImpl *)impl;
- (DOM::ProcessingInstructionImpl *)processingInstructionImpl;
@end

@interface WebCoreDOMEntityReference (CorePrivate)
+ (WebCoreDOMEntityReference *)entityReferenceWithImpl:(DOM::EntityReferenceImpl *)impl;
- (DOM::EntityReferenceImpl *)entityReferenceImpl;
@end

@interface WebCoreDOMNode (CorePrivate)
+ (WebCoreDOMNode *)nodeWithImpl:(DOM::NodeImpl *)impl;
- initWithImpl:(DOM::NodeImpl *)coreImpl;
- (DOM::NodeImpl *)impl;
@end

@interface WebCoreDOMNodeList (CorePrivate)
+ (WebCoreDOMNodeList *)nodeListWithImpl:(DOM::NodeListImpl *)impl;
- initWithImpl:(DOM::NodeListImpl *)coreImpl;
- (DOM::NodeListImpl *)impl;
@end

