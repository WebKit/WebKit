// Copyright (C) 2004-2006 Apple Computer, Inc.  All rights reserved.
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
@property(readonly) NSString *name;
@property(readonly) BOOL specified;
@property NSString *value;
@property(readonly) DOMElement *ownerElement;
@property(readonly) DOMCSSStyleDeclaration *style;
@end

@interface DOMCDATASection : DOMText
@end

@interface DOMCharacterData : DOMNode
@property NSString *data;
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

@interface DOMDocument : DOMNode
@property(readonly) DOMDocumentType *doctype;
@property(readonly) DOMImplementation *implementation;
@property(readonly) DOMElement *documentElement;
@property(readonly) DOMAbstractView *defaultView;
@property(readonly) DOMStyleSheetList *styleSheets;
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
- (DOMCSSStyleDeclaration *)getComputedStyle:(DOMElement *)element pseudoElement:(NSString *)pseudoElement;
- (DOMCSSRuleList *)getMatchedCSSRules:(DOMElement *)element pseudoElement:(NSString *)pseudoElement;
- (DOMCSSRuleList *)getMatchedCSSRules:(DOMElement *)element pseudoElement:(NSString *)pseudoElement authorOnly:(BOOL)authorOnly;
#ifdef XPATH_SUPPORT
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
@property(readonly) NSString *name;
@property(readonly) DOMNamedNodeMap *entities;
@property(readonly) DOMNamedNodeMap *notations;
@property(readonly) NSString *publicId;
@property(readonly) NSString *systemId;
@property(readonly) NSString *internalSubset;
@end

@interface DOMElement : DOMNode
@property(readonly) NSString *tagName;
@property(readonly) DOMCSSStyleDeclaration *style;
@property(readonly) int offsetLeft;
@property(readonly) int offsetTop;
@property(readonly) int offsetWidth;
@property(readonly) int offsetHeight;
@property(readonly) DOMElement *offsetParent;
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
@property(readonly) NSString *publicId;
@property(readonly) NSString *systemId;
@property(readonly) NSString *notationName;
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
@property(readonly) NSString *nodeName;
@property NSString *nodeValue;
@property(readonly) unsigned short nodeType;
@property(readonly) DOMNode *parentNode;
@property(readonly) DOMNodeList *childNodes;
@property(readonly) DOMNode *firstChild;
@property(readonly) DOMNode *lastChild;
@property(readonly) DOMNode *previousSibling;
@property(readonly) DOMNode *nextSibling;
@property(readonly) DOMNamedNodeMap *attributes;
@property(readonly) DOMDocument *ownerDocument;
@property(readonly) NSString *namespaceURI;
@property NSString *prefix;
@property(readonly) NSString *localName;
@property NSString *textContent;
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
@property(readonly) NSString *publicId;
@property(readonly) NSString *systemId;
@end

@interface DOMProcessingInstruction : DOMNode
@property(readonly) NSString *target;
@property NSString *data;
@end

@interface DOMText : DOMCharacterData
- (DOMText *)splitText:(unsigned)offset;
@end

@interface DOMHTMLAnchorElement : DOMHTMLElement
@property NSString *accessKey;
@property NSString *charset;
@property NSString *coords;
@property NSString *href;
@property NSString *hreflang;
@property NSString *name;
@property NSString *rel;
@property NSString *rev;
@property NSString *shape;
@property int tabIndex;
@property NSString *target;
@property NSString *type;
@property(readonly) NSURL *absoluteLinkURL;
- (void)blur;
- (void)focus;
@end

@interface DOMHTMLAppletElement : DOMHTMLElement
@property NSString *align;
@property NSString *alt;
@property NSString *archive;
@property NSString *code;
@property NSString *codeBase;
@property NSString *height;
@property int hspace;
@property NSString *name;
@property NSString *object;
@property int vspace;
@property NSString *width;
@end

@interface DOMHTMLAreaElement : DOMHTMLElement
@property NSString *accessKey;
@property NSString *alt;
@property NSString *coords;
@property NSString *href;
@property BOOL noHref;
@property NSString *shape;
@property int tabIndex;
@property NSString *target;
@property(readonly) NSURL *absoluteLinkURL;
@end

@interface DOMHTMLBRElement : DOMHTMLElement
@property NSString *clear;
@end

@interface DOMHTMLBaseElement : DOMHTMLElement
@property NSString *href;
@property NSString *target;
@end

@interface DOMHTMLBaseFontElement : DOMHTMLElement
@property NSString *color;
@property NSString *face;
@property int size;
@end

