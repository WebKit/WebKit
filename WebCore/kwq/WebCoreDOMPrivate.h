/*	
    WebCoreDOMPrivate.h
    Copyright 2002, Apple, Inc. All rights reserved.
*/
#include <dom/dom_element.h>
#include <dom/dom_node.h>
#include <dom/dom_string.h>
#include <dom/dom_text.h>
#include <dom/dom_xml.h>
#include <xml/dom_textimpl.h>
#include <xml/dom_xmlimpl.h>
#include <xml/dom2_rangeimpl.h>
#include <xml/dom2_eventsimpl.h>
#include <xml/xml_tokenizer.h>
#include <xml/dom_docimpl.h>
#include <xml/dom_nodeimpl.h>

#import "WebCoreDOM.h"

extern NSString *domStringToNSString(DOM::DOMString &aString);
extern DOM::DOMString NSStringToDOMString(NSString *aString);

@interface WebCoreDOMDocumentType (CorePrivate)
+ (WebCoreDOMDocumentType *)documentTypeWithImpl: (DOM::DocumentTypeImpl *)impl;
- (DOM::DocumentTypeImpl *)documentTypeImpl;
@end

@interface WebCoreDOMImplementation (CorePrivate)
+ (WebCoreDOMImplementation *)implementationWithImpl: (DOM::DOMImplementationImpl *)impl;
- initWithImpl: (DOM::DOMImplementationImpl *)coreImpl;
- (DOM::DOMImplementationImpl *)DOMImplementationImpl;
@end

@interface WebCoreDOMDocument (CorePrivate)
+ (WebCoreDOMDocument *)documentWithImpl: (DOM::DocumentImpl *)impl;
- (DOM::DocumentImpl *)documentImpl;
@end

@interface WebCoreDOMNamedNodeMap (CorePrivate)
+ (WebCoreDOMNamedNodeMap *)namedNodeMapWithImpl: (DOM::NamedNodeMapImpl *)impl;
- initWithImpl: (DOM::NamedNodeMapImpl *)coreImpl;
- (DOM::NamedNodeMapImpl *)impl;
@end

@interface WebCoreDOMElement (CorePrivate)
+ (WebCoreDOMElement *)elementWithImpl: (DOM::ElementImpl *)impl;
- (DOM::ElementImpl *)elementImpl;
@end

@interface WebCoreDOMAttr (CorePrivate)
+ (WebCoreDOMAttr *)attrWithImpl: (DOM::AttrImpl *)impl;
- (DOM::AttrImpl *)attrImpl;
@end

@interface WebCoreDOMDocumentFragment (CorePrivate)
+ (WebCoreDOMDocumentFragment *)documentFragmentWithImpl: (DOM::DocumentFragmentImpl *)impl;
- (DOM::DocumentFragmentImpl *)documentFragmentImpl;
@end

@interface WebCoreDOMCharacterData (CorePrivate)
+ (WebCoreDOMCharacterData *)characterDataWithImpl: (DOM::CharacterDataImpl *)impl;
- initWithImpl: (DOM::CharacterDataImpl *)coreImpl;
- (DOM::CharacterDataImpl *)impl;
@end

@interface WebCoreDOMText (CorePrivate)
+ (WebCoreDOMText *)textWithImpl: (DOM::TextImpl *)impl;
- (DOM::TextImpl *)textImpl;
@end

@interface WebCoreDOMComment (CorePrivate)
+ (WebCoreDOMComment *)commentWithImpl: (DOM::CommentImpl *)impl;
- (DOM::CommentImpl *)commentImpl;
@end

@interface WebCoreDOMCDATASection (CorePrivate)
+ (WebCoreDOMCDATASection *)CDATASectionWithImpl: (DOM::CDATASectionImpl *)impl;
- (DOM::CDATASectionImpl *)CDATASectionImpl;
@end

@interface WebCoreDOMProcessingInstruction (CorePrivate)
+ (WebCoreDOMProcessingInstruction *)processingInstructionWithImpl: (DOM::ProcessingInstructionImpl *)impl;
- (DOM::ProcessingInstructionImpl *)processingInstructionImpl;
@end

@interface WebCoreDOMEntityReference (CorePrivate)
+ (WebCoreDOMEntityReference *)entityReferenceWithImpl: (DOM::EntityReferenceImpl *)impl;
- (DOM::EntityReferenceImpl *)entityReferenceImpl;
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

