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

#ifndef OBJC_CODE_GENERATION
#error Do not include this header, instead include the appropriate DOM header.
#endif

@interface DOMAttr : DOMNode
@property(readonly, copy) NSString *name;
@property(readonly) BOOL specified;
@property(copy) NSString *value;
@property(readonly, retain) DOMElement *ownerElement;
@property(readonly, retain) DOMCSSStyleDeclaration *style;
@end

@interface DOMCDATASection : DOMText
@end

@interface DOMCharacterData : DOMNode
@property(copy) NSString *data;
@property(readonly) unsigned length;
- (NSString *)substringData:(unsigned)offset :(unsigned)length;
- (NSString *)substringData:(unsigned)offset length:(unsigned)length;
- (void)appendData:(NSString *)data;
- (void)insertData:(unsigned)offset :(NSString *)data;
- (void)deleteData:(unsigned)offset :(unsigned)length;
- (void)replaceData:(unsigned)offset :(unsigned)length :(NSString *)data;
- (void)insertData:(unsigned)offset data:(NSString *)data;
- (void)deleteData:(unsigned)offset length:(unsigned)length;
- (void)replaceData:(unsigned)offset length:(unsigned)length data:(NSString *)data;
@end

@interface DOMComment : DOMCharacterData
@end

@interface DOMImplementation : DOMObject
- (BOOL)hasFeature:(NSString *)feature :(NSString *)version;
- (DOMDocumentType *)createDocumentType:(NSString *)qualifiedName :(NSString *)publicId :(NSString *)systemId;
- (DOMDocument *)createDocument:(NSString *)namespaceURI :(NSString *)qualifiedName :(DOMDocumentType *)doctype;
- (DOMCSSStyleSheet *)createCSSStyleSheet:(NSString *)title :(NSString *)media;
- (BOOL)hasFeature:(NSString *)feature version:(NSString *)version;
- (DOMDocumentType *)createDocumentType:(NSString *)qualifiedName publicId:(NSString *)publicId systemId:(NSString *)systemId;
- (DOMDocument *)createDocument:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName doctype:(DOMDocumentType *)doctype;
- (DOMCSSStyleSheet *)createCSSStyleSheet:(NSString *)title media:(NSString *)media;
- (DOMHTMLDocument *)createHTMLDocument:(NSString *)title;
@end

@interface DOMAbstractView : DOMObject
@property(readonly, retain) DOMDocument *document;
@end

@interface DOMDocument : DOMNode
@property(readonly, retain) DOMDocumentType *doctype;
@property(readonly, retain) DOMImplementation *implementation;
@property(readonly, retain) DOMElement *documentElement;
@property(readonly, retain) DOMAbstractView *defaultView;
@property(readonly, retain) DOMStyleSheetList *styleSheets;
@property(readonly, retain) DOMHTMLCollection *images;
@property(readonly, retain) DOMHTMLCollection *applets;
@property(readonly, retain) DOMHTMLCollection *links;
@property(readonly, retain) DOMHTMLCollection *forms;
@property(readonly, retain) DOMHTMLCollection *anchors;
@property(copy) NSString *title;
@property(readonly, copy) NSString *referrer;
@property(readonly, copy) NSString *domain;
@property(readonly, copy) NSString *URL;
@property(retain) DOMHTMLElement *body;
@property(copy) NSString *cookie;
- (DOMElement *)createElement:(NSString *)tagName;
- (DOMDocumentFragment *)createDocumentFragment;
- (DOMText *)createTextNode:(NSString *)data;
- (DOMComment *)createComment:(NSString *)data;
- (DOMCDATASection *)createCDATASection:(NSString *)data;
- (DOMProcessingInstruction *)createProcessingInstruction:(NSString *)target :(NSString *)data;
- (DOMProcessingInstruction *)createProcessingInstruction:(NSString *)target data:(NSString *)data;
- (DOMAttr *)createAttribute:(NSString *)name;
- (DOMEntityReference *)createEntityReference:(NSString *)name;
- (DOMNodeList *)getElementsByTagName:(NSString *)tagname;
- (DOMNode *)importNode:(DOMNode *)importedNode :(BOOL)deep;
- (DOMElement *)createElementNS:(NSString *)namespaceURI :(NSString *)qualifiedName;
- (DOMAttr *)createAttributeNS:(NSString *)namespaceURI :(NSString *)qualifiedName;
- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMNode *)importNode:(DOMNode *)importedNode deep:(BOOL)deep;
- (DOMNode *)adoptNode:(DOMNode *)source;
- (DOMElement *)createElementNS:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName;
- (DOMAttr *)createAttributeNS:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName;
- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI localName:(NSString *)localName;
- (DOMElement *)getElementById:(NSString *)elementId;
- (DOMEvent *)createEvent:(NSString *)eventType;
- (DOMRange *)createRange;
- (DOMCSSStyleDeclaration *)createCSSStyleDeclaration;
- (DOMCSSStyleDeclaration *)getOverrideStyle:(DOMElement *)element :(NSString *)pseudoElement;
- (DOMCSSStyleDeclaration *)getOverrideStyle:(DOMElement *)element pseudoElement:(NSString *)pseudoElement;
- (DOMCSSStyleDeclaration *)getComputedStyle:(DOMElement *)element :(NSString *)pseudoElement;
- (DOMCSSStyleDeclaration *)getComputedStyle:(DOMElement *)element pseudoElement:(NSString *)pseudoElement;
- (DOMCSSRuleList *)getMatchedCSSRules:(DOMElement *)element pseudoElement:(NSString *)pseudoElement;
- (DOMCSSRuleList *)getMatchedCSSRules:(DOMElement *)element pseudoElement:(NSString *)pseudoElement authorOnly:(BOOL)authorOnly;
- (DOMNodeList *)getElementsByName:(NSString *)elementName;
#ifdef ENABLE_XPATH
- (DOMXPathExpression *)createExpression:(NSString *)expression :(id <DOMXPathNSResolver>)resolver;
- (DOMXPathExpression *)createExpression:(NSString *)expression resolver:(id <DOMXPathNSResolver>)resolver;
- (id <DOMXPathNSResolver>)createNSResolver:(DOMNode *)nodeResolver;
- (DOMXPathResult *)evaluate:(NSString *)expression :(DOMNode *)contextNode :(id <DOMXPathNSResolver>)resolver :(unsigned short)type :(DOMXPathResult *)inResult;
- (DOMXPathResult *)evaluate:(NSString *)expression contextNode:(DOMNode *)contextNode resolver:(id <DOMXPathNSResolver>)resolver type:(unsigned short)type inResult:(DOMXPathResult *)inResult;
#endif
@end