@interface DOMHTMLBodyElement : DOMHTMLElement
@property NSString *aLink;
@property NSString *background;
@property NSString *bgColor;
@property NSString *link;
@property NSString *text;
@property NSString *vLink;
@end

@interface DOMHTMLButtonElement : DOMHTMLElement
@property(readonly) DOMHTMLFormElement *form;
@property NSString *accessKey;
@property BOOL disabled;
@property NSString *name;
@property int tabIndex;
@property(readonly) NSString *type;
@property NSString *value;
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
@property NSString *align;
@end

@interface DOMHTMLDocument : DOMDocument
@property NSString *title;
@property(readonly) NSString *referrer;
@property(readonly) NSString *domain;
@property(readonly) NSString *URL;
@property DOMHTMLElement *body;
@property(readonly) DOMHTMLCollection *images;
@property(readonly) DOMHTMLCollection *applets;
@property(readonly) DOMHTMLCollection *links;
@property(readonly) DOMHTMLCollection *forms;
@property(readonly) DOMHTMLCollection *anchors;
@property NSString *cookie;
- (void)open;
- (void)close;
- (void)write:(NSString *)text;
- (void)writeln:(NSString *)text;
- (DOMElement *)getElementById:(NSString *)elementId;
- (DOMNodeList *)getElementsByName:(NSString *)elementName;
@end

@interface DOMHTMLElement : DOMElement
@property NSString *title;
@property NSString *idName;
@property NSString *lang;
@property NSString *dir;
@property NSString *className;
@property NSString *innerHTML;
@property NSString *innerText;
@property NSString *outerHTML;
@property NSString *outerText;
@property(readonly) DOMHTMLCollection *children;
@property NSString *contentEditable;
@property(readonly) BOOL isContentEditable;
@property(readonly) NSString *titleDisplayString;
@end

@interface DOMHTMLEmbedElement : DOMHTMLElement
@property NSString *align;
@property int height;
@property NSString *name;
@property NSString *src;
@property NSString *type;
@property int width;
@end

@interface DOMHTMLFieldSetElement : DOMHTMLElement
@property(readonly) DOMHTMLFormElement *form;
@end

@interface DOMHTMLFontElement : DOMHTMLElement
@property NSString *color;
@property NSString *face;
@property NSString *size;
@end

@interface DOMHTMLFormElement : DOMHTMLElement
@property(readonly) DOMHTMLCollection *elements;
@property(readonly) int length;
@property NSString *name;
@property NSString *acceptCharset;
@property NSString *action;
@property NSString *enctype;
@property NSString *method;
@property NSString *target;
- (void)submit;
- (void)reset;
@end

@interface DOMHTMLFrameElement : DOMHTMLElement
@property NSString *frameBorder;
@property NSString *longDesc;
@property NSString *marginHeight;
@property NSString *marginWidth;
@property NSString *name;
@property BOOL noResize;
@property NSString *scrolling;
@property NSString *src;
@property(readonly) DOMDocument *contentDocument;
@end

@interface DOMHTMLFrameSetElement : DOMHTMLElement
@property NSString *cols;
@property NSString *rows;
@end

@interface DOMHTMLHRElement : DOMHTMLElement
@property NSString *align;
@property BOOL noShade;
@property NSString *size;
@property NSString *width;
@end

@interface DOMHTMLHeadElement : DOMHTMLElement
@property NSString *profile;
@end

@interface DOMHTMLHeadingElement : DOMHTMLElement
@property NSString *align;
@end

@interface DOMHTMLHtmlElement : DOMHTMLElement
@property NSString *version;
@end

@interface DOMHTMLIFrameElement : DOMHTMLElement
@property NSString *align;
@property NSString *frameBorder;
@property NSString *height;
@property NSString *longDesc;
@property NSString *marginHeight;
@property NSString *marginWidth;
@property NSString *name;
@property NSString *scrolling;
@property NSString *src;
@property NSString *width;
@property(readonly) DOMDocument *contentDocument;
@end

@interface DOMHTMLImageElement : DOMHTMLElement
@property NSString *name;
@property NSString *align;
@property NSString *alt;
@property NSString *border;
@property int height;
@property int hspace;
@property BOOL isMap;
@property NSString *longDesc;
@property NSString *src;
@property NSString *useMap;
@property int vspace;
@property int width;
@property(readonly) NSString *altDisplayString;
@property(readonly) NSURL *absoluteImageURL;
@end

