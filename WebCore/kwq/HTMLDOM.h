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

//=========================================================================
//=========================================================================
//=========================================================================

// Important Note:
// Though this file appears as an exported header from WebKit, the
// version you should edit is in WebCore. The WebKit version is copied
// to WebKit during the build process.

//=========================================================================
//=========================================================================
//=========================================================================

@interface HTMLCollection : NSObject
- (unsigned long)length;
- (DOMNode *)item:(unsigned long)index;
- (DOMNode *)namedItem:(NSString *)name;
@end

@interface HTMLElement : DOMElement
- (NSString *)idName;
- (void)setIdName:(NSString *)idName;
- (NSString *)title;
- (void)setTitle:(NSString *)title;
- (NSString *)lang;
- (void)setLang:(NSString *)lang;
- (NSString *)dir;
- (void)setDir:(NSString *)dir;
- (NSString *)className;
- (void)setClassName:(NSString *)className;
@end

@interface HTMLDocument : DOMDocument
- (NSString *)title;
- (void)setTitle:(NSString *)title;
- (NSString *)referrer;
- (NSString *)domain;
- (NSString *)URL;
- (HTMLElement *)body;
- (HTMLCollection *)images;
- (HTMLCollection *)applets;
- (HTMLCollection *)links;
- (HTMLCollection *)forms;
- (HTMLCollection *)anchors;
- (void)setBody:(HTMLElement *)body;
- (NSString *)cookie;
- (void)setCookie:(NSString *)cookie;
- (void)open;
- (void)close;
- (void)write:(NSString *)text;
- (void)writeln:(NSString *)text;
- (DOMElement *)getElementById:(NSString *)elementId;
- (DOMNodeList *)getElementsByName:(NSString *)elementName;
@end

@interface HTMLHtmlElement : HTMLElement
- (NSString *)version;
- (void)setVersion:(NSString *)version;
@end

@interface HTMLHeadElement : HTMLElement
- (NSString *)profile;
- (void)setProfile:(NSString *)profile;
@end

@interface HTMLLinkElement : HTMLElement
- (BOOL)disabled;
- (void)setDisabled:(BOOL)disabled;
- (NSString *)charset;
- (void)setCharset:(NSString *)charset;
- (NSString *)href;
- (void)setHref:(NSString *)href;
- (NSString *)hreflang;
- (void)setHreflang:(NSString *)hreflang;
- (NSString *)media;
- (void)setMedia:(NSString *)media;
- (NSString *)rel;
- (void)setRrevel:(NSString *)rel;
- (NSString *)rev;
- (void)setRev:(NSString *)rev;
- (NSString *)target;
- (void)setTarget:(NSString *)target;
- (NSString *)type;
- (void)setType:(NSString *)type;
@end

@interface HTMLTitleElement : HTMLElement
- (NSString *)text;
- (void)setText:(NSString *)text;
@end

@interface HTMLMetaElement : HTMLElement
- (NSString *)content;
- (void)setContent:(NSString *)content;
- (NSString *)httpEquiv;
- (void)setHttpEquiv:(NSString *)httpEquiv;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)scheme;
- (void)setScheme:(NSString *)scheme;
@end

@interface HTMLBaseElement : HTMLElement
- (NSString *)href;
- (void)setHref:(NSString *)href;
- (NSString *)target;
- (void)setTarget:(NSString *)target;
@end

@interface HTMLStyleElement : HTMLElement
- (BOOL)disabled;
- (void)setDisabled:(BOOL)disabled;
- (NSString *)media;
- (void)setMedia:(NSString *)media;
- (NSString *)type;
- (void)setType:(NSString *)type;
@end

