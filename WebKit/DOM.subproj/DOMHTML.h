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

#import <WebKit/DOMCore.h>

@class DOMHTMLElement;
@class DOMHTMLFormElement;
@class DOMHTMLTableCaptionElement;
@class DOMHTMLTableSectionElement;

@interface DOMHTMLCollection : DOMObject
- (unsigned long)length;
- (DOMNode *)item:(unsigned long)index;
- (DOMNode *)namedItem:(NSString *)name;
@end

@interface DOMHTMLOptionsCollection : DOMObject
- (unsigned long)length;
- (void)setLength:(unsigned long)length;
- (DOMNode *)item:(unsigned long)index;
- (DOMNode *)namedItem:(NSString *)name;
@end

@interface DOMHTMLDocument : DOMDocument
- (NSString *)title;
- (void)setTitle:(NSString *)title;
- (NSString *)referrer;
- (NSString *)domain;
- (NSString *)URL;
- (DOMHTMLElement *)body;
- (void)setBody:(DOMHTMLElement *)body;
- (DOMHTMLCollection *)images;
- (DOMHTMLCollection *)applets;
- (DOMHTMLCollection *)links;
- (DOMHTMLCollection *)forms;
- (DOMHTMLCollection *)anchors;
- (NSString *)cookie;
- (void)setCookie:(NSString *)cookie;
- (void)open;
- (void)close;
- (void)write:(NSString *)text;
- (void)writeln:(NSString *)text;
- (DOMElement *)getElementById:(NSString *)elementId;
- (DOMNodeList *)getElementsByName:(NSString *)elementName;
@end

@interface DOMHTMLElement : DOMElement
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

@interface DOMHTMLHtmlElement : DOMHTMLElement
- (NSString *)version;
- (void)setVersion:(NSString *)version;
@end

@interface DOMHTMLHeadElement : DOMHTMLElement
- (NSString *)profile;
- (void)setProfile:(NSString *)profile;
@end

@interface DOMHTMLLinkElement : DOMHTMLElement
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
- (void)setRel:(NSString *)rel;
- (NSString *)rev;
- (void)setRev:(NSString *)rev;
- (NSString *)target;
- (void)setTarget:(NSString *)target;
- (NSString *)type;
- (void)setType:(NSString *)type;
@end

@interface DOMHTMLTitleElement : DOMHTMLElement
- (NSString *)text;
- (void)setText:(NSString *)text;
@end

@interface DOMHTMLMetaElement : DOMHTMLElement
- (NSString *)content;
- (void)setContent:(NSString *)content;
- (NSString *)httpEquiv;
- (void)setHttpEquiv:(NSString *)httpEquiv;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)scheme;
- (void)setScheme:(NSString *)scheme;
@end

@interface DOMHTMLBaseElement : DOMHTMLElement
- (NSString *)href;
- (void)setHref:(NSString *)href;
- (NSString *)target;
- (void)setTarget:(NSString *)target;
@end

@interface DOMHTMLIsIndexElement : DOMHTMLElement
- (DOMHTMLFormElement *)form;
- (NSString *)prompt;
- (void)setPrompt:(NSString *)prompt;
@end

@interface DOMHTMLStyleElement : DOMHTMLElement
- (BOOL)disabled;
- (void)setDisabled:(BOOL)disabled;
- (NSString *)media;
- (void)setMedia:(NSString *)media;
- (NSString *)type;
- (void)setType:(NSString *)type;
@end

@interface DOMHTMLBodyElement : DOMHTMLElement
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

@interface DOMHTMLFormElement : DOMHTMLElement
- (DOMHTMLCollection *)elements;
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

@interface DOMHTMLSelectElement : DOMHTMLElement
- (NSString *)type;
- (long)selectedIndex;
- (void)setSelectedIndex:(long)selectedIndex;
- (NSString *)value;
- (void)setValue:(NSString *)value;
- (long)length;
- (DOMHTMLFormElement *)form;
- (DOMHTMLOptionsCollection *)options;
- (BOOL)disabled;
- (void)setDisabled:(BOOL)disabled;
- (BOOL)multiple;
- (void)setMultiple:(BOOL)multiple;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (long)size;
- (void)setSize:(long)size;
- (long)tabIndex;
- (void)setTabIndex:(long)tabIndex;
- (void)add:(DOMHTMLElement *)element :(DOMHTMLElement *)before;
- (void)remove:(long)index;
- (void)blur;
- (void)focus;
@end

@interface DOMHTMLOptGroupElement : DOMHTMLElement
- (BOOL)disabled;
- (void)setDisabled:(BOOL)disabled;
- (NSString *)label;
- (void)setLabel:(NSString *)label;
@end