@interface DOMHTMLInputElement : DOMHTMLElement
@property NSString *defaultValue;
@property BOOL defaultChecked;
@property(readonly) DOMHTMLFormElement *form;
@property NSString *accept;
@property NSString *accessKey;
@property NSString *align;
@property NSString *alt;
@property BOOL checked;
@property BOOL disabled;
@property int maxLength;
@property NSString *name;
@property BOOL readOnly;
@property unsigned size;
@property NSString *src;
@property int tabIndex;
@property NSString *type;
@property NSString *useMap;
@property NSString *value;
@property(readonly) NSString *altDisplayString;
@property(readonly) NSURL *absoluteImageURL;
- (void)blur;
- (void)focus;
- (void)select;
- (void)click;
@end

@interface DOMHTMLIsIndexElement : DOMHTMLInputElement
@property(readonly) DOMHTMLFormElement *form;
@property NSString *prompt;
@end

@interface DOMHTMLLIElement : DOMHTMLElement
@property NSString *type;
@property int value;
@end

@interface DOMHTMLLabelElement : DOMHTMLElement
@property(readonly) DOMHTMLFormElement *form;
@property NSString *accessKey;
@property NSString *htmlFor;
@end

@interface DOMHTMLLegendElement : DOMHTMLElement
@property(readonly) DOMHTMLFormElement *form;
@property NSString *accessKey;
@property NSString *align;
@end

@interface DOMHTMLLinkElement : DOMHTMLElement
@property BOOL disabled;
@property NSString *charset;
@property NSString *href;
@property NSString *hreflang;
@property NSString *media;
@property NSString *rel;
@property NSString *rev;
@property NSString *target;
@property NSString *type;
@property(readonly) NSURL *absoluteLinkURL;
@end

@interface DOMHTMLMapElement : DOMHTMLElement
@property(readonly) DOMHTMLCollection *areas;
@property NSString *name;
@end

@interface DOMHTMLMenuElement : DOMHTMLElement
@property BOOL compact;
@end

@interface DOMHTMLMetaElement : DOMHTMLElement
@property NSString *content;
@property NSString *httpEquiv;
@property NSString *name;
@property NSString *scheme;
@end

@interface DOMHTMLModElement : DOMHTMLElement
@property NSString *cite;
@property NSString *dateTime;
@end

@interface DOMHTMLOListElement : DOMHTMLElement
@property BOOL compact;
@property int start;
@property NSString *type;
@end

@interface DOMHTMLObjectElement : DOMHTMLElement
@property(readonly) DOMHTMLFormElement *form;
@property NSString *code;
@property NSString *align;
@property NSString *archive;
@property NSString *border;
@property NSString *codeBase;
@property NSString *codeType;
@property NSString *data;
@property BOOL declare;
@property NSString *height;
@property int hspace;
@property NSString *name;
@property NSString *standby;
@property int tabIndex;
@property NSString *type;
@property NSString *useMap;
@property int vspace;
@property NSString *width;
@property(readonly) DOMDocument *contentDocument;
@property(readonly) NSURL *absoluteImageURL;
@end

@interface DOMHTMLOptGroupElement : DOMHTMLElement
@property BOOL disabled;
@property NSString *label;
@end

@interface DOMHTMLOptionElement : DOMHTMLElement
@property(readonly) DOMHTMLFormElement *form;
@property BOOL defaultSelected;
@property(readonly) NSString *text;
@property(readonly) int index;
@property BOOL disabled;
@property NSString *label;
@property BOOL selected;
@property NSString *value;
@end

@interface DOMHTMLOptionsCollection : DOMObject
@property int length;
- (DOMNode *)item:(unsigned)index;
- (DOMNode *)namedItem:(NSString *)name;
@end

@interface DOMHTMLParagraphElement : DOMHTMLElement
@property NSString *align;
@end

@interface DOMHTMLParamElement : DOMHTMLElement
@property NSString *name;
@property NSString *type;
@property NSString *value;
@property NSString *valueType;
@end

@interface DOMHTMLPreElement : DOMHTMLElement
@property int width;
@end

@interface DOMHTMLQuoteElement : DOMHTMLElement
@property NSString *cite;
@end

@interface DOMHTMLScriptElement : DOMHTMLElement
@property NSString *text;
@property NSString *htmlFor;
@property NSString *event;
@property NSString *charset;
@property BOOL defer;
@property NSString *src;
@property NSString *type;
@end

@interface DOMHTMLSelectElement : DOMHTMLElement
@property(readonly) NSString *type;
@property int selectedIndex;
@property NSString *value;
@property(readonly) int length;
@property(readonly) DOMHTMLFormElement *form;
@property(readonly) DOMHTMLOptionsCollection *options;
@property BOOL disabled;
@property BOOL multiple;
@property NSString *name;
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
@property NSString *media;
@property NSString *type;
@end

