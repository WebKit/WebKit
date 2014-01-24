// Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
// Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

// This file is used by bindings/scripts/CodeGeneratorObjC.pm to determine public API.
// All public DOM class interfaces, properties and methods need to be in this file.
// Anything not in the file will be generated into the appropriate private header file.

#include <TargetConditionals.h>

#ifndef OBJC_CODE_GENERATION
#error Do not include this header, instead include the appropriate DOM header.
#endif

@interface DOMAttr : DOMNode 10_4
@property (readonly, copy) NSString *name;
@property (readonly) BOOL specified;
@property (copy) NSString *value;
@property (readonly, strong) DOMElement *ownerElement;
@property (readonly, strong) DOMCSSStyleDeclaration *style WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMCDATASection : DOMText 10_4
@end

@interface DOMCharacterData : DOMNode 10_4
@property (copy) NSString *data;
@property (readonly) unsigned length;
- (NSString *)substringData:(unsigned)offset :(unsigned)length;
- (NSString *)substringData:(unsigned)offset length:(unsigned)length WEBKIT_AVAILABLE_MAC(10_5);
- (void)appendData:(NSString *)data;
- (void)insertData:(unsigned)offset :(NSString *)data;
- (void)deleteData:(unsigned)offset :(unsigned)length;
- (void)replaceData:(unsigned)offset :(unsigned)length :(NSString *)data;
- (void)insertData:(unsigned)offset data:(NSString *)data WEBKIT_AVAILABLE_MAC(10_5);
- (void)deleteData:(unsigned)offset length:(unsigned)length WEBKIT_AVAILABLE_MAC(10_5);
- (void)replaceData:(unsigned)offset length:(unsigned)length data:(NSString *)data WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMComment : DOMCharacterData 10_4
@end

@interface DOMImplementation : DOMObject 10_4
- (BOOL)hasFeature:(NSString *)feature :(NSString *)version;
- (DOMDocumentType *)createDocumentType:(NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId;
- (DOMDocument *)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(DOMDocumentType *)doctype;
- (DOMCSSStyleSheet *)createCSSStyleSheet:(NSString *)title :(NSString *)media;
- (BOOL)hasFeature:(NSString *)feature version:(NSString *)version WEBKIT_AVAILABLE_MAC(10_5);
- (DOMDocumentType *)createDocumentType:(NSString *)qualifiedName publicId:(NSString *)publicId systemId:(NSString *)systemId WEBKIT_AVAILABLE_MAC(10_5);
- (DOMDocument *)createDocument:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName doctype:(DOMDocumentType *)doctype WEBKIT_AVAILABLE_MAC(10_5);
- (DOMCSSStyleSheet *)createCSSStyleSheet:(NSString *)title media:(NSString *)media WEBKIT_AVAILABLE_MAC(10_5);
- (DOMHTMLDocument *)createHTMLDocument:(NSString *)title WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMAbstractView : DOMObject 10_4
@property (readonly, strong) DOMDocument *document;
@end

@interface DOMDocument : DOMNode 10_4
@property (readonly, strong) DOMDocumentType *doctype;
@property (readonly, strong) DOMImplementation *implementation;
@property (readonly, strong) DOMElement *documentElement;
@property (readonly, strong) DOMAbstractView *defaultView;
@property (readonly, strong) DOMStyleSheetList *styleSheets;
@property (readonly, strong) DOMHTMLCollection *images;
@property (readonly, strong) DOMHTMLCollection *applets;
@property (readonly, strong) DOMHTMLCollection *links;
@property (readonly, strong) DOMHTMLCollection *forms;
@property (readonly, strong) DOMHTMLCollection *anchors;
@property (copy) NSString *title;
@property (readonly, copy) NSString *referrer;
@property (readonly, copy) NSString *domain;
@property (readonly, copy) NSString *URL;
@property (strong) DOMHTMLElement *body;
@property (copy) NSString *cookie;
@property (readonly, copy) NSString *inputEncoding WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *xmlEncoding WEBKIT_AVAILABLE_MAC(10_5);
@property (copy) NSString *xmlVersion WEBKIT_AVAILABLE_MAC(10_5);
@property BOOL xmlStandalone WEBKIT_AVAILABLE_MAC(10_5);
@property (copy) NSString *documentURI WEBKIT_AVAILABLE_MAC(10_5);
@property (copy) NSString *charset WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *defaultCharset WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *readyState WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *characterSet WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *preferredStylesheetSet WEBKIT_AVAILABLE_MAC(10_5);
@property (copy) NSString *selectedStylesheetSet WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *lastModified WEBKIT_AVAILABLE_MAC(10_6);
- (DOMElement *)createElement:(NSString *)tagName;
- (DOMDocumentFragment *)createDocumentFragment;
- (DOMText *)createTextNode:(NSString *)data;
- (DOMComment *)createComment:(NSString *)data;
- (DOMCDATASection *)createCDATASection:(NSString *)data;
- (DOMProcessingInstruction *)createProcessingInstruction:(NSString *)target :(NSString *)data;
- (DOMProcessingInstruction *)createProcessingInstruction:(NSString *)target data:(NSString *)data WEBKIT_AVAILABLE_MAC(10_5);
- (DOMAttr *)createAttribute:(NSString *)name;
- (DOMEntityReference *)createEntityReference:(NSString *)name;
- (DOMNodeList *)getElementsByTagName:(NSString *)tagname;
- (DOMNode *)importNode:(DOMNode *)importedNode :(BOOL)deep;
- (DOMElement *)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName;
- (DOMAttr *)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName;
- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMNode *)importNode:(DOMNode *)importedNode deep:(BOOL)deep WEBKIT_AVAILABLE_MAC(10_5);
- (DOMNode *)adoptNode:(DOMNode *)source WEBKIT_AVAILABLE_MAC(10_5);
- (DOMElement *)createElementNS:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName WEBKIT_AVAILABLE_MAC(10_5);
- (DOMAttr *)createAttributeNS:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName WEBKIT_AVAILABLE_MAC(10_5);
- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI localName:(NSString *)localName WEBKIT_AVAILABLE_MAC(10_5);
- (DOMElement *)getElementById:(NSString *)elementId;
- (DOMEvent *)createEvent:(NSString *)eventType;
- (DOMRange *)createRange;
- (DOMCSSStyleDeclaration *)createCSSStyleDeclaration WEBKIT_AVAILABLE_MAC(10_5);
- (DOMCSSStyleDeclaration *)getOverrideStyle:(DOMElement *)element :(NSString *)pseudoElement;
- (DOMCSSStyleDeclaration *)getOverrideStyle:(DOMElement *)element pseudoElement:(NSString *)pseudoElement WEBKIT_AVAILABLE_MAC(10_5);
- (DOMCSSStyleDeclaration *)getComputedStyle:(DOMElement *)element :(NSString *)pseudoElement;
- (DOMCSSStyleDeclaration *)getComputedStyle:(DOMElement *)element pseudoElement:(NSString *)pseudoElement WEBKIT_AVAILABLE_MAC(10_5);
- (DOMCSSRuleList *)getMatchedCSSRules:(DOMElement *)element pseudoElement:(NSString *)pseudoElement WEBKIT_AVAILABLE_MAC(10_5);
- (DOMCSSRuleList *)getMatchedCSSRules:(DOMElement *)element pseudoElement:(NSString *)pseudoElement authorOnly:(BOOL)authorOnly WEBKIT_AVAILABLE_MAC(10_5);
- (DOMNodeList *)getElementsByName:(NSString *)elementName;
- (DOMNodeIterator *)createNodeIterator:(DOMNode *)root whatToShow:(unsigned)whatToShow filter:(id <DOMNodeFilter>)filter expandEntityReferences:(BOOL)expandEntityReferences WEBKIT_AVAILABLE_MAC(10_5);
- (DOMTreeWalker *)createTreeWalker:(DOMNode *)root whatToShow:(unsigned)whatToShow filter:(id <DOMNodeFilter>)filter expandEntityReferences:(BOOL)expandEntityReferences WEBKIT_AVAILABLE_MAC(10_5);
- (DOMNodeIterator *)createNodeIterator:(DOMNode *)root :(unsigned)whatToShow :(id <DOMNodeFilter>)filter :(BOOL)expandEntityReferences;
- (DOMTreeWalker *)createTreeWalker:(DOMNode *)root :(unsigned)whatToShow :(id <DOMNodeFilter>)filter :(BOOL)expandEntityReferences;
- (DOMXPathExpression *)createExpression:(NSString *)expression :(id <DOMXPathNSResolver>)resolver WEBKIT_DEPRECATED_MAC(10_5, 10_5);
- (DOMXPathExpression *)createExpression:(NSString *)expression resolver:(id <DOMXPathNSResolver>)resolver WEBKIT_AVAILABLE_MAC(10_5);
- (id <DOMXPathNSResolver>)createNSResolver:(DOMNode *)nodeResolver WEBKIT_AVAILABLE_MAC(10_5);
- (DOMXPathResult *)evaluate:(NSString *)expression :(DOMNode *)contextNode :(id <DOMXPathNSResolver>)resolver :(unsigned short)type :(DOMXPathResult *)inResult WEBKIT_DEPRECATED_MAC(10_5, 10_5);
- (DOMXPathResult *)evaluate:(NSString *)expression contextNode:(DOMNode *)contextNode resolver:(id <DOMXPathNSResolver>)resolver type:(unsigned short)type inResult:(DOMXPathResult *)inResult WEBKIT_AVAILABLE_MAC(10_5);
- (BOOL)execCommand:(NSString *)command userInterface:(BOOL)userInterface value:(NSString *)value WEBKIT_AVAILABLE_MAC(10_5);
- (BOOL)execCommand:(NSString *)command userInterface:(BOOL)userInterface WEBKIT_AVAILABLE_MAC(10_5);
- (BOOL)execCommand:(NSString *)command WEBKIT_AVAILABLE_MAC(10_5);
- (BOOL)queryCommandEnabled:(NSString *)command WEBKIT_AVAILABLE_MAC(10_5);
- (BOOL)queryCommandIndeterm:(NSString *)command WEBKIT_AVAILABLE_MAC(10_5);
- (BOOL)queryCommandState:(NSString *)command WEBKIT_AVAILABLE_MAC(10_5);
- (BOOL)queryCommandSupported:(NSString *)command WEBKIT_AVAILABLE_MAC(10_5);
- (NSString *)queryCommandValue:(NSString *)command WEBKIT_AVAILABLE_MAC(10_5);
- (DOMElement *)elementFromPoint:(int)x y:(int)y WEBKIT_AVAILABLE_MAC(10_5);
- (DOMNodeList *)getElementsByClassName:(NSString *)tagname WEBKIT_AVAILABLE_MAC(10_6);
- (DOMElement *)querySelector:(NSString *)selectors WEBKIT_AVAILABLE_MAC(10_6);
- (DOMNodeList *)querySelectorAll:(NSString *)selectors WEBKIT_AVAILABLE_MAC(10_6);
#if defined(ENABLE_FULLSCREEN_API) && ENABLE_FULLSCREEN_API
- (void)webkitCancelFullScreen WEBKIT_AVAILABLE_MAC(10_6);
#endif
@end