@interface DOMDocumentFragment : DOMNode
@end

@interface DOMDocumentType : DOMNode
@property(readonly, copy) NSString *name;
@property(readonly, retain) DOMNamedNodeMap *entities;
@property(readonly, retain) DOMNamedNodeMap *notations;
@property(readonly, copy) NSString *publicId;
@property(readonly, copy) NSString *systemId;
@property(readonly, copy) NSString *internalSubset;
@end

@interface DOMElement : DOMNode
@property(readonly, copy) NSString *tagName;
@property(readonly, retain) DOMCSSStyleDeclaration *style;
@property(readonly) int offsetLeft;
@property(readonly) int offsetTop;
@property(readonly) int offsetWidth;
@property(readonly) int offsetHeight;
@property(readonly, retain) DOMElement *offsetParent;
@property(readonly) int clientWidth;
@property(readonly) int clientHeight;
@property int scrollLeft;
@property int scrollTop;
@property(readonly) int scrollWidth;
@property(readonly) int scrollHeight;
- (NSString *)getAttribute:(NSString *)name;
- (void)setAttribute:(NSString *)name :(NSString *)value;
- (void)setAttribute:(NSString *)name value:(NSString *)value;
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
- (NSString *)getAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName;
- (void)setAttributeNS:(NSString *)namespaceURI qualifiedName:(NSString *)qualifiedName value:(NSString *)value;
- (void)removeAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName;
- (DOMNodeList *)getElementsByTagNameNS:(NSString *)namespaceURI localName:(NSString *)localName;
- (DOMAttr *)getAttributeNodeNS:(NSString *)namespaceURI localName:(NSString *)localName;
- (DOMAttr *)setAttributeNodeNS:(DOMAttr *)newAttr;
- (BOOL)hasAttribute:(NSString *)name;
- (BOOL)hasAttributeNS:(NSString *)namespaceURI :(NSString *)localName;
- (BOOL)hasAttributeNS:(NSString *)namespaceURI localName:(NSString *)localName;
- (void)focus;
- (void)blur;
- (void)scrollIntoView:(BOOL)alignWithTop;
- (void)scrollIntoViewIfNeeded:(BOOL)centerIfNeeded;
@end

@interface DOMEntity : DOMNode
@property(readonly, copy) NSString *publicId;
@property(readonly, copy) NSString *systemId;
@property(readonly, copy) NSString *notationName;
@end

@interface DOMEntityReference : DOMNode
@end

@interface DOMNamedNodeMap : DOMObject
@property(readonly) unsigned length;
- (DOMNode *)getNamedItem:(NSString *)name;
- (DOMNode *)setNamedItem:(DOMNode *)node;
- (DOMNode *)removeNamedItem:(NSString *)name;
- (DOMNode *)item:(unsigned)index;
- (DOMNode *)getNamedItemNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMNode *)getNamedItemNS:(NSString *)namespaceURI localName:(NSString *)localName;
- (DOMNode *)setNamedItemNS:(DOMNode *)node;
- (DOMNode *)removeNamedItemNS:(NSString *)namespaceURI :(NSString *)localName;
- (DOMNode *)removeNamedItemNS:(NSString *)namespaceURI localName:(NSString *)localName;
@end

@interface DOMNode : DOMObject
@property(readonly, copy) NSString *nodeName;
@property(copy) NSString *nodeValue;
@property(readonly) unsigned short nodeType;
@property(readonly, retain) DOMNode *parentNode;
@property(readonly, retain) DOMNodeList *childNodes;
@property(readonly, retain) DOMNode *firstChild;
@property(readonly, retain) DOMNode *lastChild;
@property(readonly, retain) DOMNode *previousSibling;
@property(readonly, retain) DOMNode *nextSibling;
@property(readonly, retain) DOMNamedNodeMap *attributes;
@property(readonly, retain) DOMDocument *ownerDocument;
@property(readonly, copy) NSString *namespaceURI;
@property(copy) NSString *prefix;
@property(readonly, copy) NSString *localName;
@property(copy) NSString *textContent;
- (DOMNode *)insertBefore:(DOMNode *)newChild :(DOMNode *)refChild;
- (DOMNode *)insertBefore:(DOMNode *)newChild refChild:(DOMNode *)refChild;
- (DOMNode *)replaceChild:(DOMNode *)newChild :(DOMNode *)oldChild;
- (DOMNode *)replaceChild:(DOMNode *)newChild oldChild:(DOMNode *)oldChild;
- (DOMNode *)removeChild:(DOMNode *)oldChild;
- (DOMNode *)appendChild:(DOMNode *)newChild;
- (BOOL)hasChildNodes;
- (DOMNode *)cloneNode:(BOOL)deep;
- (void)normalize;
- (BOOL)isSupported:(NSString *)feature :(NSString *)version;
- (BOOL)isSupported:(NSString *)feature version:(NSString *)version;
- (BOOL)hasAttributes;
- (BOOL)isSameNode:(DOMNode *)other;
- (BOOL)isEqualNode:(DOMNode *)other;
@end

