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

@class NSObject;

@interface WebCoreDOMObject : NSObject
{
    void *details;
}
- (id)initWithDetails:(void *)d;
@end

@protocol WebCoreNodeImplWrapper
- (DOM::NodeImpl *)impl;
@end

@protocol WebCoreNamedNodeMapImplWrapper
- (DOM::NamedNodeMapImpl *)impl;
@end

@protocol WebCoreNodeListImplWrapper
- (DOM::NodeListImpl *)impl;
@end

@protocol WebCoreDOMImplementationImplWrapper
- (DOM::DOMImplementationImpl *)impl;
@end

@protocol WebCoreDocumentFragmentImplWrapper
- (DOM::DocumentFragmentImpl *)impl;
@end

@protocol WebCoreDocumentImplWrapper
- (DOM::DocumentImpl *)impl;
@end

@protocol WebCoreCharacterDataImplWrapper
- (DOM::CharacterDataImpl *)impl;
@end

@protocol WebCoreAttrImplWrapper
- (DOM::AttrImpl *)impl;
@end

@protocol WebCoreElementImplWrapper
- (DOM::ElementImpl *)impl;
@end

@protocol WebCoreTextImplWrapper
- (DOM::TextImpl *)impl;
@end

@protocol WebCoreCommentImplWrapper
- (DOM::CommentImpl *)impl;
@end

@protocol WebCoreCDATASectionImplWrapper
- (DOM::CDATASectionImpl *)impl;
@end

@protocol WebCoreDocumentTypeImplWrapper
- (DOM::DocumentTypeImpl *)impl;
@end

@protocol WebCoreNotationImplWrapper
- (DOM::NotationImpl *)impl;
@end

@protocol WebCoreEntityImplWrapper
- (DOM::EntityImpl *)impl;
@end

@protocol WebCoreEntityReferenceImplWrapper
- (DOM::EntityReferenceImpl *)impl;
@end

@protocol WebCoreProcessingInstructionImplWrapper
- (DOM::ProcessingInstructionImpl *)impl;
@end

@protocol WebCoreRangeImplWrapper
- (DOM::RangeImpl *)impl;
@end

@interface WebCoreDOMNode : WebCoreDOMObject <DOMNode, WebCoreNodeImplWrapper>
+ (WebCoreDOMNode *)objectWithImpl:(DOM::NodeImpl *)impl;
- (id)initWithNodeImpl:(DOM::NodeImpl *)impl;
@end

@interface WebCoreDOMNamedNodeMap : WebCoreDOMObject <DOMNamedNodeMap, WebCoreNamedNodeMapImplWrapper>
+ (WebCoreDOMNamedNodeMap *)objectWithImpl:(DOM::NamedNodeMapImpl *)impl;
- (id)initWithNamedNodeMapImpl:(DOM::NamedNodeMapImpl *)impl;
@end

@interface WebCoreDOMNodeList : WebCoreDOMObject <DOMNodeList, WebCoreNodeListImplWrapper>
+ (WebCoreDOMNodeList *)objectWithImpl:(DOM::NodeListImpl *)impl;
- (id)initWithNodeListImpl:(DOM::NodeListImpl *)impl;
@end

@interface WebCoreDOMImplementation : WebCoreDOMObject <DOMImplementation, WebCoreDOMImplementationImplWrapper>
+ (WebCoreDOMImplementation *)objectWithImpl:(DOM::DOMImplementationImpl *)impl;
- (id)initWithDOMImplementationImpl:(DOM::DOMImplementationImpl *)impl;
@end

@interface WebCoreDOMDocumentFragment : WebCoreDOMNode <DOMDocumentFragment, WebCoreDocumentFragmentImplWrapper>
+ (WebCoreDOMDocumentFragment *)objectWithImpl:(DOM::DocumentFragmentImpl *)impl;
- (id)initWithDocumentFragmentImpl:(DOM::DocumentFragmentImpl *)impl;
@end

@interface WebCoreDOMDocument : WebCoreDOMNode <DOMDocument, WebCoreDocumentImplWrapper>
+ (WebCoreDOMDocument *)objectWithImpl:(DOM::DocumentImpl *)impl;
- (id)initWithDocumentImpl:(DOM::DocumentImpl *)impl;
@end

@interface WebCoreDOMCharacterData : WebCoreDOMNode <DOMCharacterData, WebCoreCharacterDataImplWrapper>
+ (WebCoreDOMCharacterData *)objectWithImpl:(DOM::CharacterDataImpl *)impl;
- (id)initWithCharacterDataImpl:(DOM::CharacterDataImpl *)impl;
@end

@interface WebCoreDOMAttr : WebCoreDOMNode <DOMAttr, WebCoreAttrImplWrapper>
+ (WebCoreDOMAttr *)objectWithImpl:(DOM::AttrImpl *)impl;
- (id)initWithAttrImpl:(DOM::AttrImpl *)impl;
@end

@interface WebCoreDOMElement : WebCoreDOMNode <DOMElement, WebCoreElementImplWrapper>
+ (WebCoreDOMElement *)objectWithImpl:(DOM::ElementImpl *)impl;
- (id)initWithElementImpl:(DOM::ElementImpl *)impl;
@end

@interface WebCoreDOMText : WebCoreDOMCharacterData <DOMText, WebCoreTextImplWrapper>
+ (WebCoreDOMText *)objectWithImpl:(DOM::TextImpl *)impl;
- (id)initWithTextImpl:(DOM::TextImpl *)impl;
@end

@interface WebCoreDOMComment : WebCoreDOMCharacterData <WebCoreCommentImplWrapper>
+ (WebCoreDOMComment *)objectWithImpl:(DOM::CommentImpl *)impl;
- (id)initWithCommentImpl:(DOM::CommentImpl *)impl;
@end