@interface DOMDocumentFragment : DOMNode 10_4
@end

@interface DOMDocumentType : DOMNode 10_4
@property (readonly, copy) NSString *name;
@property (readonly, strong) DOMNamedNodeMap *entities;
@property (readonly, strong) DOMNamedNodeMap *notations;
@property (readonly, copy) NSString *publicId;
@property (readonly, copy) NSString *systemId;
@property (readonly, copy) NSString *internalSubset;
@end

@interface DOMElement : DOMNode 10_4
@property (readonly, copy) NSString *tagName;
@property (readonly, strong) DOMCSSStyleDeclaration *style;
@property (copy) NSString *className;
@property (readonly) int offsetLeft;
@property (readonly) int offsetTop;
@property (readonly) int offsetWidth;
@property (readonly) int offsetHeight;
@property (readonly, strong) DOMElement *offsetParent;
@property (readonly) int clientWidth;
@property (readonly) int clientHeight;
@property int scrollLeft;
@property int scrollTop;
@property (readonly) int scrollWidth;
@property (readonly) int scrollHeight;
@property (readonly) int clientLeft WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int clientTop WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *innerText WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, strong) DOMElement *firstElementChild WEBKIT_AVAILABLE_MAC(10_6);
@property (readonly, strong) DOMElement *lastElementChild WEBKIT_AVAILABLE_MAC(10_6);
@property (readonly, strong) DOMElement *previousElementSibling WEBKIT_AVAILABLE_MAC(10_6);
@property (readonly, strong) DOMElement *nextElementSibling WEBKIT_AVAILABLE_MAC(10_6);
@property (readonly) unsigned childElementCount WEBKIT_AVAILABLE_MAC(10_6);
- (NSString *)getAttribute:(NSString *)name;
- (void)setAttribute:(NSString *)name :(NSString *)value;
- (void)setAttribute:(NSString *)name value:(NSString *)value WEBKIT_AVAILABLE_MAC(10_5);
- (void)removeAttribute:(NSString *)name;
- (DOMAttr *)getAttributeNode:(NSString *)name;
- (DOMAttr *)setAttributeNode:(DOMAttr *)newAttr;
- (DOMAttr *)removeAttributeNode:(DOMAttr *)oldAttr;
- (DOMNodeList *)getElementsByTagName:(NSString *)name;
- (NSString *)getAttributeNS:(NSString *)namespaceURI :(NSString *)localName;
- (void)setAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName :(NSString *)value;
- (void)removeAttributeNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMAttr *)getAttributeNodeNS:(NSString *)namespaceURI :(NSString *)localName;
- (NSString *)getAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName WEBKIT_AVAILABLE_MAC(10_5);
- (void)setAttributeNS:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName value:(NSString *)value WEBKIT_AVAILABLE_MAC(10_5);
- (void)removeAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName WEBKIT_AVAILABLE_MAC(10_5);
- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI localName:(NSString *)localName WEBKIT_AVAILABLE_MAC(10_5);
- (DOMAttr *)getAttributeNodeNS:(NSString *)namespaceURI localName:(NSString *)localName WEBKIT_AVAILABLE_MAC(10_5);
- (DOMAttr *)setAttributeNodeNS:(DOMAttr *)newAttr;
- (BOOL)hasAttribute:(NSString *)name;
- (BOOL)hasAttributeNS:(NSString *)namespaceURI :(NSString *)localName;
- (BOOL)hasAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName WEBKIT_AVAILABLE_MAC(10_5);
- (void)scrollIntoView:(BOOL)alignWithTop WEBKIT_AVAILABLE_MAC(10_5);
- (void)scrollIntoViewIfNeeded:(BOOL)centerIfNeeded WEBKIT_AVAILABLE_MAC(10_5);
- (void)scrollByLines:(int)lines WEBKIT_AVAILABLE_MAC(10_5);
- (void)scrollByPages:(int)pages WEBKIT_AVAILABLE_MAC(10_5);
- (void)focus WEBKIT_AVAILABLE_MAC(10_6);
- (void)blur WEBKIT_AVAILABLE_MAC(10_6);
- (DOMNodeList *)getElementsByClassName:(NSString *)name WEBKIT_AVAILABLE_MAC(10_6);
- (DOMElement *)querySelector:(NSString *)selectors WEBKIT_AVAILABLE_MAC(10_6);
- (DOMNodeList *)querySelectorAll:(NSString *)selectors WEBKIT_AVAILABLE_MAC(10_6);
#if defined(ENABLE_FULLSCREEN_API) && ENABLE_FULLSCREEN_API
- (void)webkitRequestFullScreen:(unsigned short)flags WEBKIT_AVAILABLE_MAC(10_6);
#endif
@end