@interface HTMLBodyElement : HTMLElement
- (NSString *)aLink;
- (void)setALink:(NSString *)aLink;
- (NSString *)background;
- (void)setBackground:(NSString *)background;
- (NSString *)bgColor;
- (void)setBgColor:(NSString *)bgColor;
- (NSString *)link;
- (void)setLink:(NSString *)link;
- (NSString *)text;
- (void)setText:(NSString *)text;
- (NSString *)vLink;
- (void)setVLink:(NSString *)vLink;
@end

@interface HTMLFormElement : HTMLElement
- (HTMLCollection *)elements;
- (long)length;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)acceptCharset;
- (void)setAcceptCharset:(NSString *)acceptCharset;
- (NSString *)action;
- (void)setAction:(NSString *)action;
- (NSString *)enctype;
- (void)setEnctype:(NSString *)enctype;
- (NSString *)method;
- (void)setMethod:(NSString *)method;
- (NSString *)target;
- (void)setTarget:(NSString *)target;
- (void)submit;
- (void)reset;
@end

@interface HTMLIsIndexElement : HTMLElement
- (NSString *)form;
- (NSString *)prompt;
- (void)setPrompt:(NSString *)prompt;
@end

@interface HTMLSelectElement : HTMLElement
- (NSString *)type;
- (long)selectedIndex;
- (void)setSelectedIndex:(long)selectedIndex;
- (NSString *)value;
- (void)setValue:(NSString *)value;
- (long)length;
- (HTMLFormElement *)form;
- (HTMLCollection *)options;
- (BOOL)disabled;
- (BOOL)multiple;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (long)size;
- (long)tabIndex;
- (void)add:(HTMLElement *)element :(HTMLElement *)before;
- (void)remove:(long)index;
- (void)blur;
- (void)focus;
@end

@interface HTMLOptGroupElement : HTMLElement
- (BOOL)disabled;
- (void)setDisabled:(BOOL)disabled;
- (NSString *)label;
- (void)setLabel:(NSString *)label;
@end

@interface HTMLOptionElement : HTMLElement
- (HTMLFormElement *)form;
- (BOOL)defaultSelected;
- (void)setDefaultSelected:(BOOL)defaultSelected;
- (NSString *)text;
- (long)index;
- (void)setIndex:(long)index;
- (BOOL)disabled;
- (void)setDisabled:(BOOL)disabled;
- (NSString *)label;
- (void)setLabel:(NSString *)label;
- (BOOL)selected;
- (NSString *)value;
- (void)setValue:(NSString *)value;
@end

@interface HTMLInputElement : HTMLElement
- (NSString *)defaultValue;
- (void)setDefaultValue:(NSString *)defaultValue;
- (BOOL)defaultChecked;
- (void)setDefaultChecked:(BOOL)defaultChecked;
- (HTMLFormElement *)form;
- (NSString *)accept;
- (void)setAccept:(NSString *)accept;
- (NSString *)accessKey;
- (void)setAccessKey:(NSString *)accessKey;
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (NSString *)alt;
- (void)setAlt:(NSString *)alt;
- (BOOL)checked;
- (void)setChecked:(BOOL)checked;
- (BOOL)disabled;
- (void)setDisabled:(BOOL)disabled;
- (long)maxLength;
- (void)setMaxLength:(long)maxLength;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (BOOL)readOnly;
- (void)setReadOnly:(BOOL)readOnly;
- (NSString *)size;
- (void)setSize:(NSString *)size;
- (NSString *)src;
- (void)setSrc:(NSString *)src;
- (long)tabIndex;
- (void)setTabIndex:(long)tabIndex;
- (NSString *)type;
- (NSString *)useMap;
- (void)setUseMap:(NSString *)useMap;
- (NSString *)value;
- (void)setValue:(NSString *)value;
- (void)blur;
- (void)focus;
- (void)select;
- (void)click;
@end