@interface DOMHTMLTableCaptionElement : DOMHTMLElement
@property NSString *align;
@end

@interface DOMHTMLTableCellElement : DOMHTMLElement
@property(readonly) int cellIndex;
@property NSString *abbr;
@property NSString *align;
@property NSString *axis;
@property NSString *bgColor;
@property NSString *ch;
@property NSString *chOff;
@property int colSpan;
@property NSString *headers;
@property NSString *height;
@property BOOL noWrap;
@property int rowSpan;
@property NSString *scope;
@property NSString *vAlign;
@property NSString *width;
@end

@interface DOMHTMLTableColElement : DOMHTMLElement
@property NSString *align;
@property NSString *ch;
@property NSString *chOff;
@property int span;
@property NSString *vAlign;
@property NSString *width;
@end

@interface DOMHTMLTableElement : DOMHTMLElement
@property DOMHTMLTableCaptionElement *caption;
@property DOMHTMLTableSectionElement *tHead;
@property DOMHTMLTableSectionElement *tFoot;
@property(readonly) DOMHTMLCollection *rows;
@property(readonly) DOMHTMLCollection *tBodies;
@property NSString *align;
@property NSString *bgColor;
@property NSString *border;
@property NSString *cellPadding;
@property NSString *cellSpacing;
@property NSString *frameBorders;
@property NSString *rules;
@property NSString *summary;
@property NSString *width;
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
@property(readonly) DOMHTMLCollection *cells;
@property NSString *align;
@property NSString *bgColor;
@property NSString *ch;
@property NSString *chOff;
@property NSString *vAlign;
- (DOMHTMLElement *)insertCell:(int)index;
- (void)deleteCell:(int)index;
@end

@interface DOMHTMLTableSectionElement : DOMHTMLElement
@property NSString *align;
@property NSString *ch;
@property NSString *chOff;
@property NSString *vAlign;
@property(readonly) DOMHTMLCollection *rows;
- (DOMHTMLElement *)insertRow:(int)index;
- (void)deleteRow:(int)index;
@end

@interface DOMHTMLTextAreaElement : DOMHTMLElement
@property NSString *defaultValue;
@property(readonly) DOMHTMLFormElement *form;
@property NSString *accessKey;
@property int cols;
@property BOOL disabled;
@property NSString *name;
@property BOOL readOnly;
@property int rows;
@property int tabIndex;
@property(readonly) NSString *type;
@property NSString *value;
- (void)blur;
- (void)focus;
- (void)select;
@end

@interface DOMHTMLTitleElement : DOMHTMLElement
@property NSString *text;
@end

@interface DOMHTMLUListElement : DOMHTMLElement
@property BOOL compact;
@property NSString *type;
@end

@interface DOMCSSCharsetRule : DOMCSSRule
@property(readonly) NSString *encoding;
@end

@interface DOMCSSFontFaceRule : DOMCSSRule
@property(readonly) DOMCSSStyleDeclaration *style;
@end

@interface DOMCSSImportRule : DOMCSSRule
@property(readonly) NSString *href;
@property(readonly) DOMMediaList *media;
@property(readonly) DOMCSSStyleSheet *styleSheet;
@end

@interface DOMCSSMediaRule : DOMCSSRule
@property(readonly) DOMMediaList *media;
@property(readonly) DOMCSSRuleList *cssRules;
- (unsigned)insertRule:(NSString *)rule :(unsigned)index;
- (unsigned)insertRule:(NSString *)rule index:(unsigned)index;
- (void)deleteRule:(unsigned)index;
@end

@interface DOMCSSPageRule : DOMCSSRule
@property NSString *selectorText;
@property(readonly) DOMCSSStyleDeclaration *style;
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

@interface DOMCSSRule : DOMObject
@property(readonly) unsigned short type;
@property NSString *cssText;
@property(readonly) DOMCSSStyleSheet *parentStyleSheet;
@property(readonly) DOMCSSRule *parentRule;
@end

@interface DOMCSSRuleList : DOMObject
@property(readonly) unsigned length;
- (DOMCSSRule *)item:(unsigned)index;
@end

@interface DOMCSSStyleDeclaration : DOMObject
@property NSString *cssText;
@property(readonly) unsigned length;
@property(readonly) DOMCSSRule *parentRule;
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
@property NSString *selectorText;
@property(readonly) DOMCSSStyleDeclaration *style;
@end

@interface DOMCSSStyleSheet : DOMStyleSheet
@property(readonly) DOMCSSRule *ownerRule;
@property(readonly) DOMCSSRuleList *cssRules;
- (unsigned)insertRule:(NSString *)rule :(unsigned)index;
- (unsigned)insertRule:(NSString *)rule index:(unsigned)index;
- (void)deleteRule:(unsigned)index;
@end