@interface DOMHTMLOptionElement : DOMHTMLElement
- (DOMHTMLFormElement *)form;
- (BOOL)defaultSelected;
- (void)setDefaultSelected:(BOOL)defaultSelected;
- (NSString *)text;
- (long)index;
- (BOOL)disabled;
- (void)setDisabled:(BOOL)disabled;
- (NSString *)label;
- (void)setLabel:(NSString *)label;
- (BOOL)selected;
- (void)setSelected:(BOOL)selected;
- (NSString *)value;
- (void)setValue:(NSString *)value;
@end

@interface DOMHTMLInputElement : DOMHTMLElement
- (NSString *)defaultValue;
- (void)setDefaultValue:(NSString *)defaultValue;
- (BOOL)defaultChecked;
- (void)setDefaultChecked:(BOOL)defaultChecked;
- (DOMHTMLFormElement *)form;
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
- (void)setType:(NSString *)type;
- (NSString *)useMap;
- (void)setUseMap:(NSString *)useMap;
- (NSString *)value;
- (void)setValue:(NSString *)value;
- (void)blur;
- (void)focus;
- (void)select;
- (void)click;
@end

@interface DOMHTMLTextAreaElement : DOMHTMLElement
- (NSString *)defaultValue;
- (void)setDefaultValue:(NSString *)defaultValue;
- (DOMHTMLFormElement *)form;
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

@interface DOMHTMLButtonElement : DOMHTMLElement
- (DOMHTMLFormElement *)form;
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

@interface DOMHTMLLabelElement : DOMHTMLElement
- (DOMHTMLFormElement *)form;
- (NSString *)accessKey;
- (void)setAccessKey:(NSString *)accessKey;
- (NSString *)htmlFor;
- (void)setHtmlFor:(NSString *)htmlFor;
@end

@interface DOMHTMLFieldSetElement : DOMHTMLElement
- (DOMHTMLFormElement *)form;
@end

@interface DOMHTMLLegendElement : DOMHTMLElement
- (DOMHTMLFormElement *)form;
- (NSString *)accessKey;
- (void)setAccessKey:(NSString *)accessKey;
- (NSString *)align;
- (void)setAlign:(NSString *)align;
@end

@interface DOMHTMLUListElement : DOMHTMLElement
- (BOOL)compact;
- (void)setCompact:(BOOL)compact;
- (NSString *)type;
- (void)setType:(NSString *)type;
@end

@interface DOMHTMLOListElement : DOMHTMLElement
- (BOOL)compact;
- (void)setCompact:(BOOL)compact;
- (long)start;
- (void)setStart:(long)start;
- (NSString *)type;
- (void)setType:(NSString *)type;
@end

@interface DOMHTMLDListElement : DOMHTMLElement
- (BOOL)compact;
- (void)setCompact:(BOOL)compact;
@end

@interface DOMHTMLDirectoryElement : DOMHTMLElement
- (BOOL)compact;
- (void)setCompact:(BOOL)compact;
@end

@interface DOMHTMLMenuElement : DOMHTMLElement
- (BOOL)compact;
- (void)setCompact:(BOOL)compact;
@end

@interface DOMHTMLLIElement : DOMHTMLElement
- (NSString *)type;
- (void)setType:(NSString *)type;
- (long)value;
- (void)setValue:(long)value;
@end

@interface DOMHTMLDivElement : DOMHTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
@end

@interface DOMHTMLParagraphElement : DOMHTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
@end

@interface DOMHTMLHeadingElement : DOMHTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
@end

@interface DOMHTMLQuoteElement : DOMHTMLElement
- (NSString *)cite;
- (void)setCite:(NSString *)cite;
@end

@interface DOMHTMLPreElement : DOMHTMLElement
- (long)width;
- (void)setWidth:(long)width;
@end

@interface DOMHTMLBRElement : DOMHTMLElement
- (NSString *)clear;
- (void)setClear:(NSString *)clear;
@end

@interface DOMHTMLBaseFontElement : DOMHTMLElement
- (NSString *)color;
- (void)setColor:(NSString *)color;
- (NSString *)face;
- (void)setFace:(NSString *)face;
- (NSString *)size;
- (void)setSize:(NSString *)size;
@end

@interface DOMHTMLFontElement : DOMHTMLElement
- (NSString *)color;
- (void)setColor:(NSString *)color;
- (NSString *)face;
- (void)setFace:(NSString *)face;
- (NSString *)size;
- (void)setSize:(NSString *)size;
@end

@interface DOMHTMLHRElement : DOMHTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (BOOL)noShade;
- (void)setNoShade:(BOOL)noShade;
- (NSString *)size;
- (void)setSize:(NSString *)size;
- (NSString *)width;
- (void)setWidth:(NSString *)width;
@end

@interface DOMHTMLModElement : DOMHTMLElement
- (NSString *)cite;
- (void)setCite:(NSString *)cite;
- (NSString *)dateTime;
- (void)setDateTime:(NSString *)dateTime;
@end

@interface DOMHTMLAnchorElement : DOMHTMLElement
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