@interface DOMEntity : DOMNode 10_4
@property (readonly, copy) NSString *publicId;
@property (readonly, copy) NSString *systemId;
@property (readonly, copy) NSString *notationName;
@end

@interface DOMEntityReference : DOMNode 10_4
@end

@interface DOMBlob : DOMObject 10_6
@property (readonly) unsigned long long size;
@end

@interface DOMFile : DOMBlob 10_6
@property (readonly, copy) NSString *name;
@end

@interface DOMFileList : DOMObject 10_6
@property (readonly) unsigned length;
- (DOMFile *)item:(unsigned)index;
@end

@interface DOMNamedNodeMap : DOMObject 10_4
@property (readonly) unsigned length;
- (DOMNode *)getNamedItem:(NSString *)name;
- (DOMNode *)setNamedItem:(DOMNode *)node;
- (DOMNode *)removeNamedItem:(NSString *)name;
- (DOMNode *)item:(unsigned)index;
- (DOMNode *)getNamedItemNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMNode *)getNamedItemNS:(NSString *)namespaceURI localName:(NSString *)localName WEBKIT_AVAILABLE_MAC(10_5);
- (DOMNode *)setNamedItemNS:(DOMNode *)node;
- (DOMNode *)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMNode *)removeNamedItemNS:(NSString *)namespaceURI localName:(NSString *)localName WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMNode : DOMObject 10_4
@property (readonly, copy) NSString *nodeName;
@property (copy) NSString *nodeValue;
@property (readonly) unsigned short nodeType;
@property (readonly, strong) DOMNode *parentNode;
@property (readonly, strong) DOMNodeList *childNodes;
@property (readonly, strong) DOMNode *firstChild;
@property (readonly, strong) DOMNode *lastChild;
@property (readonly, strong) DOMNode *previousSibling;
@property (readonly, strong) DOMNode *nextSibling;
@property (readonly, strong) DOMNamedNodeMap *attributes;
@property (readonly, strong) DOMDocument *ownerDocument;
@property (readonly, copy) NSString *namespaceURI;
@property (copy) NSString *prefix;
@property (readonly, copy) NSString *localName;
@property (copy) NSString *textContent WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *baseURI WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, strong) DOMElement *parentElement WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) BOOL isContentEditable WEBKIT_AVAILABLE_MAC(10_5);
- (DOMNode *)insertBefore:(DOMNode *)newChild :(DOMNode *)refChild;
- (DOMNode *)insertBefore:(DOMNode *)newChild refChild:(DOMNode *)refChild WEBKIT_AVAILABLE_MAC(10_5);
- (DOMNode *)replaceChild:(DOMNode *)newChild :(DOMNode *)oldChild;
- (DOMNode *)replaceChild:(DOMNode *)newChild oldChild:(DOMNode *)oldChild WEBKIT_AVAILABLE_MAC(10_5);
- (DOMNode *)removeChild:(DOMNode *)oldChild;
- (DOMNode *)appendChild:(DOMNode *)newChild;
- (BOOL)hasChildNodes;
- (DOMNode *)cloneNode:(BOOL)deep;
- (void)normalize;
- (BOOL)isSupported:(NSString *)feature :(NSString *)version;
- (BOOL)isSupported:(NSString *)feature version:(NSString *)version WEBKIT_AVAILABLE_MAC(10_5);
- (BOOL)hasAttributes;
- (BOOL)isSameNode:(DOMNode *)other WEBKIT_AVAILABLE_MAC(10_5);
- (BOOL)isEqualNode:(DOMNode *)other WEBKIT_AVAILABLE_MAC(10_5);
- (NSString *)lookupPrefix:(NSString *)namespaceURI WEBKIT_AVAILABLE_MAC(10_5);
- (BOOL)isDefaultNamespace:(NSString *)namespaceURI WEBKIT_AVAILABLE_MAC(10_5);
- (NSString *)lookupNamespaceURI:(NSString *)prefix WEBKIT_AVAILABLE_MAC(10_5);
- (unsigned short)compareDocumentPosition:(DOMNode *)other WEBKIT_AVAILABLE_MAC(10_6);
- (BOOL)contains:(DOMNode *)other WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMNodeList : DOMObject 10_4
@property (readonly) unsigned length;
- (DOMNode *)item:(unsigned)index;
@end

@interface DOMNotation : DOMNode 10_4
@property (readonly, copy) NSString *publicId;
@property (readonly, copy) NSString *systemId;
@end

@interface DOMProcessingInstruction : DOMCharacterData 10_4
@property (readonly, copy) NSString *target;
@property (readonly, strong) DOMStyleSheet *sheet WEBKIT_AVAILABLE_MAC(10_4);
@end

@interface DOMText : DOMCharacterData 10_4
@property (readonly, copy) NSString *wholeText WEBKIT_AVAILABLE_MAC(10_6);
- (DOMText *)splitText:(unsigned)offset;
- (DOMText *)replaceWholeText:(NSString *)content WEBKIT_AVAILABLE_MAC(10_6);
@end

@interface DOMHTMLAnchorElement : DOMHTMLElement 10_4
@property (copy) NSString *accessKey WEBKIT_DEPRECATED_MAC(10_4, 10_8);
@property (copy) NSString *charset;
@property (copy) NSString *coords;
@property (copy) NSString *href;
@property (copy) NSString *hreflang;
@property (copy) NSString *name;
@property (copy) NSString *rel;
@property (copy) NSString *rev;
@property (copy) NSString *shape;
@property (copy) NSString *target;
@property (copy) NSString *type;
@property (readonly, copy) NSURL *absoluteLinkURL WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *hashName WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *host WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *hostname WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *pathname WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *port WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *protocol WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *search WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *text WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMHTMLAppletElement : DOMHTMLElement 10_4
@property (copy) NSString *align;
@property (copy) NSString *alt;
@property (copy) NSString *archive;
@property (copy) NSString *code;
@property (copy) NSString *codeBase;
@property (copy) NSString *height;
@property int hspace;
@property (copy) NSString *name;
@property (copy) NSString *object;
@property int vspace;
@property (copy) NSString *width;
@end

@interface DOMHTMLAreaElement : DOMHTMLElement 10_4
@property (copy) NSString *accessKey WEBKIT_DEPRECATED_MAC(10_4, 10_8);
@property (copy) NSString *alt;
@property (copy) NSString *coords;
@property (copy) NSString *href;
@property BOOL noHref;
@property (copy) NSString *shape;
@property (copy) NSString *target;
@property (readonly, copy) NSURL *absoluteLinkURL WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *hashName WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *host WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *hostname WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *pathname WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *port WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *protocol WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSString *search WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMHTMLBRElement : DOMHTMLElement 10_4
@property (copy) NSString *clear;
@end

@interface DOMHTMLBaseElement : DOMHTMLElement 10_4
@property (copy) NSString *href;
@property (copy) NSString *target;
@end

@interface DOMHTMLBaseFontElement : DOMHTMLElement 10_4
@property (copy) NSString *color;
@property (copy) NSString *face;
@property (copy) NSString *size;
@end

@interface DOMHTMLBodyElement : DOMHTMLElement 10_4
@property (copy) NSString *aLink;
@property (copy) NSString *background;
@property (copy) NSString *bgColor;
@property (copy) NSString *link;
@property (copy) NSString *text;
@property (copy) NSString *vLink;
@end