@interface DOMNodeList : DOMObject
@property(readonly) unsigned length;
- (DOMNode *)item:(unsigned)index;
@end

@interface DOMNotation : DOMNode
@property(readonly, copy) NSString *publicId;
@property(readonly, copy) NSString *systemId;
@end

@interface DOMProcessingInstruction : DOMNode
@property(readonly, copy) NSString *target;
@property(copy) NSString *data;
@end

@interface DOMText : DOMCharacterData
- (DOMText *)splitText:(unsigned)offset;
@end

@interface DOMHTMLAnchorElement : DOMHTMLElement
@property(copy) NSString *accessKey;
@property(copy) NSString *charset;
@property(copy) NSString *coords;
@property(copy) NSString *href;
@property(copy) NSString *hreflang;
@property(copy) NSString *name;
@property(copy) NSString *rel;
@property(copy) NSString *rev;
@property(copy) NSString *shape;
@property int tabIndex;
@property(copy) NSString *target;
@property(copy) NSString *type;
@property(readonly, copy) NSURL *absoluteLinkURL;
- (void)blur;
- (void)focus;
@end

@interface DOMHTMLAppletElement : DOMHTMLElement
@property(copy) NSString *align;
@property(copy) NSString *alt;
@property(copy) NSString *archive;
@property(copy) NSString *code;
@property(copy) NSString *codeBase;
@property(copy) NSString *height;
@property int hspace;
@property(copy) NSString *name;
@property(copy) NSString *object;
@property int vspace;
@property(copy) NSString *width;
@end

@interface DOMHTMLAreaElement : DOMHTMLElement
@property(copy) NSString *accessKey;
@property(copy) NSString *alt;
@property(copy) NSString *coords;
@property(copy) NSString *href;
@property BOOL noHref;
@property(copy) NSString *shape;
@property int tabIndex;
@property(copy) NSString *target;
@property(readonly, copy) NSURL *absoluteLinkURL;
@end

@interface DOMHTMLBRElement : DOMHTMLElement
@property(copy) NSString *clear;
@end

@interface DOMHTMLBaseElement : DOMHTMLElement
@property(copy) NSString *href;
@property(copy) NSString *target;
@end

@interface DOMHTMLBaseFontElement : DOMHTMLElement
@property(copy) NSString *color;
@property(copy) NSString *face;
@property(copy) NSString *size;
@end

@interface DOMHTMLBodyElement : DOMHTMLElement
@property(copy) NSString *aLink;
@property(copy) NSString *background;
@property(copy) NSString *bgColor;
@property(copy) NSString *link;
@property(copy) NSString *text;
@property(copy) NSString *vLink;
@end

@interface DOMHTMLButtonElement : DOMHTMLElement
@property(readonly, retain) DOMHTMLFormElement *form;
@property(copy) NSString *accessKey;
@property BOOL disabled;
@property(copy) NSString *name;
@property int tabIndex;
@property(readonly, copy) NSString *type;
@property(copy) NSString *value;
@end

@interface DOMHTMLCanvasElement : DOMHTMLElement
@property int height;
@property int width;
@end

@interface DOMHTMLCollection : DOMObject
@property(readonly) unsigned length;
- (DOMNode *)item:(unsigned)index;
- (DOMNode *)namedItem:(NSString *)name;
@end

@interface DOMHTMLDListElement : DOMHTMLElement
@property BOOL compact;
@end

@interface DOMHTMLDirectoryElement : DOMHTMLElement
@property BOOL compact;
@end

@interface DOMHTMLDivElement : DOMHTMLElement
@property(copy) NSString *align;
@end

@interface DOMHTMLDocument : DOMDocument
- (void)open;
- (void)close;
- (void)write:(NSString *)text;
- (void)writeln:(NSString *)text;
@end

@interface DOMHTMLElement : DOMElement
@property(copy) NSString *title;
@property(copy) NSString *idName;
@property(copy) NSString *lang;
@property(copy) NSString *dir;
@property(copy) NSString *className;
@property(copy) NSString *innerHTML;
@property(copy) NSString *innerText;
@property(copy) NSString *outerHTML;
@property(copy) NSString *outerText;
@property(readonly, retain) DOMHTMLCollection *children;
@property(copy) NSString *contentEditable;
@property(readonly) BOOL isContentEditable;
@property(readonly, copy) NSString *titleDisplayString;
@end

@interface DOMHTMLEmbedElement : DOMHTMLElement
@property(copy) NSString *align;
@property int height;
@property(copy) NSString *name;
@property(copy) NSString *src;
@property(copy) NSString *type;
@property int width;
@end

