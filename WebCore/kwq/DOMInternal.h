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

#import "DOM.h"

namespace DOM {
    class AttrImpl;
    class CDATASectionImpl;
    class CharacterDataImpl;
    class CommentImpl;
    class DocumentFragmentImpl;
    class DocumentTypeImpl;
    class DocumentImpl;
    class DOMImplementationImpl;
    class ElementImpl;
    class EntityImpl;
    class EntityReferenceImpl;
    class NamedNodeMapImpl;
    class NodeImpl;
    class NodeListImpl;
    class NotationImpl;
    class ProcessingInstructionImpl;
    class RangeImpl;
    class TextImpl;
}

@interface DOMNode (SubclassResponsibility)
- (DOM::NodeImpl *)nodeImpl;
- (Class)classForDOMDocument;
@end

@interface WebCoreDOMNode : DOMNode
{
	DOM::NodeImpl *m_impl;
}
+ (DOMNode *)nodeWithImpl:(DOM::NodeImpl *)impl;
- (id)initWithNodeImpl:(DOM::NodeImpl *)impl;
@end

@interface DOMNamedNodeMap (SubclassResponsibility)
- (DOM::NamedNodeMapImpl *)namedNodeMapImpl;
- (Class)classForDOMNode;
@end

@interface WebCoreDOMNamedNodeMap : DOMNamedNodeMap
{
	DOM::NamedNodeMapImpl *m_impl;
}
+ (DOMNamedNodeMap *)namedNodeMapWithImpl:(DOM::NamedNodeMapImpl *)impl;
- (id)initWithNamedNodeMapImpl:(DOM::NamedNodeMapImpl *)impl;
@end

@interface DOMNodeList (SubclassResponsibility)
- (DOM::NodeListImpl *)nodeListImpl;
@end

@interface WebCoreDOMNodeList : DOMNodeList
{
	DOM::NodeListImpl *m_impl;
}
+ (DOMNodeList *)nodeListWithImpl:(DOM::NodeListImpl *)impl;
- (id)initWithNodeListImpl:(DOM::NodeListImpl *)impl;
@end

@interface DOMImplementation (SubclassResponsibility)
- (DOM::DOMImplementationImpl *)DOMImplementationImpl;
- (Class)classForDOMDocumentType;
- (Class)classForDOMDocument;
@end

@interface WebCoreDOMImplementation : DOMImplementation
{
	DOM::DOMImplementationImpl *m_impl;
}
+ (DOMImplementation *)DOMImplementationWithImpl:(DOM::DOMImplementationImpl *)impl;
- (id)initWithDOMImplementationImpl:(DOM::DOMImplementationImpl *)impl;
@end

@interface DOMDocumentFragment (SubclassResponsibility)
- (DOM::DocumentFragmentImpl *)documentFragmentImpl;
@end

@interface WebCoreDOMDocumentFragment : DOMDocumentFragment
{
	DOM::DocumentFragmentImpl *m_impl;
}
+ (DOMDocumentFragment *)documentFragmentWithImpl:(DOM::DocumentFragmentImpl *)impl;
- (id)initWithDocumentFragmentImpl:(DOM::DocumentFragmentImpl *)impl;
@end

@interface DOMDocument (SubclassResponsibility)
- (DOM::DocumentImpl *)documentImpl;
- (Class)classForDOMAttr;
- (Class)classForDOMCDATASection;
- (Class)classForDOMComment;
- (Class)classForDOMDocumentFragment;
- (Class)classForDOMDocumentType;
- (Class)classForDOMElement;
- (Class)classForDOMEntityReference;
- (Class)classForDOMImplementation;
- (Class)classForDOMNode;
- (Class)classForDOMNodeList;
- (Class)classForDOMProcessingInstruction;
- (Class)classForDOMText;
@end

@interface WebCoreDOMDocument : DOMDocument
{
	DOM::DocumentImpl *m_impl;
}
+ (DOMDocument *)documentWithImpl:(DOM::DocumentImpl *)impl;
- (id)initWithDocumentImpl:(DOM::DocumentImpl *)impl;
@end

@interface DOMCharacterData (SubclassResponsibility)
- (DOM::CharacterDataImpl *)characterDataImpl;
@end

@interface WebCoreDOMCharacterData : DOMCharacterData
{
	DOM::CharacterDataImpl *m_impl;
}
+ (DOMCharacterData *)characterDataWithImpl:(DOM::CharacterDataImpl *)impl;
- (id)initWithCharacterDataImpl:(DOM::CharacterDataImpl *)impl;
@end

@interface DOMAttr (SubclassResponsibility)
- (DOM::AttrImpl *)attrImpl;
- (Class)classForDOMElement;
@end