@interface DOMHTMLButtonElement : DOMHTMLElement 10_4
@property (copy) NSString *accessKey WEBKIT_DEPRECATED_MAC(10_4, 10_8);
@property (readonly, strong) DOMHTMLFormElement *form;
@property BOOL disabled;
@property (copy) NSString *name;
@property (copy) NSString *type;
@property (copy) NSString *value;
@property BOOL autofocus WEBKIT_AVAILABLE_MAC(10_6);
@property (readonly) BOOL willValidate WEBKIT_AVAILABLE_MAC(10_6);
- (void)click WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMHTMLCanvasElement : DOMHTMLElement 10_5
@property int height;
@property int width;
@end

@interface DOMHTMLCollection : DOMObject 10_4
@property (readonly) unsigned length;
- (DOMNode *)item:(unsigned)index;
- (DOMNode *)namedItem:(NSString *)name;
- (DOMNodeList *)tags:(NSString *)name WEBKIT_AVAILABLE_MAC(10_6);
@end

@interface DOMHTMLDListElement : DOMHTMLElement 10_4
@property BOOL compact;
@end

@interface DOMHTMLDirectoryElement : DOMHTMLElement 10_4
@property BOOL compact;
@end

@interface DOMHTMLDivElement : DOMHTMLElement 10_4
@property (copy) NSString *align;
@end

@interface DOMHTMLDocument : DOMDocument 10_4
@property (readonly, strong) DOMHTMLCollection *embeds WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, strong) DOMHTMLCollection *plugins WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, strong) DOMHTMLCollection *scripts WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int width WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int height WEBKIT_AVAILABLE_MAC(10_5);
@property (copy) NSString *dir WEBKIT_AVAILABLE_MAC(10_5);
@property (copy) NSString *designMode WEBKIT_AVAILABLE_MAC(10_5);
@property (copy) NSString *bgColor WEBKIT_AVAILABLE_MAC(10_5);
@property (copy) NSString *fgColor WEBKIT_AVAILABLE_MAC(10_5);
@property (copy) NSString *alinkColor WEBKIT_AVAILABLE_MAC(10_5);
@property (copy) NSString *linkColor WEBKIT_AVAILABLE_MAC(10_5);
@property (copy) NSString *vlinkColor WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, strong) DOMElement *activeElement WEBKIT_AVAILABLE_MAC(10_6);
@property (readonly, copy) NSString *compatMode WEBKIT_AVAILABLE_MAC(10_6);
- (void)captureEvents WEBKIT_AVAILABLE_MAC(10_5);
- (void)releaseEvents WEBKIT_AVAILABLE_MAC(10_5);
- (void)clear WEBKIT_AVAILABLE_MAC(10_6);
- (BOOL)hasFocus WEBKIT_AVAILABLE_MAC(10_6);
- (void)open;
- (void)close;
- (void)write:(NSString *)text;
- (void)writeln:(NSString *)text;
@end

@interface DOMHTMLElement : DOMElement 10_4
@property (copy) NSString *accessKey WEBKIT_AVAILABLE_MAC(10_8);
@property (copy) NSString *title;
@property (copy) NSString *idName;
@property (copy) NSString *lang;
@property (copy) NSString *dir;
@property (copy) NSString *innerHTML;
@property (copy) NSString *innerText;
@property (copy) NSString *outerHTML;
@property (copy) NSString *outerText;
@property (readonly, strong) DOMHTMLCollection *children;
@property (copy) NSString *contentEditable;
@property (readonly) BOOL isContentEditable;
@property (readonly, copy) NSString *titleDisplayString WEBKIT_AVAILABLE_MAC(10_5);
@property int tabIndex;
- (void)click WEBKIT_AVAILABLE_MAC(10_8);
@end

@interface DOMHTMLEmbedElement : DOMHTMLElement 10_4
@property (copy) NSString *align;
@property int height;
@property (copy) NSString *name;
@property (copy) NSString *src;
@property (copy) NSString *type;
@property int width;
@end

@interface DOMHTMLFieldSetElement : DOMHTMLElement 10_4
@property (readonly, strong) DOMHTMLFormElement *form;
@end

@interface DOMHTMLFontElement : DOMHTMLElement 10_4
@property (copy) NSString *color;
@property (copy) NSString *face;
@property (copy) NSString *size;
@end

@interface DOMHTMLFormElement : DOMHTMLElement 10_4
@property (readonly, strong) DOMHTMLCollection *elements;
@property (readonly) int length;
@property (copy) NSString *name;
@property (copy) NSString *acceptCharset;
@property (copy) NSString *action;
@property (copy) NSString *enctype;
@property (copy) NSString *method;
@property (copy) NSString *target;
@property (copy) NSString *encoding WEBKIT_AVAILABLE_MAC(10_5);
- (void)submit;
- (void)reset;
@end

@interface DOMHTMLFrameElement : DOMHTMLElement 10_4
@property (copy) NSString *frameBorder;
@property (copy) NSString *longDesc;
@property (copy) NSString *marginHeight;
@property (copy) NSString *marginWidth;
@property (copy) NSString *name;
@property BOOL noResize;
@property (copy) NSString *scrolling;
@property (copy) NSString *src;
@property (readonly, strong) DOMDocument *contentDocument;
@property (readonly, strong) DOMAbstractView *contentWindow WEBKIT_AVAILABLE_MAC(10_5);
@property (copy) NSString *location WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int width WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int height WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMHTMLFrameSetElement : DOMHTMLElement 10_4
@property (copy) NSString *cols;
@property (copy) NSString *rows;
@end

@interface DOMHTMLHRElement : DOMHTMLElement 10_4
@property (copy) NSString *align;
@property BOOL noShade;
@property (copy) NSString *size;
@property (copy) NSString *width;
@end

@interface DOMHTMLHeadElement : DOMHTMLElement 10_4
@property (copy) NSString *profile;
@end

@interface DOMHTMLHeadingElement : DOMHTMLElement 10_4
@property (copy) NSString *align;
@end

@interface DOMHTMLHtmlElement : DOMHTMLElement 10_4
@property (copy) NSString *version;
@end

@interface DOMHTMLIFrameElement : DOMHTMLElement 10_4
@property (copy) NSString *align;
@property (copy) NSString *frameBorder;
@property (copy) NSString *height;
@property (copy) NSString *longDesc;
@property (copy) NSString *marginHeight;
@property (copy) NSString *marginWidth;
@property (copy) NSString *name;
@property (copy) NSString *scrolling;
@property (copy) NSString *src;
@property (copy) NSString *width;
@property (readonly, strong) DOMDocument *contentDocument;
@property (readonly, strong) DOMAbstractView *contentWindow WEBKIT_AVAILABLE_MAC(10_6);
@end