@interface DOMHTMLFieldSetElement : DOMHTMLElement
@property(readonly, retain) DOMHTMLFormElement *form;
@end

@interface DOMHTMLFontElement : DOMHTMLElement
@property(copy) NSString *color;
@property(copy) NSString *face;
@property(copy) NSString *size;
@end

@interface DOMHTMLFormElement : DOMHTMLElement
@property(readonly, retain) DOMHTMLCollection *elements;
@property(readonly) int length;
@property(copy) NSString *name;
@property(copy) NSString *acceptCharset;
@property(copy) NSString *action;
@property(copy) NSString *enctype;
@property(copy) NSString *method;
@property(copy) NSString *target;
- (void)submit;
- (void)reset;
@end

@interface DOMHTMLFrameElement : DOMHTMLElement
@property(copy) NSString *frameBorder;
@property(copy) NSString *longDesc;
@property(copy) NSString *marginHeight;
@property(copy) NSString *marginWidth;
@property(copy) NSString *name;
@property BOOL noResize;
@property(copy) NSString *scrolling;
@property(copy) NSString *src;
@property(readonly, retain) DOMDocument *contentDocument;
@end

@interface DOMHTMLFrameSetElement : DOMHTMLElement
@property(copy) NSString *cols;
@property(copy) NSString *rows;
@end

@interface DOMHTMLHRElement : DOMHTMLElement
@property(copy) NSString *align;
@property BOOL noShade;
@property(copy) NSString *size;
@property(copy) NSString *width;
@end

@interface DOMHTMLHeadElement : DOMHTMLElement
@property(copy) NSString *profile;
@end

@interface DOMHTMLHeadingElement : DOMHTMLElement
@property(copy) NSString *align;
@end

@interface DOMHTMLHtmlElement : DOMHTMLElement
@property(copy) NSString *version;
@end

@interface DOMHTMLIFrameElement : DOMHTMLElement
@property(copy) NSString *align;
@property(copy) NSString *frameBorder;
@property(copy) NSString *height;
@property(copy) NSString *longDesc;
@property(copy) NSString *marginHeight;
@property(copy) NSString *marginWidth;
@property(copy) NSString *name;
@property(copy) NSString *scrolling;
@property(copy) NSString *src;
@property(copy) NSString *width;
@property(readonly, retain) DOMDocument *contentDocument;
@end

@interface DOMHTMLImageElement : DOMHTMLElement
@property(copy) NSString *name;
@property(copy) NSString *align;
@property(copy) NSString *alt;
@property(copy) NSString *border;
@property int height;
@property int hspace;
@property BOOL isMap;
@property(copy) NSString *longDesc;
@property(copy) NSString *src;
@property(copy) NSString *useMap;
@property int vspace;
@property int width;
@property(readonly, copy) NSString *altDisplayString;
@property(readonly, copy) NSURL *absoluteImageURL;
@end

@interface DOMHTMLInputElement : DOMHTMLElement
@property(copy) NSString *defaultValue;
@property BOOL defaultChecked;
@property(readonly, retain) DOMHTMLFormElement *form;
@property(copy) NSString *accept;
@property(copy) NSString *accessKey;
@property(copy) NSString *align;
@property(copy) NSString *alt;
@property BOOL checked;
@property BOOL disabled;
@property int maxLength;
@property(copy) NSString *name;
@property BOOL readOnly;
@property(copy) NSString *size;
@property(copy) NSString *src;
@property int tabIndex;
@property(copy) NSString *type;
@property(copy) NSString *useMap;
@property(copy) NSString *value;
@property(readonly, copy) NSString *altDisplayString;
@property(readonly, copy) NSURL *absoluteImageURL;
- (void)blur;
- (void)focus;
- (void)select;
- (void)click;
@end

@interface DOMHTMLIsIndexElement : DOMHTMLInputElement
@property(readonly, retain) DOMHTMLFormElement *form;
@property(copy) NSString *prompt;
@end

@interface DOMHTMLLIElement : DOMHTMLElement
@property(copy) NSString *type;
@property int value;
@end

@interface DOMHTMLLabelElement : DOMHTMLElement
@property(readonly, retain) DOMHTMLFormElement *form;
@property(copy) NSString *accessKey;
@property(copy) NSString *htmlFor;
@end

@interface DOMHTMLLegendElement : DOMHTMLElement
@property(readonly, retain) DOMHTMLFormElement *form;
@property(copy) NSString *accessKey;
@property(copy) NSString *align;
@end

@interface DOMHTMLLinkElement : DOMHTMLElement
@property BOOL disabled;
@property(copy) NSString *charset;
@property(copy) NSString *href;
@property(copy) NSString *hreflang;
@property(copy) NSString *media;
@property(copy) NSString *rel;
@property(copy) NSString *rev;
@property(copy) NSString *target;
@property(copy) NSString *type;
@property(readonly, copy) NSURL *absoluteLinkURL;
@end

@interface DOMHTMLMapElement : DOMHTMLElement
@property(readonly, retain) DOMHTMLCollection *areas;
@property(copy) NSString *name;
@end

@interface DOMHTMLMarqueeElement : DOMHTMLElement
- (void)start;
- (void)stop;
@end

@interface DOMHTMLMenuElement : DOMHTMLElement
@property BOOL compact;
@end

@interface DOMHTMLMetaElement : DOMHTMLElement
@property(copy) NSString *content;
@property(copy) NSString *httpEquiv;
@property(copy) NSString *name;
@property(copy) NSString *scheme;
@end