@interface HTMLTextAreaElement : HTMLElement
- (NSString *)defaultValue;
- (void)setDefaultValue:(NSString *)defaultValue;
- (HTMLFormElement *)form;
- (NSString *)accessKey;
- (void)setAccessKey:(NSString *)accessKey;
- (long)cols;
- (void)setCols:(long)cols;
- (BOOL)disabled;
- (void)setDisabled:(BOOL)disabled;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (BOOL)readOnly;
- (void)setReadOnly:(BOOL)readOnly;
- (long)rows;
- (void)setRows:(long)rows;
- (long)tabIndex;
- (void)setTabIndex:(long)tabIndex;
- (NSString *)type;
- (NSString *)value;
- (void)setValue:(NSString *)value;
- (void)blur;
- (void)focus;
- (void)select;
@end

@interface HTMLButtonElement : HTMLElement
- (HTMLFormElement *)form;
- (NSString *)accessKey;
- (void)setAccessKey:(NSString *)accessKey;
- (BOOL)disabled;
- (void)setDisabled:(BOOL)disabled;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (long)tabIndex;
- (void)setTabIndex:(long)tabIndex;
- (NSString *)type;
- (NSString *)value;
- (void)setValue:(NSString *)value;
@end

@interface HTMLLabelElement : HTMLElement
- (HTMLFormElement *)form;
- (NSString *)accessKey;
- (void)setAccessKey:(NSString *)accessKey;
- (NSString *)htmlFor;
- (void)setHtmlFor:(NSString *)htmlFor;
@end

@interface HTMLFieldSetElement : HTMLElement
- (HTMLFormElement *)form;
@end

@interface HTMLLegendElement : HTMLElement
- (HTMLFormElement *)form;
- (NSString *)accessKey;
- (void)setAccessKey:(NSString *)accessKey;
- (NSString *)align;
- (void)setAlign:(NSString *)align;
@end

@interface HTMLUListElement : HTMLElement
- (BOOL)compact;
- (void)setCompact:(BOOL)compact;
- (NSString *)type;
- (void)setType:(NSString *)type;
@end

@interface HTMLOListElement : HTMLElement
- (BOOL)compact;
- (void)setCompact:(BOOL)compact;
- (long)start;
- (void)setStart:(long)start;
- (NSString *)type;
- (void)setType:(NSString *)type;
@end

@interface HTMLDListElement : HTMLElement
- (BOOL)compact;
- (void)setCompact:(BOOL)compact;
@end

@interface HTMLDirectoryElement : HTMLElement
- (BOOL)compact;
- (void)setCompact:(BOOL)compact;
@end

@interface HTMLMenuElement : HTMLElement
- (BOOL)compact;
- (void)setCompact:(BOOL)compact;
@end

@interface HTMLLIElement : HTMLElement
- (NSString *)type;
- (void)setType:(NSString *)type;
- (long)value;
- (void)setValue:(long)value;
@end

@interface HTMLBlockquoteElement : HTMLElement
- (NSString *)cite;
- (void)setCite:(NSString *)cite;
@end

@interface HTMLDivElement : HTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
@end

@interface HTMLParagraphElement : HTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
@end

@interface HTMLHeadingElement : HTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
@end

@interface HTMLQuoteElement : HTMLElement
- (NSString *)cite;
- (void)setCite:(NSString *)cite;
@end

@interface HTMLPreElement : HTMLElement
- (long)width;
- (void)setWidth:(long)width;
@end

@interface HTMLBRElement : HTMLElement
- (NSString *)clear;
- (void)setClear:(NSString *)clear;
@end

@interface HTMLBaseFontElement : HTMLElement
- (NSString *)color;
- (void)setColor:(NSString *)color;
- (NSString *)face;
- (void)setFace:(NSString *)face;
- (NSString *)size;
- (void)setSize:(NSString *)size;
@end

@interface HTMLFontElement : HTMLElement
- (NSString *)color;
- (void)setColor:(NSString *)color;
- (NSString *)face;
- (void)setFace:(NSString *)face;
- (NSString *)size;
- (void)setSize:(NSString *)size;
@end