@interface DOMHTMLImageElement : DOMHTMLElement
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (NSString *)alt;
- (void)setAlt:(NSString *)alt;
- (NSString *)border;
- (void)setBorder:(NSString *)border;
- (long)height;
- (void)setHeight:(long)height;
- (long)hspace;
- (void)setHspace:(long)hspace;
- (BOOL)isMap;
- (void)setIsMap:(BOOL)isMap;
- (NSString *)longDesc;
- (void)setLongDesc:(NSString *)longDesc;
- (NSString *)src;
- (void)setSrc:(NSString *)src;
- (NSString *)useMap;
- (void)setUseMap:(NSString *)useMap;
- (long)vspace;
- (void)setVspace:(long)vspace;
- (long)width;
- (void)setWidth:(long)width;
@end

@interface DOMHTMLObjectElement : DOMHTMLElement
- (DOMHTMLFormElement *)form;
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
- (long)hspace;
- (void)setHspace:(long)hspace;
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
- (long)vspace;
- (void)setVspace:(long)vspace;
- (NSString *)width;
- (void)setWidth:(NSString *)width;
- (DOMDocument *)contentDocument;
@end

@interface DOMHTMLParamElement : DOMHTMLElement
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)type;
- (void)setType:(NSString *)type;
- (NSString *)value;
- (void)setValue:(NSString *)value;
- (NSString *)valueType;
- (void)setValueType:(NSString *)valueType;
@end

@interface DOMHTMLAppletElement : DOMHTMLElement
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
- (NSString *)height;
- (void)setHeight:(NSString *)height;
- (long)hspace;
- (void)setHspace:(long)hspace;
- (NSString *)name;
- (void)setName:(NSString *)name;
- (NSString *)object;
- (void)setObject:(NSString *)object;
- (long)vspace;
- (void)setVspace:(long)vspace;
- (NSString *)width;
- (void)setWidth:(NSString *)width;
@end

@interface DOMHTMLMapElement : DOMHTMLElement
- (DOMHTMLCollection *)areas;
- (NSString *)name;
- (void)setName:(NSString *)name;
@end

@interface DOMHTMLAreaElement : DOMHTMLElement
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

@interface DOMHTMLScriptElement : DOMHTMLElement
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

@interface DOMHTMLTableElement : DOMHTMLElement
- (DOMHTMLTableCaptionElement *)caption;
- (void)setCaption:(DOMHTMLTableCaptionElement *)caption;
- (DOMHTMLTableSectionElement *)tHead;
- (void)setTHead:(DOMHTMLTableSectionElement *)tHead;
- (DOMHTMLTableSectionElement *)tFoot;
- (void)setTFoot:(DOMHTMLTableSectionElement *)tFoot;
- (DOMHTMLCollection *)rows;
- (DOMHTMLCollection *)tBodies;
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
// frameBorders corresponds to the frame method
- (NSString *)frameBorders; 
- (void)setFrameBorders:(NSString *)frameBorders;
- (NSString *)rules;
- (void)setRules:(NSString *)rules;
- (NSString *)summary;
- (void)setSummary:(NSString *)summary;
- (NSString *)width;
- (void)setWidth:(NSString *)width;
- (DOMHTMLElement *)createTHead;
- (void)deleteTHead;
- (DOMHTMLElement *)createTFoot;
- (void)deleteTFoot;
- (DOMHTMLElement *)createCaption;
- (void)deleteCaption;
- (DOMHTMLElement *)insertRow:(long)index;
- (void)deleteRow:(long)index;
@end

@interface DOMHTMLTableCaptionElement : DOMHTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
@end

@interface DOMHTMLTableColElement : DOMHTMLElement
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

@interface DOMHTMLTableSectionElement : DOMHTMLElement
- (NSString *)align;
- (void)setAlign:(NSString *)align;
- (NSString *)ch;
- (void)setCh:(NSString *)ch;
- (NSString *)chOff;
- (void)setChOff:(NSString *)chOff;
- (NSString *)vAlign;
- (void)setVAlign:(NSString *)vAlign;
- (DOMHTMLCollection *)rows;
- (DOMHTMLElement *)insertRow:(long)index;
- (void)deleteRow:(long)index;
@end

@interface DOMHTMLTableRowElement : DOMHTMLElement
- (long)rowIndex;
- (long)sectionRowIndex;
- (DOMHTMLCollection *)cells;
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
- (DOMHTMLElement *)insertCell:(long)index;
- (void)deleteCell:(long)index;
@end

@interface DOMHTMLTableCellElement : DOMHTMLElement
- (long)cellIndex;
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

@interface DOMHTMLFrameSetElement : DOMHTMLElement
- (NSString *)cols;
- (void)setCols:(NSString *)cols;
- (NSString *)rows;
- (void)setRows:(NSString *)rows;
@end

@interface DOMHTMLFrameElement : DOMHTMLElement
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
- (DOMDocument *)contentDocument;
@end

@interface DOMHTMLIFrameElement : DOMHTMLElement
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
- (DOMDocument *)contentDocument;
@end