@interface DOMHTMLModElement : DOMHTMLElement
@property(copy) NSString *cite;
@property(copy) NSString *dateTime;
@end

@interface DOMHTMLOListElement : DOMHTMLElement
@property BOOL compact;
@property int start;
@property(copy) NSString *type;
@end

@interface DOMHTMLObjectElement : DOMHTMLElement
@property(readonly, retain) DOMHTMLFormElement *form;
@property(copy) NSString *code;
@property(copy) NSString *align;
@property(copy) NSString *archive;
@property(copy) NSString *border;
@property(copy) NSString *codeBase;
@property(copy) NSString *codeType;
@property(copy) NSString *data;
@property BOOL declare;
@property(copy) NSString *height;
@property int hspace;
@property(copy) NSString *name;
@property(copy) NSString *standby;
@property int tabIndex;
@property(copy) NSString *type;
@property(copy) NSString *useMap;
@property int vspace;
@property(copy) NSString *width;
@property(readonly, retain) DOMDocument *contentDocument;
@property(readonly, copy) NSURL *absoluteImageURL;
@end

@interface DOMHTMLOptGroupElement : DOMHTMLElement
@property BOOL disabled;
@property(copy) NSString *label;
@end

@interface DOMHTMLOptionElement : DOMHTMLElement
@property(readonly, retain) DOMHTMLFormElement *form;
@property BOOL defaultSelected;
@property(readonly, copy) NSString *text;
@property(readonly) int index;
@property BOOL disabled;
@property(copy) NSString *label;
@property BOOL selected;
@property(copy) NSString *value;
@end

@interface DOMHTMLOptionsCollection : DOMObject
@property unsigned length;
- (DOMNode *)item:(unsigned)index;
- (DOMNode *)namedItem:(NSString *)name;
@end

@interface DOMHTMLParagraphElement : DOMHTMLElement
@property(copy) NSString *align;
@end

@interface DOMHTMLParamElement : DOMHTMLElement
@property(copy) NSString *name;
@property(copy) NSString *type;
@property(copy) NSString *value;
@property(copy) NSString *valueType;
@end

@interface DOMHTMLPreElement : DOMHTMLElement
@property int width;
@end

@interface DOMHTMLQuoteElement : DOMHTMLElement
@property(copy) NSString *cite;
@end

@interface DOMHTMLScriptElement : DOMHTMLElement
@property(copy) NSString *text;
@property(copy) NSString *htmlFor;
@property(copy) NSString *event;
@property(copy) NSString *charset;
@property BOOL defer;
@property(copy) NSString *src;
@property(copy) NSString *type;
@end

@interface DOMHTMLSelectElement : DOMHTMLElement
@property(readonly, copy) NSString *type;
@property int selectedIndex;
@property(copy) NSString *value;
@property(readonly) int length;
@property(readonly, retain) DOMHTMLFormElement *form;
@property(readonly, retain) DOMHTMLOptionsCollection *options;
@property BOOL disabled;
@property BOOL multiple;
@property(copy) NSString *name;
@property int size;
@property int tabIndex;
- (void)add:(DOMHTMLElement *)element :(DOMHTMLElement *)before;
- (void)add:(DOMHTMLElement *)element before:(DOMHTMLElement *)before;
- (void)remove:(int)index;
- (void)blur;
- (void)focus;
@end

@interface DOMHTMLStyleElement : DOMHTMLElement
@property BOOL disabled;
@property(copy) NSString *media;
@property(copy) NSString *type;
@end

@interface DOMHTMLTableCaptionElement : DOMHTMLElement
@property(copy) NSString *align;
@end

@interface DOMHTMLTableCellElement : DOMHTMLElement
@property(readonly) int cellIndex;
@property(copy) NSString *abbr;
@property(copy) NSString *align;
@property(copy) NSString *axis;
@property(copy) NSString *bgColor;
@property(copy) NSString *ch;
@property(copy) NSString *chOff;
@property int colSpan;
@property(copy) NSString *headers;
@property(copy) NSString *height;
@property BOOL noWrap;
@property int rowSpan;
@property(copy) NSString *scope;
@property(copy) NSString *vAlign;
@property(copy) NSString *width;
@end

@interface DOMHTMLTableColElement : DOMHTMLElement
@property(copy) NSString *align;
@property(copy) NSString *ch;
@property(copy) NSString *chOff;
@property int span;
@property(copy) NSString *vAlign;
@property(copy) NSString *width;
@end

@interface DOMHTMLTableElement : DOMHTMLElement
@property(retain) DOMHTMLTableCaptionElement *caption;
@property(retain) DOMHTMLTableSectionElement *tHead;
@property(retain) DOMHTMLTableSectionElement *tFoot;
@property(readonly, retain) DOMHTMLCollection *rows;
@property(readonly, retain) DOMHTMLCollection *tBodies;
@property(copy) NSString *align;
@property(copy) NSString *bgColor;
@property(copy) NSString *border;
@property(copy) NSString *cellPadding;
@property(copy) NSString *cellSpacing;
@property(copy) NSString *frameBorders;
@property(copy) NSString *rules;
@property(copy) NSString *summary;
@property(copy) NSString *width;
- (DOMHTMLElement *)createTHead;
- (void)deleteTHead;
- (DOMHTMLElement *)createTFoot;
- (void)deleteTFoot;
- (DOMHTMLElement *)createCaption;
- (void)deleteCaption;
- (DOMHTMLElement *)insertRow:(int)index;
- (void)deleteRow:(int)index;
@end