@interface DOMHTMLImageElement : DOMHTMLElement 10_4
@property (copy) NSString *name;
@property (copy) NSString *align;
@property (copy) NSString *alt;
@property (copy) NSString *border;
@property int height;
@property int hspace;
@property BOOL isMap;
@property (copy) NSString *longDesc;
@property (copy) NSString *src;
@property (copy) NSString *useMap;
@property int vspace;
@property int width;
@property (readonly, copy) NSString *altDisplayString WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSURL *absoluteImageURL WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) BOOL complete WEBKIT_AVAILABLE_MAC(10_5);
@property (copy) NSString *lowsrc WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int naturalHeight WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int naturalWidth WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int x WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int y WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMHTMLInputElement : DOMHTMLElement 10_4
@property (copy) NSString *defaultValue;
@property BOOL defaultChecked;
@property (readonly, strong) DOMHTMLFormElement *form;
@property (copy) NSString *accept;
@property (copy) NSString *accessKey WEBKIT_DEPRECATED_MAC(10_4, 10_8);
@property (copy) NSString *align;
@property (copy) NSString *alt;
@property BOOL checked;
@property BOOL disabled;
@property int maxLength;
@property (copy) NSString *name;
@property BOOL readOnly;
@property (copy) NSString *size;
@property (copy) NSString *src;
@property (copy) NSString *type;
@property (copy) NSString *useMap;
@property (copy) NSString *value;
@property (readonly, copy) NSString *altDisplayString WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, copy) NSURL *absoluteImageURL WEBKIT_AVAILABLE_MAC(10_5);
@property BOOL indeterminate WEBKIT_AVAILABLE_MAC(10_5);
@property int selectionStart WEBKIT_AVAILABLE_MAC(10_5);
@property int selectionEnd WEBKIT_AVAILABLE_MAC(10_5);
@property BOOL autofocus WEBKIT_AVAILABLE_MAC(10_6);
@property BOOL multiple WEBKIT_AVAILABLE_MAC(10_6);
@property (readonly) BOOL willValidate WEBKIT_AVAILABLE_MAC(10_6);
@property (strong) DOMFileList *files WEBKIT_AVAILABLE_MAC(10_6);
- (void)select;
- (void)click;
- (void)setSelectionRange:(int)start end:(int)end WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMHTMLLIElement : DOMHTMLElement 10_4
@property (copy) NSString *type;
@property int value;
@end

@interface DOMHTMLLabelElement : DOMHTMLElement 10_4
@property (copy) NSString *accessKey WEBKIT_DEPRECATED_MAC(10_4, 10_8);
@property (readonly, strong) DOMHTMLFormElement *form;
@property (copy) NSString *htmlFor;
@end

@interface DOMHTMLLegendElement : DOMHTMLElement 10_4
@property (copy) NSString *accessKey WEBKIT_DEPRECATED_MAC(10_4, 10_8);
@property (readonly, strong) DOMHTMLFormElement *form;
@property (copy) NSString *align;
@end

@interface DOMHTMLLinkElement : DOMHTMLElement 10_4
@property BOOL disabled;
@property (copy) NSString *charset;
@property (copy) NSString *href;
@property (copy) NSString *hreflang;
@property (copy) NSString *media;
@property (copy) NSString *rel;
@property (copy) NSString *rev;
@property (copy) NSString *target;
@property (copy) NSString *type;
@property (readonly, copy) NSURL *absoluteLinkURL WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, strong) DOMStyleSheet *sheet WEBKIT_AVAILABLE_MAC(10_4);
@end

@interface DOMHTMLMapElement : DOMHTMLElement 10_4
@property (readonly, strong) DOMHTMLCollection *areas;
@property (copy) NSString *name;
@end

@interface DOMHTMLMarqueeElement : DOMHTMLElement 10_5
- (void)start;
- (void)stop;
@end

@interface DOMHTMLMenuElement : DOMHTMLElement 10_4
@property BOOL compact;
@end

@interface DOMHTMLMetaElement : DOMHTMLElement 10_4
@property (copy) NSString *content;
@property (copy) NSString *httpEquiv;
@property (copy) NSString *name;
@property (copy) NSString *scheme;
@end

@interface DOMHTMLModElement : DOMHTMLElement 10_4
@property (copy) NSString *cite;
@property (copy) NSString *dateTime;
@end

@interface DOMHTMLOListElement : DOMHTMLElement 10_4
@property BOOL compact;
@property int start;
@property (copy) NSString *type;
@end

@interface DOMHTMLObjectElement : DOMHTMLElement 10_4
@property (readonly, strong) DOMHTMLFormElement *form;
@property (copy) NSString *code;
@property (copy) NSString *align;
@property (copy) NSString *archive;
@property (copy) NSString *border;
@property (copy) NSString *codeBase;
@property (copy) NSString *codeType;
@property (copy) NSString *data;
@property BOOL declare;
@property (copy) NSString *height;
@property int hspace;
@property (copy) NSString *name;
@property (copy) NSString *standby;
@property (copy) NSString *type;
@property (copy) NSString *useMap;
@property int vspace;
@property (copy) NSString *width;
@property (readonly, strong) DOMDocument *contentDocument;
@property (readonly, copy) NSURL *absoluteImageURL WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMHTMLOptGroupElement : DOMHTMLElement 10_4
@property BOOL disabled;
@property (copy) NSString *label;
@end

@interface DOMHTMLOptionElement : DOMHTMLElement 10_4
@property (readonly, strong) DOMHTMLFormElement *form;
@property BOOL defaultSelected;
@property (readonly, copy) NSString *text;
@property (readonly) int index;
@property BOOL disabled;
@property (copy) NSString *label;
@property BOOL selected;
@property (copy) NSString *value;
@end

@interface DOMHTMLOptionsCollection : DOMObject 10_4
@property unsigned length;
@property int selectedIndex WEBKIT_AVAILABLE_MAC(10_5);
- (DOMNode *)item:(unsigned)index;
- (DOMNode *)namedItem:(NSString *)name;
- (void)add:(DOMHTMLOptionElement *)option index:(unsigned)index WEBKIT_AVAILABLE_MAC(10_5);
- (void)remove:(unsigned)index WEBKIT_AVAILABLE_MAC(10_6);
@end

@interface DOMHTMLParagraphElement : DOMHTMLElement 10_4
@property (copy) NSString *align;
@end

@interface DOMHTMLParamElement : DOMHTMLElement 10_4
@property (copy) NSString *name;
@property (copy) NSString *type;
@property (copy) NSString *value;
@property (copy) NSString *valueType;
@end

@interface DOMHTMLPreElement : DOMHTMLElement 10_4
@property int width;
@property BOOL wrap WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMHTMLQuoteElement : DOMHTMLElement 10_4
@property (copy) NSString *cite;
@end

@interface DOMHTMLScriptElement : DOMHTMLElement 10_4
@property (copy) NSString *text;
@property (copy) NSString *htmlFor;
@property (copy) NSString *event;
@property (copy) NSString *charset;
@property BOOL defer;
@property (copy) NSString *src;
@property (copy) NSString *type;
@end

@interface DOMHTMLSelectElement : DOMHTMLElement 10_4
@property (readonly, copy) NSString *type;
@property int selectedIndex;
@property (copy) NSString *value;
@property (readonly) int length;
@property (readonly, strong) DOMHTMLFormElement *form;
@property (readonly, strong) DOMHTMLOptionsCollection *options;
@property BOOL disabled;
@property BOOL multiple;
@property (copy) NSString *name;
@property int size;
@property (readonly) BOOL willValidate WEBKIT_AVAILABLE_MAC(10_6);
@property BOOL autofocus WEBKIT_AVAILABLE_MAC(10_6);
- (void)add:(DOMHTMLElement *)element :(DOMHTMLElement *)before;
- (void)add:(DOMHTMLElement *)element before:(DOMHTMLElement *)before WEBKIT_AVAILABLE_MAC(10_5);
- (void)remove:(int)index;
- (DOMNode *)item:(unsigned)index WEBKIT_AVAILABLE_MAC(10_6);
- (DOMNode *)namedItem:(NSString *)name WEBKIT_AVAILABLE_MAC(10_6);
@end

@interface DOMHTMLStyleElement : DOMHTMLElement 10_4
@property BOOL disabled;
@property (copy) NSString *media;
@property (copy) NSString *type;
@property (readonly, strong) DOMStyleSheet *sheet WEBKIT_AVAILABLE_MAC(10_4);
@end

@interface DOMHTMLTableCaptionElement : DOMHTMLElement 10_4
@property (copy) NSString *align;
@end

@interface DOMHTMLTableCellElement : DOMHTMLElement 10_4
@property (readonly) int cellIndex;
@property (copy) NSString *abbr;
@property (copy) NSString *align;
@property (copy) NSString *axis;
@property (copy) NSString *bgColor;
@property (copy) NSString *ch;
@property (copy) NSString *chOff;
@property int colSpan;
@property (copy) NSString *headers;
@property (copy) NSString *height;
@property BOOL noWrap;
@property int rowSpan;
@property (copy) NSString *scope;
@property (copy) NSString *vAlign;
@property (copy) NSString *width;
@end

