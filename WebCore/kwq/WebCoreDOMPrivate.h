/*	
    WebCoreDOMPrivate.h
    Copyright 2002, Apple, Inc. All rights reserved.
*/
#include <dom/dom_element.h>
#include <dom/dom_string.h>
#include <dom/dom_text.h>
#include <dom/dom_xml.h>
#include <xml/dom_textimpl.h>
#include <xml/dom_xmlimpl.h>
#include <xml/dom2_rangeimpl.h>
#include <xml/dom2_eventsimpl.h>
#include <xml/xml_tokenizer.h>
#include <xml/dom_docimpl.h>

#import "WebCoreDOM.h"

extern NSString *domStringToNSString(DOM::DOMString &aString);
extern DOM::DOMString NSStringToDOMString(NSString *aString);

@interface WebCoreDOMDocumentType (CorePrivate)
+ (WebCoreDOMDocumentType *)documentTypeWithImpl: (DOM::DocumentTypeImpl *)impl;
- initWithImpl: (DOM::DocumentTypeImpl *)coreImpl;
- (DOM::DocumentTypeImpl *)impl;
@end

@interface WebCoreDOMImplementation (CorePrivate)
+ (WebCoreDOMImplementation *)implementationWithImpl: (DOM::DOMImplementationImpl *)impl;
- initWithImpl: (DOM::DOMImplementationImpl *)coreImpl;
- (DOM::DOMImplementationImpl *)impl;
@end

@interface WebCoreDOMDocument (CorePrivate)
+ (WebCoreDOMDocument *)documentWithImpl: (DOM::DocumentImpl *)impl;
- initWithImpl: (DOM::DocumentImpl *)coreImpl;
- (DOM::DocumentImpl *)impl;
@end

@interface WebCoreDOMNamedNodeMap (CorePrivate)
+ (WebCoreDOMNamedNodeMap *)namedNodeMapWithImpl: (DOM::NamedNodeMapImpl *)impl;
- initWithImpl: (DOM::NamedNodeMapImpl *)coreImpl;
- (DOM::NamedNodeMapImpl *)impl;
@end

@interface WebCoreDOMElement (CorePrivate)
+ (WebCoreDOMElement *)elementWithImpl: (DOM::ElementImpl *)impl;
- initWithImpl: (DOM::ElementImpl *)coreImpl;
- (DOM::ElementImpl *)impl;
@end

@interface WebCoreDOMAttr (CorePrivate)
+ (WebCoreDOMAttr *)attrWithImpl: (DOM::AttrImpl *)impl;
- initWithImpl: (DOM::AttrImpl *)coreImpl;
- (DOM::AttrImpl *)impl;
@end

@interface WebCoreDOMDocumentFragment (CorePrivate)
+ (WebCoreDOMDocumentFragment *)documentFragmentWithImpl: (DOM::DocumentFragmentImpl *)impl;
- initWithImpl: (DOM::DocumentFragmentImpl *)coreImpl;
- (DOM::DocumentFragmentImpl *)impl;
@end

@interface WebCoreDOMCharacterData (CorePrivate)
+ (WebCoreDOMCharacterData *)characterDataWithImpl: (DOM::CharacterDataImpl *)impl;
- initWithImpl: (DOM::CharacterDataImpl *)coreImpl;
- (DOM::CharacterDataImpl *)impl;
@end

@interface WebCoreDOMText (CorePrivate)
+ (WebCoreDOMText *)textWithImpl: (DOM::TextImpl *)impl;
- initWithImpl: (DOM::TextImpl *)coreImpl;
- (DOM::TextImpl *)impl;
@end

@interface WebCoreDOMComment (CorePrivate)
+ (WebCoreDOMComment *)commentWithImpl: (DOM::CommentImpl *)impl;
- initWithImpl: (DOM::CommentImpl *)coreImpl;
- (DOM::CommentImpl *)impl;
@end

@interface WebCoreDOMCDATASection (CorePrivate)
+ (WebCoreDOMCDATASection *)CDATASectionWithImpl: (DOM::CDATASectionImpl *)impl;
- initWithImpl: (DOM::CDATASectionImpl *)coreImpl;
- (DOM::CDATASectionImpl *)impl;
@end

@interface WebCoreDOMProcessingInstruction (CorePrivate)
+ (WebCoreDOMProcessingInstruction *)processingInstructionWithImpl: (DOM::ProcessingInstructionImpl *)impl;
- initWithImpl: (DOM::ProcessingInstructionImpl *)coreImpl;
- (DOM::ProcessingInstructionImpl *)impl;
@end

@interface WebCoreDOMEntityReference (CorePrivate)
+ (WebCoreDOMEntityReference *)entityReferenceWithImpl: (DOM::EntityReferenceImpl *)impl;
- initWithImpl: (DOM::EntityReferenceImpl *)coreImpl;
- (DOM::EntityReferenceImpl *)impl;
@end

@interface WebCoreDOMNode (CorePrivate)
+ (WebCoreDOMNode *)nodeWithImpl: (DOM::NodeImpl *)impl;
- initWithImpl: (DOM::NodeImpl *)coreImpl;
- (DOM::NodeImpl *)impl;
@end

@interface WebCoreDOMNodeList (CorePrivate)
+ (WebCoreDOMNodeList *)nodeListWithImpl: (DOM::NodeListImpl *)impl;
- initWithImpl: (DOM::NodeListImpl *)coreImpl;
- (DOM::NodeListImpl *)impl;
@end