@interface DOMHTMLTableRowElement : DOMHTMLElement
@property(readonly) int rowIndex;
@property(readonly) int sectionRowIndex;
@property(readonly, retain) DOMHTMLCollection *cells;
@property(copy) NSString *align;
@property(copy) NSString *bgColor;
@property(copy) NSString *ch;
@property(copy) NSString *chOff;
@property(copy) NSString *vAlign;
- (DOMHTMLElement *)insertCell:(int)index;
- (void)deleteCell:(int)index;
@end

@interface DOMHTMLTableSectionElement : DOMHTMLElement
@property(copy) NSString *align;
@property(copy) NSString *ch;
@property(copy) NSString *chOff;
@property(copy) NSString *vAlign;
@property(readonly, retain) DOMHTMLCollection *rows;
- (DOMHTMLElement *)insertRow:(int)index;
- (void)deleteRow:(int)index;
@end

@interface DOMHTMLTextAreaElement : DOMHTMLElement
@property(copy) NSString *defaultValue;
@property(readonly, retain) DOMHTMLFormElement *form;
@property(copy) NSString *accessKey;
@property int cols;
@property BOOL disabled;
@property(copy) NSString *name;
@property BOOL readOnly;
@property int rows;
@property int tabIndex;
@property(readonly, copy) NSString *type;
@property(copy) NSString *value;
- (void)blur;
- (void)focus;
- (void)select;
@end

@interface DOMHTMLTitleElement : DOMHTMLElement
@property(copy) NSString *text;
@end

@interface DOMHTMLUListElement : DOMHTMLElement
@property BOOL compact;
@property(copy) NSString *type;
@end

@interface DOMStyleSheetList : DOMObject
@property(readonly) unsigned length;
- (DOMStyleSheet *)item:(unsigned)index;
@end

@interface DOMCSSCharsetRule : DOMCSSRule
@property(readonly, copy) NSString *encoding;
@end

@interface DOMCSSFontFaceRule : DOMCSSRule
@property(readonly, retain) DOMCSSStyleDeclaration *style;
@end

@interface DOMCSSImportRule : DOMCSSRule
@property(readonly, copy) NSString *href;
@property(readonly, retain) DOMMediaList *media;
@property(readonly, retain) DOMCSSStyleSheet *styleSheet;
@end

@interface DOMCSSMediaRule : DOMCSSRule
@property(readonly, retain) DOMMediaList *media;
@property(readonly, retain) DOMCSSRuleList *cssRules;
- (unsigned)insertRule:(NSString *)rule :(unsigned)index;
- (unsigned)insertRule:(NSString *)rule index:(unsigned)index;
- (void)deleteRule:(unsigned)index;
@end

@interface DOMCSSPageRule : DOMCSSRule
@property(copy) NSString *selectorText;
@property(readonly, retain) DOMCSSStyleDeclaration *style;
@end

@interface DOMCSSPrimitiveValue : DOMCSSValue
@property(readonly) unsigned short primitiveType;
- (void)setFloatValue:(unsigned short)unitType :(float)floatValue;
- (void)setFloatValue:(unsigned short)unitType floatValue:(float)floatValue;
- (float)getFloatValue:(unsigned short)unitType;
- (void)setStringValue:(unsigned short)stringType :(NSString *)stringValue;
- (void)setStringValue:(unsigned short)stringType stringValue:(NSString *)stringValue;
- (NSString *)getStringValue;
- (DOMCounter *)getCounterValue;
- (DOMRect *)getRectValue;
- (DOMRGBColor *)getRGBColorValue;
@end

@interface DOMRGBColor : DOMObject
@property(readonly, retain) DOMCSSPrimitiveValue *red;
@property(readonly, retain) DOMCSSPrimitiveValue *green;
@property(readonly, retain) DOMCSSPrimitiveValue *blue;
@property(readonly, retain) DOMCSSPrimitiveValue *alpha;
@property(readonly, copy) NSColor *color;
@end

@interface DOMCSSRule : DOMObject
@property(readonly) unsigned short type;
@property(copy) NSString *cssText;
@property(readonly, retain) DOMCSSStyleSheet *parentStyleSheet;
@property(readonly, retain) DOMCSSRule *parentRule;
@end

@interface DOMCSSRuleList : DOMObject
@property(readonly) unsigned length;
- (DOMCSSRule *)item:(unsigned)index;
@end

@interface DOMCSSStyleDeclaration : DOMObject
@property(copy) NSString *cssText;
@property(readonly) unsigned length;
@property(readonly, retain) DOMCSSRule *parentRule;
- (NSString *)getPropertyValue:(NSString *)propertyName;
- (DOMCSSValue *)getPropertyCSSValue:(NSString *)propertyName;
- (NSString *)removeProperty:(NSString *)propertyName;
- (NSString *)getPropertyPriority:(NSString *)propertyName;
- (void)setProperty:(NSString *)propertyName :(NSString *)value :(NSString *)priority;
- (void)setProperty:(NSString *)propertyName value:(NSString *)value priority:(NSString *)priority;
- (NSString *)item:(unsigned)index;
- (NSString *)getPropertyShorthand:(NSString *)propertyName;
- (BOOL)isPropertyImplicit:(NSString *)propertyName;
@end

@interface DOMCSSStyleRule : DOMCSSRule
@property(copy) NSString *selectorText;
@property(readonly, retain) DOMCSSStyleDeclaration *style;
@end