@interface DOMHTMLTableColElement : DOMHTMLElement 10_4
@property (copy) NSString *align;
@property (copy) NSString *ch;
@property (copy) NSString *chOff;
@property int span;
@property (copy) NSString *vAlign;
@property (copy) NSString *width;
@end

@interface DOMHTMLTableElement : DOMHTMLElement 10_4
@property (strong) DOMHTMLTableCaptionElement *caption;
@property (strong) DOMHTMLTableSectionElement *tHead;
@property (strong) DOMHTMLTableSectionElement *tFoot;
@property (readonly, strong) DOMHTMLCollection *rows;
@property (readonly, strong) DOMHTMLCollection *tBodies;
@property (copy) NSString *align;
@property (copy) NSString *bgColor;
@property (copy) NSString *border;
@property (copy) NSString *cellPadding;
@property (copy) NSString *cellSpacing;
@property (copy) NSString *frameBorders;
@property (copy) NSString *rules;
@property (copy) NSString *summary;
@property (copy) NSString *width;
- (DOMHTMLElement *)createTHead;
- (void)deleteTHead;
- (DOMHTMLElement *)createTFoot;
- (void)deleteTFoot;
- (DOMHTMLElement *)createCaption;
- (void)deleteCaption;
- (DOMHTMLElement *)insertRow:(int)index;
- (void)deleteRow:(int)index;
@end

@interface DOMHTMLTableRowElement : DOMHTMLElement 10_4
@property (readonly) int rowIndex;
@property (readonly) int sectionRowIndex;
@property (readonly, strong) DOMHTMLCollection *cells;
@property (copy) NSString *align;
@property (copy) NSString *bgColor;
@property (copy) NSString *ch;
@property (copy) NSString *chOff;
@property (copy) NSString *vAlign;
- (DOMHTMLElement *)insertCell:(int)index;
- (void)deleteCell:(int)index;
@end

@interface DOMHTMLTableSectionElement : DOMHTMLElement 10_4
@property (copy) NSString *align;
@property (copy) NSString *ch;
@property (copy) NSString *chOff;
@property (copy) NSString *vAlign;
@property (readonly, strong) DOMHTMLCollection *rows;
- (DOMHTMLElement *)insertRow:(int)index;
- (void)deleteRow:(int)index;
@end

@interface DOMHTMLTextAreaElement : DOMHTMLElement 10_4
@property (copy) NSString *defaultValue;
@property (readonly, strong) DOMHTMLFormElement *form;
@property (copy) NSString *accessKey WEBKIT_DEPRECATED_MAC(10_4, 10_8);
@property int cols;
@property BOOL disabled;
@property (copy) NSString *name;
@property BOOL readOnly;
@property int rows;
@property (readonly, copy) NSString *type;
@property (copy) NSString *value;
@property int selectionStart WEBKIT_AVAILABLE_MAC(10_5);
@property int selectionEnd WEBKIT_AVAILABLE_MAC(10_5);
@property BOOL autofocus WEBKIT_AVAILABLE_MAC(10_6);
@property (readonly) BOOL willValidate WEBKIT_AVAILABLE_MAC(10_6);
- (void)select;
- (void)setSelectionRange:(int)start end:(int)end WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMHTMLTitleElement : DOMHTMLElement 10_4
@property (copy) NSString *text;
@end

@interface DOMHTMLUListElement : DOMHTMLElement 10_4
@property BOOL compact;
@property (copy) NSString *type;
@end

@interface DOMStyleSheetList : DOMObject 10_4
@property (readonly) unsigned length;
- (DOMStyleSheet *)item:(unsigned)index;
@end

@interface DOMCSSCharsetRule : DOMCSSRule 10_4
@property (readonly, copy) NSString *encoding;
@end

@interface DOMCSSFontFaceRule : DOMCSSRule 10_4
@property (readonly, strong) DOMCSSStyleDeclaration *style;
@end

@interface DOMCSSImportRule : DOMCSSRule 10_4
@property (readonly, copy) NSString *href;
@property (readonly, strong) DOMMediaList *media;
@property (readonly, strong) DOMCSSStyleSheet *styleSheet;
@end

@interface DOMCSSMediaRule : DOMCSSRule 10_4
@property (readonly, strong) DOMMediaList *media;
@property (readonly, strong) DOMCSSRuleList *cssRules;
- (unsigned)insertRule:(NSString *)rule :(unsigned)index;
- (unsigned)insertRule:(NSString *)rule index:(unsigned)index WEBKIT_AVAILABLE_MAC(10_5);
- (void)deleteRule:(unsigned)index;
@end

@interface DOMCSSPageRule : DOMCSSRule 10_4
@property (copy) NSString *selectorText;
@property (readonly, strong) DOMCSSStyleDeclaration *style;
@end

@interface DOMCSSPrimitiveValue : DOMCSSValue 10_4
@property (readonly) unsigned short primitiveType;
- (void)setFloatValue:(unsigned short)unitType :(float)floatValue;
- (void)setFloatValue:(unsigned short)unitType floatValue:(float)floatValue WEBKIT_AVAILABLE_MAC(10_5);
- (float)getFloatValue:(unsigned short)unitType;
- (void)setStringValue:(unsigned short)stringType :(NSString *)stringValue;
- (void)setStringValue:(unsigned short)stringType stringValue:(NSString *)stringValue WEBKIT_AVAILABLE_MAC(10_5);
- (NSString *)getStringValue;
- (DOMCounter *)getCounterValue;
- (DOMRect *)getRectValue;
- (DOMRGBColor *)getRGBColorValue;
@end

@interface DOMRGBColor : DOMObject 10_4
@property (readonly, strong) DOMCSSPrimitiveValue *red;
@property (readonly, strong) DOMCSSPrimitiveValue *green;
@property (readonly, strong) DOMCSSPrimitiveValue *blue;
@property (readonly, strong) DOMCSSPrimitiveValue *alpha;
#if !TARGET_OS_IPHONE
@property (readonly, copy) NSColor *color WEBKIT_AVAILABLE_MAC(10_5);
#else
- (CGColorRef)color;
#endif
@end

@interface DOMCSSRule : DOMObject 10_4
@property (readonly) unsigned short type;
@property (copy) NSString *cssText;
@property (readonly, strong) DOMCSSStyleSheet *parentStyleSheet;
@property (readonly, strong) DOMCSSRule *parentRule;
@end

@interface DOMCSSRuleList : DOMObject 10_4
@property (readonly) unsigned length;
- (DOMCSSRule *)item:(unsigned)index;
@end

@interface DOMCSSStyleDeclaration : DOMObject 10_4
@property (copy) NSString *cssText;
@property (readonly) unsigned length;
@property (readonly, strong) DOMCSSRule *parentRule;
- (NSString *)getPropertyValue:(NSString *)propertyName;
- (DOMCSSValue *)getPropertyCSSValue:(NSString *)propertyName;
- (NSString *)removeProperty:(NSString *)propertyName;
- (NSString *)getPropertyPriority:(NSString *)propertyName;
- (void)setProperty:(NSString *)propertyName :(NSString *)value :(NSString *)priority;
- (void)setProperty:(NSString *)propertyName value:(NSString *)value priority:(NSString *)priority WEBKIT_AVAILABLE_MAC(10_5);
- (NSString *)item:(unsigned)index;
- (NSString *)getPropertyShorthand:(NSString *)propertyName WEBKIT_DEPRECATED_MAC(10_5, 10_5);
- (BOOL)isPropertyImplicit:(NSString *)propertyName WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMCSSStyleRule : DOMCSSRule 10_4
@property (copy) NSString *selectorText;
@property (readonly, strong) DOMCSSStyleDeclaration *style;
@end