@interface HTMLHRElement : HTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (BOOL)noShade;
- (void)setNoShade:(BOOL)noShade;
- (NSString *)size;
- (void)setSize:(NSString *)size;
- (NSString *)width;
- (void)setWidth:(NSString *)width;
@end

@interface HTMLModElement : HTMLElement
- (NSString *)cite;
- (void)setCite:(NSString *)cite;
- (NSString *)dateTime;
- (void)setDateTime:(NSString *)dateTime;
@end

@interface HTMLAnchorElement : HTMLElement
- (NSString *)accessKey;
- (void)setAccessKey:(NSString *)accessKey;
- (NSString *)charset;
- (void)setCharset:(NSString *)charset;
- (NSString *)coords;
- (void)setCoords:(NSString *)coords;
- (NSString *)href;
- (void)setHref:(NSString *)href;
- (NSString *)hreflang;
- (void)setHreflang:(NSString *)hreflang;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)rel;
- (void)setRel:(NSString *)rel;
- (NSString *)rev;
- (void)setRev:(NSString *)rev;
- (NSString *)shape;
- (void)setShape:(NSString *)shape;
- (long)tabIndex;
- (void)setTabIndex:(long)tabIndex;
- (NSString *)target;
- (void)setTarget:(NSString *)target;
- (NSString *)type;
- (void)setType:(NSString *)type;
- (void)blur;
- (void)focus;
@end

@interface HTMLImageElement : HTMLElement
- (NSString *)lowSrc;
- (void)setLowSrc:(NSString *)lowSrc;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (NSString *)alt;
- (void)setAlt:(NSString *)alt;
- (NSString *)border;
- (void)setBorder:(NSString *)border;
- (NSString *)height;
- (void)setHeight:(NSString *)height;
- (NSString *)hspace;
- (void)setHspace:(NSString *)hspace;
- (BOOL)isMap;
- (void)setIsMap:(BOOL)isMap;
- (NSString *)longDesc;
- (void)setLongDesc:(NSString *)longDesc;
- (NSString *)src;
- (void)setSrc:(NSString *)src;
- (NSString *)useMap;
- (void)setUseMap:(NSString *)useMap;
- (NSString *)vspace;
- (void)setVspace:(NSString *)vspace;
- (NSString *)width;
- (void)setWidth:(NSString *)width;
@end

@interface HTMLObjectElement : HTMLElement
- (HTMLFormElement *)form;
- (NSString *)code;
- (void)setCode:(NSString *)code;
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (NSString *)archive;
- (void)setArchive:(NSString *)archive;
- (NSString *)border;
- (void)setBorder:(NSString *)border;
- (NSString *)codeBase;
- (void)setCodeBase:(NSString *)codeBase;
- (NSString *)codeType;
- (void)setCodeType:(NSString *)codeType;
- (NSString *)data;
- (void)setData:(NSString *)data;
- (BOOL)declare;
- (void)setDeclare:(BOOL)declare;
- (NSString *)height;
- (void)setHeight:(NSString *)height;
- (NSString *)hspace;
- (void)setHspace:(NSString *)hspace;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)standby;
- (void)setStandby:(NSString *)standby;
- (long)tabIndex;
- (void)setTabIndex:(long)tabIndex;
- (NSString *)type;
- (void)setType:(NSString *)type;
- (NSString *)useMap;
- (void)setUseMap:(NSString *)useMap;
- (NSString *)vspace;
- (void)setVspace:(NSString *)vspace;
- (NSString *)width;
- (void)setWidth:(NSString *)width;
@end

@interface HTMLParamElement : HTMLElement
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)type;
- (void)setType:(NSString *)type;
- (NSString *)value;
- (void)setValue:(NSString *)value;
- (NSString *)valueType;
- (void)setValueType:(NSString *)valueType;
@end