@interface WebCoreDOMCDATASection : WebCoreDOMText <DOMCDATASection, WebCoreCDATASectionImplWrapper>
+ (WebCoreDOMCDATASection *)objectWithImpl:(DOM::CDATASectionImpl *)impl;
- (id)initWithCDATASectionImpl:(DOM::CDATASectionImpl *)impl;
@end

@interface WebCoreDOMDocumentType : WebCoreDOMNode <DOMDocumentType, WebCoreDocumentTypeImplWrapper>
+ (WebCoreDOMDocumentType *)objectWithImpl:(DOM::DocumentTypeImpl *)impl;
- (id)initWithDocumentTypeImpl:(DOM::DocumentTypeImpl *)impl;
@end

@interface WebCoreDOMNotation : WebCoreDOMNode <DOMNotation, WebCoreNotationImplWrapper>
+ (WebCoreDOMNotation *)objectWithImpl:(DOM::NotationImpl *)impl;
- (id)initWithNotationImpl:(DOM::NotationImpl *)impl;
@end

@interface WebCoreDOMEntity : WebCoreDOMNode <DOMEntity, WebCoreEntityImplWrapper>
+ (WebCoreDOMEntity *)objectWithImpl:(DOM::EntityImpl *)impl;
- (id)initWithEntityImpl:(DOM::EntityImpl *)impl;
@end

@interface WebCoreDOMEntityReference : WebCoreDOMNode <DOMEntityReference, WebCoreEntityReferenceImplWrapper>
+ (WebCoreDOMEntityReference *)objectWithImpl:(DOM::EntityReferenceImpl *)impl;
- (id)initWithEntityReferenceImpl:(DOM::EntityReferenceImpl *)impl;
@end

@interface WebCoreDOMProcessingInstruction : WebCoreDOMNode <DOMProcessingInstruction, WebCoreProcessingInstructionImplWrapper>
+ (WebCoreDOMProcessingInstruction *)objectWithImpl:(DOM::ProcessingInstructionImpl *)impl;
- (id)initWithProcessingInstructionImpl:(DOM::ProcessingInstructionImpl *)impl;
@end

@interface WebCoreDOMRange : WebCoreDOMObject <DOMRange, WebCoreRangeImplWrapper>
+ (WebCoreDOMRange *)objectWithImpl:(DOM::RangeImpl *)impl;
- (id)initWithRangeImpl:(DOM::RangeImpl *)impl;
@end


//------------------------------------------------------------------------------------------
// Impl accessor conveniences

inline DOM::NodeImpl *nodeImpl(id <DOMNode> instance)
{
    return [(id <WebCoreNodeImplWrapper>)instance impl];
}

inline DOM::NamedNodeMapImpl *namedNodeMapImpl(id <DOMNamedNodeMap> instance)
{
    return [(id <WebCoreNamedNodeMapImplWrapper>)instance impl];
}

inline DOM::NodeListImpl *nodeListImpl(id <DOMNodeList> instance)
{
    return [(id <WebCoreNodeListImplWrapper>)instance impl];
}

inline DOM::DOMImplementationImpl *domImplementationImpl(id <DOMImplementation> instance)
{
    return [(id <WebCoreDOMImplementationImplWrapper>)instance impl];
}

inline DOM::DocumentFragmentImpl *documentFragmentImpl(id <DOMDocumentFragment> instance)
{
    return [(id <WebCoreDocumentFragmentImplWrapper>)instance impl];
}

inline DOM::DocumentImpl *documentImpl(id <DOMDocument> instance)
{
    return [(id <WebCoreDocumentImplWrapper>)instance impl];
}

inline DOM::CharacterDataImpl *characterDataImpl(id <DOMCharacterData> instance)
{
    return [(id <WebCoreCharacterDataImplWrapper>)instance impl];
}

inline DOM::AttrImpl *attrImpl(id <DOMAttr> instance)
{
    return [(id <WebCoreAttrImplWrapper>)instance impl];
}

inline DOM::ElementImpl *elementImpl(id <DOMElement> instance)
{
    return [(id <WebCoreElementImplWrapper>)instance impl];
}

inline DOM::TextImpl *textImpl(id <DOMText> instance)
{
    return [(id <WebCoreTextImplWrapper>)instance impl];
}

inline DOM::CommentImpl *commentImpl(id <DOMComment> instance)
{
    return [(id <WebCoreCommentImplWrapper>)instance impl];
}

inline DOM::CDATASectionImpl *cdataSectionImpl(id <DOMCDATASection> instance)
{
    return [(id <WebCoreCDATASectionImplWrapper>)instance impl];
}

inline DOM::DocumentTypeImpl *documentTypeImpl(id <DOMDocumentType> instance)
{
    return [(id <WebCoreDocumentTypeImplWrapper>)instance impl];
}

inline DOM::NotationImpl *notationImpl(id <DOMNotation> instance)
{
    return [(id <WebCoreNotationImplWrapper>)instance impl];
}

inline DOM::EntityImpl *entityImpl(id <DOMEntity> instance)
{
    return [(id <WebCoreEntityImplWrapper>)instance impl];
}

inline DOM::EntityReferenceImpl *entityReferenceImpl(id <DOMEntityReference> instance)
{
    return [(id <WebCoreEntityReferenceImplWrapper>)instance impl];
}

inline DOM::ProcessingInstructionImpl *processingInstructionImpl(id <DOMProcessingInstruction> instance)
{
    return [(id <WebCoreProcessingInstructionImplWrapper>)instance impl];
}

inline DOM::RangeImpl *rangeImpl(id <DOMRange> instance)
{
    return [(id <WebCoreRangeImplWrapper>)instance impl];
}