@interface DOMStyleSheet : DOMObject 10_4
@property (readonly, copy) NSString *type;
@property BOOL disabled;
@property (readonly, strong) DOMNode *ownerNode;
@property (readonly, strong) DOMStyleSheet *parentStyleSheet;
@property (readonly, copy) NSString *href;
@property (readonly, copy) NSString *title;
@property (readonly, strong) DOMMediaList *media;
@end

@interface DOMCSSStyleSheet : DOMStyleSheet 10_4
@property (readonly, strong) DOMCSSRule *ownerRule;
@property (readonly, strong) DOMCSSRuleList *cssRules;
@property (readonly, strong) DOMCSSRuleList *rules WEBKIT_AVAILABLE_MAC(10_6);
- (unsigned)insertRule:(NSString *)rule :(unsigned)index;
- (unsigned)insertRule:(NSString *)rule index:(unsigned)index WEBKIT_AVAILABLE_MAC(10_5);
- (void)deleteRule:(unsigned)index;
- (int)addRule:(NSString *)selector style:(NSString *)style index:(unsigned)index WEBKIT_AVAILABLE_MAC(10_6);
- (void)removeRule:(unsigned)index WEBKIT_AVAILABLE_MAC(10_6);
@end

@interface DOMCSSValue : DOMObject 10_4
@property (copy) NSString *cssText;
@property (readonly) unsigned short cssValueType;
@end

@interface DOMCSSValueList : DOMCSSValue 10_4
@property (readonly) unsigned length;
- (DOMCSSValue *)item:(unsigned)index;
@end

@interface DOMCSSUnknownRule : DOMCSSRule 10_4
@end

@interface DOMCounter : DOMObject 10_4
@property (readonly, copy) NSString *identifier;
@property (readonly, copy) NSString *listStyle;
@property (readonly, copy) NSString *separator;
@end

@interface DOMRect : DOMObject 10_4
@property (readonly, strong) DOMCSSPrimitiveValue *top;
@property (readonly, strong) DOMCSSPrimitiveValue *right;
@property (readonly, strong) DOMCSSPrimitiveValue *bottom;
@property (readonly, strong) DOMCSSPrimitiveValue *left;
@end

@interface DOMEvent : DOMObject 10_4
@property (readonly, copy) NSString *type;
@property (readonly, strong) id <DOMEventTarget> target;
@property (readonly, strong) id <DOMEventTarget> currentTarget;
@property (readonly) unsigned short eventPhase;
@property (readonly) BOOL bubbles;
@property (readonly) BOOL cancelable;
@property (readonly) DOMTimeStamp timeStamp;
@property (readonly, strong) id <DOMEventTarget> srcElement WEBKIT_AVAILABLE_MAC(10_6);
@property BOOL returnValue WEBKIT_AVAILABLE_MAC(10_6);
@property BOOL cancelBubble WEBKIT_AVAILABLE_MAC(10_6);
- (void)stopPropagation;
- (void)preventDefault;
- (void)initEvent:(NSString *)eventTypeArg canBubbleArg:(BOOL)canBubbleArg cancelableArg:(BOOL)cancelableArg WEBKIT_AVAILABLE_MAC(10_5);
- (void)initEvent:(NSString *)eventTypeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg;
@end

@interface DOMUIEvent : DOMEvent 10_4
@property (readonly, strong) DOMAbstractView *view;
@property (readonly) int detail;
@property (readonly) int keyCode WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int charCode WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int layerX WEBKIT_DEPRECATED_MAC(10_5, 10_5);
@property (readonly) int layerY WEBKIT_DEPRECATED_MAC(10_5, 10_5);
@property (readonly) int pageX WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int pageY WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int which WEBKIT_AVAILABLE_MAC(10_5);
- (void)initUIEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view detail:(int)detail WEBKIT_AVAILABLE_MAC(10_5);
- (void)initUIEvent:(NSString *)type :(BOOL)canBubble :(BOOL)cancelable :(DOMAbstractView *)view :(int)detail;
@end

@interface DOMMutationEvent : DOMEvent 10_4
@property (readonly, strong) DOMNode *relatedNode;
@property (readonly, copy) NSString *prevValue;
@property (readonly, copy) NSString *newValue;
@property (readonly, copy) NSString *attrName;
@property (readonly) unsigned short attrChange;
- (void)initMutationEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable relatedNode:(DOMNode *)relatedNode prevValue:(NSString *)prevValue newValue:(NSString *)newValue attrName:(NSString *)attrName attrChange:(unsigned short)attrChange WEBKIT_AVAILABLE_MAC(10_5);
- (void)initMutationEvent:(NSString *)type :(BOOL)canBubble :(BOOL)cancelable :(DOMNode *)relatedNode :(NSString *)prevValue :(NSString *)newValue :(NSString *)attrName :(unsigned short)attrChange;
@end

@interface DOMOverflowEvent : DOMEvent 10_5
@property (readonly) unsigned short orient;
@property (readonly) BOOL horizontalOverflow;
@property (readonly) BOOL verticalOverflow;
- (void)initOverflowEvent:(unsigned short)orient horizontalOverflow:(BOOL)horizontalOverflow verticalOverflow:(BOOL)verticalOverflow;
@end

@interface DOMWheelEvent : DOMMouseEvent 10_5
@property (readonly) BOOL isHorizontal;
@property (readonly) int wheelDelta;
@property (readonly) int wheelDeltaX WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int wheelDeltaY WEBKIT_AVAILABLE_MAC(10_5);
- (void)initWheelEvent:(int)wheelDeltaX wheelDeltaY:(int)wheelDeltaY view:(DOMAbstractView *)view screenX:(int)screenX screenY:(int)screenY clientX:(int)clientX clientY:(int)clientY ctrlKey:(BOOL)ctrlKey altKey:(BOOL)altKey shiftKey:(BOOL)shiftKey metaKey:(BOOL)metaKey WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMKeyboardEvent : DOMUIEvent 10_5
@property (readonly, copy) NSString *keyIdentifier;
@property (readonly) unsigned location WEBKIT_AVAILABLE_MAC(10_8);
@property (readonly) unsigned keyLocation WEBKIT_DEPRECATED_MAC(10_5, 10_5);
@property (readonly) BOOL ctrlKey;
@property (readonly) BOOL shiftKey;
@property (readonly) BOOL altKey;
@property (readonly) BOOL metaKey;
@property (readonly) int keyCode;
@property (readonly) int charCode;
@property (readonly) BOOL altGraphKey WEBKIT_AVAILABLE_MAC(10_5);
- (BOOL)getModifierState:(NSString *)keyIdentifierArg;
- (void)initKeyboardEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view keyIdentifier:(NSString *)keyIdentifier keyLocation:(unsigned)keyLocation ctrlKey:(BOOL)ctrlKey altKey:(BOOL)altKey shiftKey:(BOOL)shiftKey metaKey:(BOOL)metaKey altGraphKey:(BOOL)altGraphKey WEBKIT_DEPRECATED_MAC(10_5, 10_5);
- (void)initKeyboardEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view keyIdentifier:(NSString *)keyIdentifier keyLocation:(unsigned)keyLocation ctrlKey:(BOOL)ctrlKey altKey:(BOOL)altKey shiftKey:(BOOL)shiftKey metaKey:(BOOL)metaKey WEBKIT_DEPRECATED_MAC(10_5, 10_5);
- (void)initKeyboardEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view keyIdentifier:(NSString *)keyIdentifier location:(unsigned)location ctrlKey:(BOOL)ctrlKey altKey:(BOOL)altKey shiftKey:(BOOL)shiftKey metaKey:(BOOL)metaKey altGraphKey:(BOOL)altGraphKey WEBKIT_AVAILABLE_MAC(10_8);
- (void)initKeyboardEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view keyIdentifier:(NSString *)keyIdentifier location:(unsigned)location ctrlKey:(BOOL)ctrlKey altKey:(BOOL)altKey shiftKey:(BOOL)shiftKey metaKey:(BOOL)metaKey WEBKIT_AVAILABLE_MAC(10_8);
@end