@interface DOMCSSValue : DOMObject
@property NSString *cssText;
@property(readonly) unsigned short cssValueType;
@end

@interface DOMCSSValueList : DOMCSSValue
@property(readonly) unsigned length;
- (DOMCSSValue *)item:(unsigned)index;
@end

@interface DOMCSSUnknownRule : DOMCSSRule
@end

@interface DOMCounter : DOMObject
@property(readonly) NSString *identifier;
@property(readonly) NSString *listStyle;
@property(readonly) NSString *separator;
@end

@interface DOMRect : DOMObject
@property(readonly) DOMCSSPrimitiveValue *top;
@property(readonly) DOMCSSPrimitiveValue *right;
@property(readonly) DOMCSSPrimitiveValue *bottom;
@property(readonly) DOMCSSPrimitiveValue *left;
@end

@interface DOMEvent : DOMObject
@property(readonly) NSString *type;
@property(readonly) id <DOMEventTarget> target;
@property(readonly) id <DOMEventTarget> currentTarget;
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
@property(readonly) DOMAbstractView *view;
@property(readonly) int detail;
- (void)initUIEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view detail:(int)detail;
- (void)initUIEvent:(NSString *)type :(BOOL)canBubble :(BOOL)cancelable :(DOMAbstractView *)view :(int)detail;
@end

@interface DOMMutationEvent : DOMEvent
@property(readonly) DOMNode *relatedNode;
@property(readonly) NSString *prevValue;
@property(readonly) NSString *newValue;
@property(readonly) NSString *attrName;
@property(readonly) unsigned short attrChange;
- (void)initMutationEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable relatedNode:(DOMNode *)relatedNode prevValue:(NSString *)prevValue newValue:(NSString *)newValue attrName:(NSString *)attrName attrChange:(unsigned short)attrChange;
- (void)initMutationEvent:(NSString *)type :(BOOL)canBubble :(BOOL)cancelable :(DOMNode *)relatedNode :(NSString *)prevValue :(NSString *)newValue :(NSString *)attrName :(unsigned short)attrChange;
@end

@interface DOMOverflowEvent : DOMEvent
@property(readonly) unsigned short orient;
@property(readonly) BOOL horizontalOverflow;
@property(readonly) BOOL verticalOverflow;
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
@property(readonly) NSString *keyIdentifier;
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
@property(readonly) id <DOMEventTarget> relatedTarget;
- (void)initMouseEvent:(NSString *)type canBubble:(BOOL)canBubble cancelable:(BOOL)cancelable view:(DOMAbstractView *)view detail:(int)detail screenX:(int)screenX screenY:(int)screenY clientX:(int)clientX clientY:(int)clientY ctrlKey:(BOOL)ctrlKey altKey:(BOOL)altKey shiftKey:(BOOL)shiftKey metaKey:(BOOL)metaKey button:(unsigned short)button relatedTarget:(id <DOMEventTarget>)relatedTarget;
- (void)initMouseEvent:(NSString *)type :(BOOL)canBubble :(BOOL)cancelable :(DOMAbstractView *)view :(int)detail :(int)screenX :(int)screenY :(int)clientX :(int)clientY :(BOOL)ctrlKey :(BOOL)altKey :(BOOL)shiftKey :(BOOL)metaKey :(unsigned short)button :(id <DOMEventTarget>)relatedTarget;
@end

@interface DOMRange : DOMObject
@property(readonly) DOMNode *startContainer;
@property(readonly) int startOffset;
@property(readonly) DOMNode *endContainer;
@property(readonly) int endOffset;
@property(readonly) BOOL collapsed;
@property(readonly) DOMNode *commonAncestorContainer;
@property(readonly) NSString *text;
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
@property(readonly) DOMNode *root;
@property(readonly) unsigned whatToShow;
@property(readonly) id <DOMNodeFilter> filter;
@property(readonly) BOOL expandEntityReferences;
- (DOMNode *)nextNode;
- (DOMNode *)previousNode;
- (void)detach;
@end

@interface DOMTreeWalker : DOMObject
@property(readonly) DOMNode *root;
@property(readonly) unsigned whatToShow;
@property(readonly) id <DOMNodeFilter> filter;
@property(readonly) BOOL expandEntityReferences;
@property DOMNode *currentNode;
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
@property(readonly) NSString *stringValue;
@property(readonly) BOOL booleanValue;
@property(readonly) DOMNode *singleNodeValue;
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