@interface HTMLAppletElement : HTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (NSString *)alt;
- (void)setAlt:(NSString *)alt;
- (NSString *)archive;
- (void)setArchive:(NSString *)archive;
- (NSString *)code;
- (void)setCode:(NSString *)code;
- (NSString *)codeBase;
- (void)setCodeBase:(NSString *)codeBase;
- (NSString *)codeType;
- (void)setCodeType:(NSString *)codeType;
- (NSString *)height;
- (void)setHeight:(NSString *)height;
- (NSString *)hspace;
- (void)setHspace:(NSString *)hspace;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)object;
- (void)setObject:(NSString *)object;
- (NSString *)vspace;
- (void)setVspace:(NSString *)vspace;
- (NSString *)width;
- (void)setWidth:(NSString *)width;
@end

@interface HTMLMapElement : HTMLElement
- (HTMLCollection *)areas;
- (NSString *)name;
- (void)setName:(NSString *)name;
@end

@interface HTMLAreaElement : HTMLElement
- (NSString *)accessKey;
- (void)setAccessKey:(NSString *)accessKey;
- (NSString *)alt;
- (void)setAlt:(NSString *)alt;
- (NSString *)coords;
- (void)setCoords:(NSString *)coords;
- (NSString *)href;
- (void)setHref:(NSString *)href;
- (BOOL)noHref;
- (void)setNoHref:(BOOL)noHref;
- (NSString *)shape;
- (void)setShape:(NSString *)shape;
- (long)tabIndex;
- (void)setTabIndex:(long)tabIndex;
- (NSString *)target;
- (void)setTarget:(NSString *)target;
@end

@interface HTMLScriptElement : HTMLElement
- (NSString *)text;
- (void)setText:(NSString *)text;
- (NSString *)htmlFor;
- (void)setHtmlFor:(NSString *)htmlFor;
- (NSString *)event;
- (void)setEvent:(NSString *)event;
- (NSString *)charset;
- (void)setCharset:(NSString *)charset;
- (BOOL)defer;
- (void)setDefer:(BOOL)defer;
- (NSString *)src;
- (void)setSrc:(NSString *)src;
- (NSString *)type;
- (void)setType:(NSString *)type;
@end

@interface HTMLTableCaptionElement : HTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
@end

@interface HTMLTableSectionElement : HTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (NSString *)ch;
- (void)setCh:(NSString *)ch;
- (NSString *)chOff;
- (void)setChOff:(NSString *)chOff;
- (NSString *)vAlign;
- (void)setVAlign:(NSString *)vAlign;
- (HTMLCollection *)rows;
- (HTMLElement *)insertRow:(long)index;
- (void)deleteRow:(long)index;
@end

@interface HTMLTableElement : HTMLElement
- (HTMLTableCaptionElement *)caption;
- (HTMLTableSectionElement *)tHead;
- (HTMLTableSectionElement *)tFoot;
- (HTMLCollection *)rows;
- (HTMLCollection *)tBodies;
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (NSString *)bgColor;
- (void)setBgColor:(NSString *)bgColor;
- (NSString *)border;
- (void)setBorder:(NSString *)border;
- (NSString *)cellPadding;
- (void)setCellPadding:(NSString *)cellPadding;
- (NSString *)cellSpacing;
- (void)setCellSpacing:(NSString *)cellSpacing;
- (NSString *)frame;
- (void)setFrame:(NSString *)frame;
- (NSString *)rules;
- (void)setRules:(NSString *)rules;
- (NSString *)summary;
- (void)setSummary:(NSString *)summary;
- (NSString *)width;
- (void)setWidth:(NSString *)width;
- (HTMLElement *)createTHead;
- (void)deleteTHead;
- (HTMLElement *)createTFoot;
- (void)deleteTFoot;
- (HTMLElement *)createCaption;
- (void)deleteCaption;
- (HTMLElement *)insertRow:(long)index;
- (void)deleteRow:(long)index;
@end