@interface DOMStyleSheet : DOMObject
@property(readonly, copy) NSString *type;
@property BOOL disabled;
@property(readonly, retain) DOMNode *ownerNode;
@property(readonly, retain) DOMStyleSheet *parentStyleSheet;
@property(readonly, copy) NSString *href;
@property(readonly, copy) NSString *title;
@property(readonly, retain) DOMMediaList *media;
@end

@interface DOMCSSStyleSheet : DOMStyleSheet
@property(readonly, retain) DOMCSSRule *ownerRule;
@property(readonly, retain) DOMCSSRuleList *cssRules;
- (unsigned)insertRule:(NSString *)rule :(unsigned)index;
- (unsigned)insertRule:(NSString *)rule index:(unsigned)index;
- (void)deleteRule:(unsigned)index;
@end

@interface DOMCSSValue : DOMObject
@property(copy) NSString *cssText;
@property(readonly) unsigned short cssValueType;
@end

@interface DOMCSSValueList : DOMCSSValue
@property(readonly) unsigned length;
- (DOMCSSValue *)item:(unsigned)index;
@end

@interface DOMCSSUnknownRule : DOMCSSRule
@end

@interface DOMCounter : DOMObject
@property(readonly, copy) NSString *identifier;
@property(readonly, copy) NSString *listStyle;
@property(readonly, copy) NSString *separator;
@end

@interface DOMRect : DOMObject
@property(readonly, retain) DOMCSSPrimitiveValue *top;
@property(readonly, retain) DOMCSSPrimitiveValue *right;
@property(readonly, retain) DOMCSSPrimitiveValue *bottom;
@property(readonly, retain) DOMCSSPrimitiveValue *left;
@end

@interface DOMEvent : DOMObject
@property(readonly, copy) NSString *type;
@property(readonly, retain) id <DOMEventTarget> target;
@property(readonly, retain) id <DOMEventTarget> currentTarget;
@property(readonly) unsigned short eventPhase;
@property(readonly) BOOL bubbles;
@property(readonly) BOOL cancelable;
@property(readonly) DOMTimeStamp timeStamp;
- (void)stopPropagation;
- (void)preventDefault;
- (void)initEvent:(NSString *)eventTypeArg canBubbleArg:(BOOL)canBubbleArg cancelableArg:(BOOL)cancelableArg;
- (void)initEvent:(NSString *)eventTypeArg :(BOOL)canBubbleArg :(BOOL)cancelableArg;
@end

@interface DOMUIEvent : DOMEvent
@property(readonly, retain) DOMAbstractView *view;
@property(readonly) int detail;
- (void)initUIEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view detail:(int)detail;
- (void)initUIEvent:(NSString *)type :(BOOL)canBubble :(BOOL)cancelable :(DOMAbstractView *)view :(int)detail;
@end

@interface DOMMutationEvent : DOMEvent
@property(readonly, retain) DOMNode *relatedNode;
@property(readonly, copy) NSString *prevValue;
@property(readonly, copy) NSString *newValue;
@property(readonly, copy) NSString *attrName;
@property(readonly) unsigned short attrChange;
- (void)initMutationEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable relatedNode:(DOMNode *)relatedNode prevValue:(NSString *)prevValue newValue:(NSString *)newValue attrName:(NSString *)attrName attrChange:(unsigned short)attrChange;
- (void)initMutationEvent:(NSString *)type :(BOOL)canBubble :(BOOL)cancelable :(DOMNode *)relatedNode :(NSString *)prevValue :(NSString *)newValue :(NSString *)attrName :(unsigned short)attrChange;
@end

@interface DOMOverflowEvent : DOMEvent
@property(readonly) unsigned short orient;
@property(readonly) BOOL horizontalOverflow;
@property(readonly) BOOL verticalOverflow;
- (void)initOverflowEvent:(unsigned short)orient horizontalOverflow:(BOOL)horizontalOverflow verticalOverflow:(BOOL)verticalOverflow;
@end

@interface DOMWheelEvent : DOMUIEvent
@property(readonly) int screenX;
@property(readonly) int screenY;
@property(readonly) int clientX;
@property(readonly) int clientY;
@property(readonly) BOOL ctrlKey;
@property(readonly) BOOL shiftKey;
@property(readonly) BOOL altKey;
@property(readonly) BOOL metaKey;
@property(readonly) BOOL isHorizontal;
@property(readonly) int wheelDelta;
@end

@interface DOMKeyboardEvent : DOMUIEvent
@property(readonly, copy) NSString *keyIdentifier;
@property(readonly) unsigned keyLocation;
@property(readonly) BOOL ctrlKey;
@property(readonly) BOOL shiftKey;
@property(readonly) BOOL altKey;
@property(readonly) BOOL metaKey;
@property(readonly) int keyCode;
@property(readonly) int charCode;
- (BOOL)getModifierState:(NSString *)keyIdentifierArg;
@end

