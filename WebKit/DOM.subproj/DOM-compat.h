/*	
    DOM-compat.h
    Copyright 2004, Apple, Inc. All rights reserved.
*/

@protocol DOMNode;
@protocol DOMNamedNodeMap;
@protocol DOMNodeList;
@protocol DOMImplementation;
@protocol DOMDocumentFragment;
@protocol DOMDocument;
@protocol DOMCharacterData;
@protocol DOMAttr;
@protocol DOMElement;
@protocol DOMText;
@protocol DOMComment;
@protocol DOMCDATASection;
@protocol DOMDocumentType;
@protocol DOMNotation;
@protocol DOMEntity;
@protocol DOMEntityReference;
@protocol DOMProcessingInstruction;

@protocol WebDOMNode <DOMNode>
@end

@protocol WebDOMNamedNodeMap <DOMNamedNodeMap>
@end

@protocol WebDOMNodeList <DOMNodeList>
@end

@protocol WebDOMDocumentType <DOMDocumentType>
@end

@protocol WebDOMDocumentFragment <DOMDocumentFragment>
@end

@protocol WebDOMImplementation <DOMImplementation>
@end

@protocol WebDOMDocument <DOMDocument>
@end

@protocol WebDOMAttr <DOMAttr>
@end

@protocol WebDOMCharacterData <DOMCharacterData>
@end

@protocol WebDOMComment <DOMComment>
@end

@protocol WebDOMText <DOMText>
@end

@protocol WebDOMCDATASection <DOMCDATASection>
@end

@protocol WebDOMProcessingInstruction <DOMProcessingInstruction>
@end

@protocol WebDOMEntityReference <DOMEntityReference>
@end

@protocol WebDOMElement <DOMElement>
@end

#define WebNodeType DOMNodeType
#define WebDOMAttr DOMAttr
#define WebDOMComment DOMComment
#define WebDOMCDATASection DOMCDATASection
#define WebDOMDocument DOMDocument
#define WebDOMElement DOMElement
#define WebDOMEntityReference DOMEntityReference
#define WebDOMNamedNodeMap DOMNamedNodeMap
#define WebDOMNode DOMNode
#define WebDOMNodeList DOMNodeList
#define WebDOMProcessingInstruction DOMProcessingInstruction
#define WebDOMText DOMText