@interface HTMLTableColElement : HTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (NSString *)ch;
- (void)setCh:(NSString *)ch;
- (NSString *)chOff;
- (void)setChOff:(NSString *)chOff;
- (long)span;
- (void)setSpan:(long)span;
- (NSString *)vAlign;
- (void)setVAlign:(NSString *)vAlign;
- (NSString *)width;
- (void)setWidth:(NSString *)width;
@end

@interface HTMLTableRowElement : HTMLElement
- (long)rowIndex;
- (void)setRowIndex:(long)rowIndex;
- (long)sectionRowIndex;
- (void)setSectionRowIndex:(long)sectionRowIndex;
- (HTMLCollection *)cells;
- (void)setCells:(HTMLCollection *)cells; // Is cells really read/write?
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (NSString *)bgColor;
- (void)setBgColor:(NSString *)bgColor;
- (NSString *)ch;
- (void)setCh:(NSString *)ch;
- (NSString *)chOff;
- (void)setChOff:(NSString *)chOff;
- (NSString *)vAlign;
- (void)setVAlign:(NSString *)vAlign;
- (HTMLElement *)insertCell:(long)index;
- (void)deleteCell:(long)index;
@end

@interface HTMLTableCellElement : HTMLElement
- (long)cellIndex;
- (void)setCellIndex:(long)cellIndex;
- (NSString *)abbr;
- (void)setAbbr:(NSString *)abbr;
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (NSString *)axis;
- (void)setAxis:(NSString *)axis;
- (NSString *)bgColor;
- (void)setBgColor:(NSString *)bgColor;
- (NSString *)ch;
- (void)setCh:(NSString *)ch;
- (NSString *)chOff;
- (void)setChOff:(NSString *)chOff;
- (long)colSpan;
- (void)setColSpan:(long)colSpan;
- (NSString *)headers;
- (void)setHeaders:(NSString *)headers;
- (NSString *)height;
- (void)setHeight:(NSString *)height;
- (BOOL)noWrap;
- (void)setNoWrap:(BOOL)noWrap;
- (long)rowSpan;
- (void)setRowSpan:(long)rowSpan;
- (NSString *)scope;
- (void)setScope:(NSString *)scope;
- (NSString *)vAlign;
- (void)setVAlign:(NSString *)vAlign;
- (NSString *)width;
- (void)setWidth:(NSString *)width;
@end

@interface HTMLFrameSetElement : HTMLElement
- (NSString *)cols;
- (void)setCols:(NSString *)cols;
- (NSString *)rows;
- (void)setRows:(NSString *)rows;
@end

@interface HTMLFrameElement : HTMLElement
- (NSString *)frameBorder;
- (void)setFrameBorder:(NSString *)frameBorder;
- (NSString *)longDesc;
- (void)setLongDesc:(NSString *)longDesc;
- (NSString *)marginHeight;
- (void)setMarginHeight:(NSString *)marginHeight;
- (NSString *)marginWidth;
- (void)setMarginWidth:(NSString *)marginWidth;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (BOOL)noResize;
- (void)setNoResize:(BOOL)noResize;
- (NSString *)scrolling;
- (void)setScrolling:(NSString *)scrolling;
- (NSString *)src;
- (void)setSrc:(NSString *)src;
@end

@interface HTMLIFrameElement : HTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (NSString *)frameBorder;
- (void)setFrameBorder:(NSString *)frameBorder;
- (NSString *)height;
- (void)setHeight:(NSString *)height;
- (NSString *)longDesc;
- (void)setLongDesc:(NSString *)longDesc;
- (NSString *)marginHeight;
- (void)setMarginHeight:(NSString *)marginHeight;
- (NSString *)marginWidth;
- (void)setMarginWidth:(NSString *)marginWidth;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)scrolling;
- (void)setScrolling:(NSString *)scrolling;
- (NSString *)src;
- (void)setSrc:(NSString *)src;
- (NSString *)width;
- (void)setWidth:(NSString *)width;
@end