@interface DOMMouseEvent : DOMUIEvent
@property(readonly) int screenX;
@property(readonly) int screenY;
@property(readonly) int clientX;
@property(readonly) int clientY;
@property(readonly) BOOL ctrlKey;
@property(readonly) BOOL shiftKey;
@property(readonly) BOOL altKey;
@property(readonly) BOOL metaKey;
@property(readonly) unsigned short button;
@property(readonly, retain) id <DOMEventTarget> relatedTarget;
- (void)initMouseEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view detail:(int)detail screenX:(int)screenX screenY:(int)screenY clientX:(int)clientX clientY:(int)clientY ctrlKey:(BOOL)ctrlKey altKey:(BOOL)altKey shiftKey:(BOOL)shiftKey metaKey:(BOOL)metaKey button:(unsigned short)button relatedTarget:(id <DOMEventTarget>)relatedTarget;
- (void)initMouseEvent:(NSString *)type :(BOOL)canBubble :(BOOL)cancelable :(DOMAbstractView *)view :(int)detail :(int)screenX :(int)screenY :(int)clientX :(int)clientY :(BOOL)ctrlKey :(BOOL)altKey :(BOOL)shiftKey :(BOOL)metaKey :(unsigned short)button :(id <DOMEventTarget>)relatedTarget;
@end

@interface DOMRange : DOMObject
@property(readonly, retain) DOMNode *startContainer;
@property(readonly) int startOffset;
@property(readonly, retain) DOMNode *endContainer;
@property(readonly) int endOffset;
@property(readonly) BOOL collapsed;
@property(readonly, retain) DOMNode *commonAncestorContainer;
@property(readonly, copy) NSString *text;
- (void)setStart:(DOMNode *)refNode offset:(int)offset;
- (void)setStart:(DOMNode *)refNode :(int)offset;
- (void)setEnd:(DOMNode *)refNode offset:(int)offset;
- (void)setEnd:(DOMNode *)refNode :(int)offset;
- (void)setStartBefore:(DOMNode *)refNode;
- (void)setStartAfter:(DOMNode *)refNode;
- (void)setEndBefore:(DOMNode *)refNode;
- (void)setEndAfter:(DOMNode *)refNode;
- (void)collapse:(BOOL)toStart;
- (void)selectNode:(DOMNode *)refNode;
- (void)selectNodeContents:(DOMNode *)refNode;
- (short)compareBoundaryPoints:(unsigned short)how sourceRange:(DOMRange *)sourceRange;
- (short)compareBoundaryPoints:(unsigned short)how :(DOMRange *)sourceRange;
- (void)deleteContents;
- (DOMDocumentFragment *)extractContents;
- (DOMDocumentFragment *)cloneContents;
- (void)insertNode:(DOMNode *)newNode;
- (void)surroundContents:(DOMNode *)newParent;
- (DOMRange *)cloneRange;
- (NSString *)toString;
- (void)detach;
@end

@interface DOMNodeIterator : DOMObject
@property(readonly, retain) DOMNode *root;
@property(readonly) unsigned whatToShow;
@property(readonly, retain) id <DOMNodeFilter> filter;
@property(readonly) BOOL expandEntityReferences;
- (DOMNode *)nextNode;
- (DOMNode *)previousNode;
- (void)detach;
@end

@interface DOMMediaList : DOMObject
@property(copy) NSString *mediaText;
@property(readonly) unsigned length;
- (NSString *)item:(unsigned)index;
- (void)deleteMedium:(NSString *)oldMedium;
- (void)appendMedium:(NSString *)newMedium;
@end

@interface DOMTreeWalker : DOMObject
@property(readonly, retain) DOMNode *root;
@property(readonly) unsigned whatToShow;
@property(readonly, retain) id <DOMNodeFilter> filter;
@property(readonly) BOOL expandEntityReferences;
@property(retain) DOMNode *currentNode;
- (DOMNode *)parentNode;
- (DOMNode *)firstChild;
- (DOMNode *)lastChild;
- (DOMNode *)previousSibling;
- (DOMNode *)nextSibling;
- (DOMNode *)previousNode;
- (DOMNode *)nextNode;
@end

@interface DOMXPathResult : DOMObject
@property(readonly) unsigned short resultType;
@property(readonly) double numberValue;
@property(readonly, copy) NSString *stringValue;
@property(readonly) BOOL booleanValue;
@property(readonly, retain) DOMNode *singleNodeValue;
@property(readonly) BOOL invalidIteratorState;
@property(readonly) unsigned snapshotLength;
- (DOMNode *)iterateNext;
- (DOMNode *)snapshotItem:(unsigned)index;
@end

@interface DOMXPathExpression : DOMObject
// We should be able to remove the old style version of this method
// once we can verify that all the internal Apple usages have switched
// to the new style, because this has never been in a public release.
- (DOMXPathResult *)evaluate:(DOMNode *)contextNode type:(unsigned short)type inResult:(DOMXPathResult *)inResult;
- (DOMXPathResult *)evaluate:(DOMNode *)contextNode :(unsigned short)type :(DOMXPathResult *)inResult;
@end

// Protocols

@protocol DOMEventListener <NSObject>
- (void)handleEvent:(DOMEvent *)evt;
@end

@protocol DOMEventTarget <NSObject, NSCopying>
- (void)addEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture;
- (void)removeEventListener:(NSString *)type :(id <DOMEventListener>)listener :(BOOL)useCapture;
- (void)addEventListener:(NSString *)type listener:(id <DOMEventListener>)listener useCapture:(BOOL)useCapture;
- (void)removeEventListener:(NSString *)type listener:(id <DOMEventListener>)listener useCapture:(BOOL)useCapture;
- (BOOL)dispatchEvent:(DOMEvent *)event;
@end

@protocol DOMNodeFilter <NSObject>
- (short)acceptNode:(DOMNode *)n;
@end

@protocol DOMXPathNSResolver <NSObject>
- (NSString *)lookupNamespaceURI:(NSString *)prefix;
@end