@interface WebCoreDOMAttr : DOMAttr
{
	DOM::AttrImpl *m_impl;
}
+ (DOMAttr *)attrWithImpl:(DOM::AttrImpl *)impl;
- (id)initWithAttrImpl:(DOM::AttrImpl *)impl;
@end

@interface DOMElement (SubclassResponsibility)
- (DOM::ElementImpl *)elementImpl;
- (Class)classForDOMAttr;
- (Class)classForDOMNodeList;
@end

@interface WebCoreDOMElement : DOMElement
{
	DOM::ElementImpl *m_impl;
}
+ (DOMElement *)elementWithImpl:(DOM::ElementImpl *)impl;
- (id)initWithElementImpl:(DOM::ElementImpl *)impl;
@end

@interface DOMText (SubclassResponsibility)
- (DOM::TextImpl *)textImpl;
@end

@interface WebCoreDOMText : DOMText
{
	DOM::TextImpl *m_impl;
}
+ (DOMText *)textWithImpl:(DOM::TextImpl *)impl;
- (id)initWithTextImpl:(DOM::TextImpl *)impl;
@end

@interface DOMComment (SubclassResponsibility)
- (DOM::CommentImpl *)commentImpl;
@end

@interface WebCoreDOMComment : DOMComment
{
	DOM::CommentImpl *m_impl;
}
+ (DOMComment *)commentWithImpl:(DOM::CommentImpl *)impl;
- (id)initWithCommentImpl:(DOM::CommentImpl *)impl;
@end

@interface DOMCDATASection (SubclassResponsibility)
- (DOM::CDATASectionImpl *)CDATASectionImpl;
@end

@interface WebCoreDOMCDATASection : DOMCDATASection
{
	DOM::CDATASectionImpl *m_impl;
}
+ (DOMCDATASection *)CDATASectionWithImpl:(DOM::CDATASectionImpl *)impl;
- (id)initWithCDATASectionImpl:(DOM::CDATASectionImpl *)impl;
@end

@interface DOMDocumentType (SubclassResponsibility)
- (DOM::DocumentTypeImpl *)documentTypeImpl;
- (Class)classForDOMNamedNodeMap;
@end

@interface WebCoreDOMDocumentType : DOMDocumentType
{
	DOM::DocumentTypeImpl *m_impl;
}
+ (DOMDocumentType *)documentTypeWithImpl:(DOM::DocumentTypeImpl *)impl;
- (id)initWithDocumentTypeImpl:(DOM::DocumentTypeImpl *)impl;
@end

@interface DOMNotation (SubclassResponsibility)
- (DOM::NotationImpl *)notationImpl;
@end

@interface WebCoreDOMNotation : DOMNotation
{
	DOM::NotationImpl *m_impl;
}
+ (DOMNotation *)notationWithImpl:(DOM::NotationImpl *)impl;
- (id)initWithNotationImpl:(DOM::NotationImpl *)impl;
@end

@interface DOMEntity (SubclassResponsibility)
- (DOM::EntityImpl *)entityImpl;
@end

@interface WebCoreDOMEntity : DOMEntity
{
	DOM::EntityImpl *m_impl;
}
+ (DOMEntity *)entityWithImpl:(DOM::EntityImpl *)impl;
- (id)initWithEntityImpl:(DOM::EntityImpl *)impl;
@end

@interface DOMEntityReference (SubclassResponsibility)
- (DOM::EntityReferenceImpl *)entityReferenceImpl;
@end

@interface WebCoreDOMEntityReference : DOMEntityReference
{
	DOM::EntityReferenceImpl *m_impl;
}
+ (DOMEntityReference *)entityReferenceWithImpl:(DOM::EntityReferenceImpl *)impl;
- (id)initWithEntityReferenceImpl:(DOM::EntityReferenceImpl *)impl;
@end

@interface DOMProcessingInstruction (SubclassResponsibility)
- (DOM::ProcessingInstructionImpl *)processingInstructionImpl;
@end

@interface WebCoreDOMProcessingInstruction : DOMProcessingInstruction
{
	DOM::ProcessingInstructionImpl *m_impl;
}
+ (DOMProcessingInstruction *)processingInstructionWithImpl:(DOM::ProcessingInstructionImpl *)impl;
- (id)initWithProcessingInstructionImpl:(DOM::ProcessingInstructionImpl *)impl;
@end

@interface DOMRange (SubclassResponsibility)
- (DOM::RangeImpl *)rangeImpl;
- (Class)classForDOMDocumentFragment;
- (Class)classForDOMNode;
@end

@interface WebCoreDOMRange : DOMRange
{
	DOM::RangeImpl *m_impl;
}
+ (DOMRange *)rangeWithImpl:(DOM::RangeImpl *)impl;
- (id)initWithRangeImpl:(DOM::RangeImpl *)impl;
@end