@interface DOMMouseEvent : DOMUIEvent 10_4
@property (readonly) int screenX;
@property (readonly) int screenY;
@property (readonly) int clientX;
@property (readonly) int clientY;
@property (readonly) BOOL ctrlKey;
@property (readonly) BOOL shiftKey;
@property (readonly) BOOL altKey;
@property (readonly) BOOL metaKey;
@property (readonly) unsigned short button;
@property (readonly, strong) id <DOMEventTarget> relatedTarget;
@property (readonly) int offsetX WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int offsetY WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int x WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) int y WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, strong) DOMNode *fromElement WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly, strong) DOMNode *toElement WEBKIT_AVAILABLE_MAC(10_5);
- (void)initMouseEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view detail:(int)detail screenX:(int)screenX screenY:(int)screenY clientX:(int)clientX clientY:(int)clientY ctrlKey:(BOOL)ctrlKey altKey:(BOOL)altKey shiftKey:(BOOL)shiftKey metaKey:(BOOL)metaKey button:(unsigned short)button relatedTarget:(id <DOMEventTarget>)relatedTarget WEBKIT_AVAILABLE_MAC(10_5);
- (void)initMouseEvent:(NSString *)type :(BOOL)canBubble :(BOOL)cancelable :(DOMAbstractView *)view :(int)detail :(int)screenX :(int)screenY :(int)clientX :(int)clientY :(BOOL)ctrlKey :(BOOL)altKey :(BOOL)shiftKey :(BOOL)metaKey :(unsigned short)button :(id <DOMEventTarget>)relatedTarget;
@end

@interface DOMRange : DOMObject 10_4
@property (readonly, strong) DOMNode *startContainer;
@property (readonly) int startOffset;
@property (readonly, strong) DOMNode *endContainer;
@property (readonly) int endOffset;
@property (readonly) BOOL collapsed;
@property (readonly, strong) DOMNode *commonAncestorContainer;
@property (readonly, copy) NSString *text WEBKIT_AVAILABLE_MAC(10_5);
- (void)setStart:(DOMNode *)refNode offset:(int)offset WEBKIT_AVAILABLE_MAC(10_5);
- (void)setStart:(DOMNode *)refNode :(int)offset;
- (void)setEnd:(DOMNode *)refNode offset:(int)offset WEBKIT_AVAILABLE_MAC(10_5);
- (void)setEnd:(DOMNode *)refNode :(int)offset;
- (void)setStartBefore:(DOMNode *)refNode;
- (void)setStartAfter:(DOMNode *)refNode;
- (void)setEndBefore:(DOMNode *)refNode;
- (void)setEndAfter:(DOMNode *)refNode;
- (void)collapse:(BOOL)toStart;
- (void)selectNode:(DOMNode *)refNode;
- (void)selectNodeContents:(DOMNode *)refNode;
- (short)compareBoundaryPoints:(unsigned short)how sourceRange:(DOMRange *)sourceRange WEBKIT_AVAILABLE_MAC(10_5);
- (short)compareBoundaryPoints:(unsigned short)how :(DOMRange *)sourceRange;
- (void)deleteContents;
- (DOMDocumentFragment *)extractContents;
- (DOMDocumentFragment *)cloneContents;
- (void)insertNode:(DOMNode *)newNode;
- (void)surroundContents:(DOMNode *)newParent;
- (DOMRange *)cloneRange;
- (NSString *)toString;
- (void)detach;
- (DOMDocumentFragment *)createContextualFragment:(NSString *)html WEBKIT_AVAILABLE_MAC(10_5);
- (BOOL)intersectsNode:(DOMNode *)refNode WEBKIT_AVAILABLE_MAC(10_5);
- (short)compareNode:(DOMNode *)refNode WEBKIT_AVAILABLE_MAC(10_5);
- (short)comparePoint:(DOMNode *)refNode offset:(int)offset WEBKIT_AVAILABLE_MAC(10_5);
- (BOOL)isPointInRange:(DOMNode *)refNode offset:(int)offset WEBKIT_AVAILABLE_MAC(10_5);
@end

@interface DOMNodeIterator : DOMObject 10_4
@property (readonly, strong) DOMNode *root;
@property (readonly) unsigned whatToShow;
@property (readonly, strong) id <DOMNodeFilter> filter;
@property (readonly) BOOL expandEntityReferences;
@property (readonly, strong) DOMNode *referenceNode WEBKIT_AVAILABLE_MAC(10_5);
@property (readonly) BOOL pointerBeforeReferenceNode WEBKIT_AVAILABLE_MAC(10_5);
- (DOMNode *)nextNode;
- (DOMNode *)previousNode;
- (void)detach;
@end

@interface DOMMediaList : DOMObject 10_4
@property (copy) NSString *mediaText;
@property (readonly) unsigned length;
- (NSString *)item:(unsigned)index;
- (void)deleteMedium:(NSString *)oldMedium;
- (void)appendMedium:(NSString *)newMedium;
@end

@interface DOMTreeWalker : DOMObject 10_4
@property (readonly, strong) DOMNode *root;
@property (readonly) unsigned whatToShow;
@property (readonly, strong) id <DOMNodeFilter> filter;
@property (readonly) BOOL expandEntityReferences;
@property (strong) DOMNode *currentNode;
- (DOMNode *)parentNode;
- (DOMNode *)firstChild;
- (DOMNode *)lastChild;
- (DOMNode *)previousSibling;
- (DOMNode *)nextSibling;
- (DOMNode *)previousNode;
- (DOMNode *)nextNode;
@end

@interface DOMXPathResult : DOMObject 10_5
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

@interface DOMXPathExpression : DOMObject 10_5
- (DOMXPathResult *)evaluate:(DOMNode *)contextNode type:(unsigned short)type inResult:(DOMXPathResult *)inResult WEBKIT_AVAILABLE_MAC(10_5);
- (DOMXPathResult *)evaluate:(DOMNode *)contextNode :(unsigned short)type :(DOMXPathResult *)inResult WEBKIT_DEPRECATED_MAC(10_5, 10_5);
@end

@interface DOMProgressEvent : DOMEvent 10_6
@property (readonly) BOOL lengthComputable;
@property (readonly) unsigned long long loaded;
@property (readonly) unsigned long long total;
@end

// Protocols

@protocol DOMEventListener <NSObject> 10_4
- (void)handleEvent:(DOMEvent *)evt;
@end

@protocol DOMEventTarget <NSObject, NSCopying> 10_4
- (void)addEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture;
- (void)removeEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture;
- (void)addEventListener:(NSString *)type listener:(id <DOMEventListener>)listener useCapture:(BOOL)useCapture WEBKIT_AVAILABLE_MAC(10_5);
- (void)removeEventListener:(NSString *)type listener:(id <DOMEventListener>)listener useCapture:(BOOL)useCapture WEBKIT_AVAILABLE_MAC(10_5);
- (BOOL)dispatchEvent:(DOMEvent *)event;
@end

@protocol DOMNodeFilter <NSObject> 10_4
- (short)acceptNode:(DOMNode *)n;
@end

@protocol DOMXPathNSResolver <NSObject> 10_5
- (NSString *)lookupNamespaceURI:(NSString *)prefix;
@end

#if TARGET_OS_IPHONE
#include <WebKitAdditions/PublicDOMInterfacesIOS.h>
#endif
