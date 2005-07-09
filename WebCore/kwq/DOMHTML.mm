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

#import "DOMHTML.h"

#import "dom_string.h"
#import "html_baseimpl.h"
#import "html_blockimpl.h"
#import "html_documentimpl.h"
#import "html_elementimpl.h"
#import "html_formimpl.h"
#import "html_headimpl.h"
#import "html_imageimpl.h"
#import "html_inlineimpl.h"
#import "html_listimpl.h"
#import "html_miscimpl.h"
#import "html_objectimpl.h"
#import "html_tableimpl.h"
#import "htmlattrs.h"
#import "dom_elementimpl.h"
#import "dom_nodeimpl.h"
#import "markup.h"

#import "DOMExtensions.h"
#import "DOMInternal.h"
#import "DOMHTMLInternal.h"
#import "KWQAssertions.h"
#import "KWQFoundationExtras.h"

using DOM::Document;
using DOM::DocumentFragmentImpl;
using DOM::DOMString;
using DOM::ElementImpl;
using DOM::HTMLAnchorElementImpl;
using DOM::HTMLAppletElementImpl;
using DOM::HTMLAreaElementImpl;
using DOM::HTMLBaseElementImpl;
using DOM::HTMLBaseFontElementImpl;
using DOM::HTMLBodyElementImpl;
using DOM::HTMLBRElementImpl;
using DOM::HTMLButtonElementImpl;
using DOM::HTMLCollectionImpl;
using DOM::HTMLDirectoryElementImpl;
using DOM::HTMLDivElementImpl;
using DOM::HTMLDListElementImpl;
using DOM::HTMLDocumentImpl;
using DOM::HTMLElementImpl;
using DOM::HTMLEmbedElementImpl;
using DOM::HTMLFieldSetElementImpl;
using DOM::HTMLFontElementImpl;
using DOM::HTMLFormCollectionImpl;
using DOM::HTMLFormElementImpl;
using DOM::HTMLFrameElementImpl;
using DOM::HTMLFrameSetElementImpl;
using DOM::HTMLGenericFormElementImpl;
using DOM::HTMLHeadElementImpl;
using DOM::HTMLHeadingElementImpl;
using DOM::HTMLHRElementImpl;
using DOM::HTMLHtmlElementImpl;
using DOM::HTMLIFrameElementImpl;
using DOM::HTMLImageElementImpl;
using DOM::HTMLInputElementImpl;
using DOM::HTMLIsIndexElementImpl;
using DOM::HTMLLabelElementImpl;
using DOM::HTMLLegendElementImpl;
using DOM::HTMLLIElementImpl;
using DOM::HTMLLinkElementImpl;
using DOM::HTMLMapElementImpl;
using DOM::HTMLMenuElementImpl;
using DOM::HTMLMetaElementImpl;
using DOM::HTMLObjectElementImpl;
using DOM::HTMLOListElementImpl;
using DOM::HTMLOptGroupElementImpl;
using DOM::HTMLOptionElementImpl;
using DOM::HTMLOptionsCollectionImpl;
using DOM::HTMLParagraphElementImpl;
using DOM::HTMLParamElementImpl;
using DOM::HTMLPreElementImpl;
using DOM::HTMLScriptElementImpl;
using DOM::HTMLSelectElementImpl;
using DOM::HTMLStyleElementImpl;
using DOM::HTMLTableElementImpl;
using DOM::HTMLTableCaptionElementImpl;
using DOM::HTMLTableCellElementImpl;
using DOM::HTMLTableColElementImpl;
using DOM::HTMLTableRowElementImpl;
using DOM::HTMLTableSectionElementImpl;
using DOM::HTMLTextAreaElementImpl;
using DOM::HTMLTitleElementImpl;
using DOM::HTMLUListElementImpl;
using DOM::NameNodeListImpl;
using DOM::NodeImpl;

@interface DOMHTMLCollection (WebCoreInternal)
+ (DOMHTMLCollection *)_collectionWithImpl:(HTMLCollectionImpl *)impl;
@end

@interface DOMHTMLElement (WebCoreInternal)
+ (DOMHTMLElement *)_elementWithImpl:(HTMLElementImpl *)impl;
- (HTMLElementImpl *)_HTMLElementImpl;
@end

@interface DOMHTMLFormElement (WebCoreInternal)
+ (DOMHTMLFormElement *)_formElementWithImpl:(HTMLFormElementImpl *)impl;
@end

@interface DOMHTMLTableCaptionElement (WebCoreInternal)
+ (DOMHTMLTableCaptionElement *)_tableCaptionElementWithImpl:(HTMLTableCaptionElementImpl *)impl;
- (HTMLTableCaptionElementImpl *)_tableCaptionElementImpl;
@end

@interface DOMHTMLTableSectionElement (WebCoreInternal)
+ (DOMHTMLTableSectionElement *)_tableSectionElementWithImpl:(HTMLTableSectionElementImpl *)impl;
- (HTMLTableSectionElementImpl *)_tableSectionElementImpl;
@end

@interface DOMHTMLTableElement (WebCoreInternal)
+ (DOMHTMLTableElement *)_tableElementWithImpl:(HTMLTableElementImpl *)impl;
- (HTMLTableElementImpl *)_tableElementImpl;
@end

@interface DOMHTMLTableCellElement (WebCoreInternal)
+ (DOMHTMLTableCellElement *)_tableCellElementWithImpl:(HTMLTableCellElementImpl *)impl;
- (HTMLTableCellElementImpl *)_tableCellElementImpl;
@end

//------------------------------------------------------------------------------------------

@implementation DOMHTMLCollection

- (void)dealloc
{
    if (_internal) {
        DOM_cast<HTMLCollectionImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<HTMLCollectionImpl *>(_internal)->deref();
    }
    [super finalize];
}

- (HTMLCollectionImpl *)_collectionImpl
{
    return DOM_cast<HTMLCollectionImpl *>(_internal);
}

- (unsigned long)length
{
    return [self _collectionImpl]->length();
}

- (DOMNode *)item:(unsigned long)index
{
    return [DOMNode _nodeWithImpl:[self _collectionImpl]->item(index)];
}

- (DOMNode *)namedItem:(NSString *)name
{
    return [DOMNode _nodeWithImpl:[self _collectionImpl]->namedItem(name)];
}

@end

@implementation DOMHTMLCollection (WebCoreInternal)

- (id)_initWithCollectionImpl:(HTMLCollectionImpl *)impl
{
    ASSERT(impl);
    
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMHTMLCollection *)_collectionWithImpl:(HTMLCollectionImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithCollectionImpl:impl] autorelease];
}

@end

@implementation DOMHTMLOptionsCollection

- (void)dealloc
{
    if (_internal) {
        DOM_cast<HTMLOptionsCollectionImpl *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<HTMLOptionsCollectionImpl *>(_internal)->deref();
    }
    [super finalize];
}

- (id)_initWithOptionsCollectionImpl:(HTMLOptionsCollectionImpl *)impl
{
    ASSERT(impl);
    
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMHTMLOptionsCollection *)_optionsCollectionWithImpl:(HTMLOptionsCollectionImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithOptionsCollectionImpl:impl] autorelease];
}

- (HTMLOptionsCollectionImpl *)_optionsCollectionImpl
{
    return DOM_cast<HTMLOptionsCollectionImpl *>(_internal);
}

- (unsigned long)length
{
    return [self _optionsCollectionImpl]->length();
}

- (void)setLength:(unsigned long)length
{
    [self _optionsCollectionImpl]->setLength(length);
}

- (DOMNode *)item:(unsigned long)index
{
    return [DOMNode _nodeWithImpl:[self _optionsCollectionImpl]->item(index)];
}

- (DOMNode *)namedItem:(NSString *)name
{
    return [DOMNode _nodeWithImpl:[self _optionsCollectionImpl]->namedItem(name)];
}

@end

@implementation DOMHTMLElement

- (NSString *)idName
{
    return [self _HTMLElementImpl]->getAttribute(ATTR_ID);
}

- (void)setIdName:(NSString *)idName
{
    [self _HTMLElementImpl]->setAttribute(ATTR_ID, idName);
}

- (NSString *)title
{
    return [self _HTMLElementImpl]->getAttribute(ATTR_TITLE);
}

- (void)setTitle:(NSString *)title
{
    [self _HTMLElementImpl]->setAttribute(ATTR_TITLE, title);
}

- (NSString *)lang
{
    return [self _HTMLElementImpl]->getAttribute(ATTR_LANG);
}

- (void)setLang:(NSString *)lang
{
    [self _HTMLElementImpl]->setAttribute(ATTR_LANG, lang);
}

- (NSString *)dir
{
    return [self _HTMLElementImpl]->getAttribute(ATTR_DIR);
}

- (void)setDir:(NSString *)dir
{
    [self _HTMLElementImpl]->setAttribute(ATTR_DIR, dir);
}

- (NSString *)className
{
    return [self _HTMLElementImpl]->getAttribute(ATTR_CLASS);
}

- (void)setClassName:(NSString *)className
{
    [self _HTMLElementImpl]->setAttribute(ATTR_CLASS, className);
}

@end

@implementation DOMHTMLElement (WebCoreInternal)

+ (DOMHTMLElement *)_elementWithImpl:(HTMLElementImpl *)impl
{
    return static_cast<DOMHTMLElement *>([DOMNode _nodeWithImpl:impl]);
}

- (HTMLElementImpl *)_HTMLElementImpl
{
    return static_cast<HTMLElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

@end

@implementation DOMHTMLElement (DOMHTMLElementExtensions)

- (NSString *)innerHTML
{
    return [self _HTMLElementImpl]->innerHTML();
}

- (void)setInnerHTML:(NSString *)innerHTML
{
    int exception = 0;
    [self _HTMLElementImpl]->setInnerHTML(innerHTML, exception);
    raiseOnDOMError(exception);
}

- (NSString *)outerHTML
{
    return [self _HTMLElementImpl]->outerHTML();
}

- (void)setOuterHTML:(NSString *)outerHTML
{
    int exception = 0;
    [self _HTMLElementImpl]->setOuterHTML(outerHTML, exception);
    raiseOnDOMError(exception);
}

- (NSString *)innerText
{
    return [self _HTMLElementImpl]->innerText();
}

- (void)setInnerText:(NSString *)innerText
{
    int exception = 0;
    [self _HTMLElementImpl]->setInnerText(innerText, exception);
    raiseOnDOMError(exception);
}

- (NSString *)outerText
{
    return [self _HTMLElementImpl]->outerText();
}

- (void)setOuterText:(NSString *)outerText
{
    int exception = 0;
    [self _HTMLElementImpl]->setOuterText(outerText, exception);
    raiseOnDOMError(exception);
}

- (DOMHTMLCollection *)children
{
    HTMLCollectionImpl *collection = new HTMLCollectionImpl([self _HTMLElementImpl], HTMLCollectionImpl::NODE_CHILDREN);
    return [DOMHTMLCollection _collectionWithImpl:collection];
}

- (NSString *)contentEditable
{
    return [self _HTMLElementImpl]->contentEditable();
}

- (void)setContentEditable:(NSString *)contentEditable
{
    [self _HTMLElementImpl]->setContentEditable(contentEditable);
}

- (BOOL)isContentEditable
{
    return [self _HTMLElementImpl]->isContentEditable();
}

@end

@implementation DOMHTMLDocument

- (HTMLDocumentImpl *)_HTMLDocumentImpl
{
    return static_cast<HTMLDocumentImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)title
{
    return [self _HTMLDocumentImpl]->title();
}

- (void)setTitle:(NSString *)title
{
    [self _HTMLDocumentImpl]->setTitle(title);
}

- (NSString *)referrer
{
     return [self _HTMLDocumentImpl]->referrer();
}

- (NSString *)domain
{
     return [self _HTMLDocumentImpl]->domain();
}

- (NSString *)URL
{
    return [self _HTMLDocumentImpl]->URL().getNSString();
}

- (DOMHTMLElement *)body
{
    return [DOMHTMLElement _elementWithImpl:[self _HTMLDocumentImpl]->body()];
}

- (DOMHTMLCollection *)images
{
    HTMLCollectionImpl *collection = new HTMLCollectionImpl([self _HTMLDocumentImpl], HTMLCollectionImpl::DOC_IMAGES);
    return [DOMHTMLCollection _collectionWithImpl:collection];
}

- (DOMHTMLCollection *)applets
{
    HTMLCollectionImpl *collection = new HTMLCollectionImpl([self _HTMLDocumentImpl], HTMLCollectionImpl::DOC_APPLETS);
    return [DOMHTMLCollection _collectionWithImpl:collection];
}

- (DOMHTMLCollection *)links
{
    HTMLCollectionImpl *collection = new HTMLCollectionImpl([self _HTMLDocumentImpl], HTMLCollectionImpl::DOC_LINKS);
    return [DOMHTMLCollection _collectionWithImpl:collection];
}

- (DOMHTMLCollection *)forms
{
    HTMLCollectionImpl *collection = new HTMLCollectionImpl([self _HTMLDocumentImpl], HTMLCollectionImpl::DOC_FORMS);
    return [DOMHTMLCollection _collectionWithImpl:collection];
}

- (DOMHTMLCollection *)anchors
{
    HTMLCollectionImpl *collection = new HTMLCollectionImpl([self _HTMLDocumentImpl], HTMLCollectionImpl::DOC_ANCHORS);
    return [DOMHTMLCollection _collectionWithImpl:collection];
}

- (void)setBody:(DOMHTMLElement *)body
{
    int exceptionCode = 0;
    [self _HTMLDocumentImpl]->setBody([body _HTMLElementImpl], exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (NSString *)cookie
{
    return [self _HTMLDocumentImpl]->cookie();
}

- (void)setCookie:(NSString *)cookie
{
    [self _HTMLDocumentImpl]->setCookie(cookie);
}

- (void)open
{
    [self _HTMLDocumentImpl]->open();
}

- (void)close
{
    [self _HTMLDocumentImpl]->close();
}

- (void)write:(NSString *)text
{
    [self _HTMLDocumentImpl]->write(text);
}

- (void)writeln:(NSString *)text
{
    [self _HTMLDocumentImpl]->writeln(text);
}

- (DOMElement *)getElementById:(NSString *)elementId
{
    return [DOMElement _elementWithImpl:[self _HTMLDocumentImpl]->getElementById(elementId)];
}

- (DOMNodeList *)getElementsByName:(NSString *)elementName
{
    NameNodeListImpl *nodeList = new NameNodeListImpl([self _HTMLDocumentImpl], elementName);
    return [DOMNodeList _nodeListWithImpl:nodeList];
}

@end

@implementation DOMHTMLDocument (WebPrivate)

- (DOMDocumentFragment *)_createDocumentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString
{
    DocumentFragmentImpl *fragment = createFragmentFromMarkup([self _documentImpl], QString::fromNSString(markupString), QString::fromNSString(baseURLString));
    return [DOMDocumentFragment _documentFragmentWithImpl:fragment];
}

- (DOMDocumentFragment *)_createDocumentFragmentWithText:(NSString *)text
{
    return [DOMDocumentFragment _documentFragmentWithImpl:createFragmentFromText([self _documentImpl], QString::fromNSString(text))];
}

@end

@implementation DOMHTMLHtmlElement

- (HTMLHtmlElementImpl *)_HTMLHtmlElementImpl
{
    return static_cast<HTMLHtmlElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)version
{
    return [self _HTMLHtmlElementImpl]->getAttribute(ATTR_VERSION);
}

- (void)setVersion:(NSString *)version
{
    [self _HTMLHtmlElementImpl]->setAttribute(ATTR_VERSION, version);
}

@end

@implementation DOMHTMLHeadElement

- (HTMLHeadElementImpl *)_headElementImpl
{
    return static_cast<HTMLHeadElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)profile
{
    return [self _headElementImpl]->getAttribute(ATTR_PROFILE);
}

- (void)setProfile:(NSString *)profile
{
    [self _headElementImpl]->setAttribute(ATTR_PROFILE, profile);
}

@end

@implementation DOMHTMLLinkElement

- (HTMLLinkElementImpl *)_linkElementImpl
{
    return static_cast<HTMLLinkElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)disabled
{
    return ![self _linkElementImpl]->getAttribute(ATTR_DISABLED).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _linkElementImpl]->setAttribute(ATTR_DISABLED, disabled ? "" : 0);
    [self _linkElementImpl]->setDisabledState(disabled);
}

- (NSString *)charset
{
    return [self _linkElementImpl]->getAttribute(ATTR_CHARSET);
}

- (void)setCharset:(NSString *)charset
{
    [self _linkElementImpl]->setAttribute(ATTR_CHARSET, charset);
}

- (NSString *)href
{
    return [self _linkElementImpl]->href();
}

- (void)setHref:(NSString *)href
{
    [self _linkElementImpl]->setAttribute(ATTR_HREF, href);
}

- (NSString *)hreflang
{
    return [self _linkElementImpl]->getAttribute(ATTR_HREFLANG);
}

- (void)setHreflang:(NSString *)hreflang
{
   [self _linkElementImpl]->setAttribute(ATTR_HREFLANG, hreflang);
}

- (NSString *)media
{
    return [self _linkElementImpl]->getAttribute(ATTR_MEDIA);
}

- (void)setMedia:(NSString *)media
{
    [self _linkElementImpl]->setAttribute(ATTR_MEDIA, media);
}

- (NSString *)rel
{
    return [self _linkElementImpl]->getAttribute(ATTR_REL);
}

- (void)setRel:(NSString *)rel
{
    [self _linkElementImpl]->setAttribute(ATTR_REL, rel);
}

- (NSString *)rev
{
    return [self _linkElementImpl]->getAttribute(ATTR_REV);
}

- (void)setRev:(NSString *)rev
{
    [self _linkElementImpl]->setAttribute(ATTR_REV, rev);
}

- (NSString *)target
{
    return [self _linkElementImpl]->getAttribute(ATTR_TARGET);
}

- (void)setTarget:(NSString *)target
{
    [self _linkElementImpl]->setAttribute(ATTR_TARGET, target);
}

- (NSString *)type
{
    return [self _linkElementImpl]->getAttribute(ATTR_TYPE);
}

- (void)setType:(NSString *)type
{
    [self _linkElementImpl]->setAttribute(ATTR_TYPE, type);
}

@end

@implementation DOMHTMLTitleElement

- (HTMLTitleElementImpl *)_titleElementImpl
{
    return static_cast<HTMLTitleElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)text
{
    return [self _titleElementImpl]->getAttribute(ATTR_TEXT);
}

- (void)setText:(NSString *)text
{
    [self _titleElementImpl]->setAttribute(ATTR_TEXT, text);
}

@end

@implementation DOMHTMLMetaElement

- (HTMLMetaElementImpl *)_metaElementImpl
{
    return static_cast<HTMLMetaElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)content
{
    return [self _metaElementImpl]->getAttribute(ATTR_CONTENT);
}

- (void)setContent:(NSString *)content
{
    [self _metaElementImpl]->setAttribute(ATTR_CONTENT, content);
}

- (NSString *)httpEquiv
{
    return [self _metaElementImpl]->getAttribute(ATTR_HTTP_EQUIV);
}

- (void)setHttpEquiv:(NSString *)httpEquiv
{
    [self _metaElementImpl]->setAttribute(ATTR_HTTP_EQUIV, httpEquiv);
}

- (NSString *)name
{
    return [self _metaElementImpl]->getAttribute(ATTR_NAME);
}

- (void)setName:(NSString *)name
{
    [self _metaElementImpl]->setAttribute(ATTR_NAME, name);
}

- (NSString *)scheme
{
    return [self _metaElementImpl]->getAttribute(ATTR_SCHEME);
}

- (void)setScheme:(NSString *)scheme
{
    [self _metaElementImpl]->setAttribute(ATTR_SCHEME, scheme);
}

@end

@implementation DOMHTMLBaseElement

- (HTMLBaseElementImpl *)_baseElementImpl
{
    return static_cast<HTMLBaseElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)href
{
    return [self _baseElementImpl]->href();
}

- (void)setHref:(NSString *)href
{
    [self _baseElementImpl]->setAttribute(ATTR_HREF, href);
}

- (NSString *)target
{
    return [self _baseElementImpl]->getAttribute(ATTR_TARGET);
}

- (void)setTarget:(NSString *)target
{
    [self _baseElementImpl]->setAttribute(ATTR_SCHEME, target);
}

@end

@implementation DOMHTMLStyleElement

- (HTMLStyleElementImpl *)_styleElementImpl
{
    return static_cast<HTMLStyleElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)disabled
{
    return ![self _styleElementImpl]->getAttribute(ATTR_DISABLED).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _styleElementImpl]->setAttribute(ATTR_DISABLED, disabled ? "" : 0);
}

- (NSString *)media
{
    return [self _styleElementImpl]->getAttribute(ATTR_MEDIA);
}

- (void)setMedia:(NSString *)media
{
    [self _styleElementImpl]->setAttribute(ATTR_MEDIA, media);
}

- (NSString *)type
{
    return [self _styleElementImpl]->getAttribute(ATTR_TYPE);
}

- (void)setType:(NSString *)type
{
    [self _styleElementImpl]->setAttribute(ATTR_TYPE, type);
}

@end

@implementation DOMHTMLBodyElement

- (HTMLBodyElementImpl *)_bodyElementImpl
{
    return static_cast<HTMLBodyElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)aLink
{
    return [self _bodyElementImpl]->getAttribute(ATTR_ALINK);
}

- (void)setALink:(NSString *)aLink
{
    [self _bodyElementImpl]->setAttribute(ATTR_ALINK, aLink);
}

- (NSString *)background
{
    return [self _bodyElementImpl]->getAttribute(ATTR_BACKGROUND);
}

- (void)setBackground:(NSString *)background
{
    [self _bodyElementImpl]->setAttribute(ATTR_BACKGROUND, background);
}

- (NSString *)bgColor
{
    return [self _bodyElementImpl]->getAttribute(ATTR_BGCOLOR);
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _bodyElementImpl]->setAttribute(ATTR_BGCOLOR, bgColor);
}

- (NSString *)link
{
    return [self _bodyElementImpl]->getAttribute(ATTR_LINK);
}

- (void)setLink:(NSString *)link
{
    [self _bodyElementImpl]->setAttribute(ATTR_LINK, link);
}

- (NSString *)text
{
    return [self _bodyElementImpl]->getAttribute(ATTR_TEXT);
}

- (void)setText:(NSString *)text
{
    [self _bodyElementImpl]->setAttribute(ATTR_TEXT, text);
}

- (NSString *)vLink
{
    return [self _bodyElementImpl]->getAttribute(ATTR_VLINK);
}

- (void)setVLink:(NSString *)vLink
{
    [self _bodyElementImpl]->setAttribute(ATTR_VLINK, vLink);
}

@end

@implementation DOMHTMLFormElement

- (HTMLFormElementImpl *)_formElementImpl
{
    return static_cast<HTMLFormElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (DOMHTMLCollection *)elements
{
    HTMLCollectionImpl *collection = new HTMLFormCollectionImpl([self _formElementImpl]);
    return [DOMHTMLCollection _collectionWithImpl:collection];
}

- (long)length
{
    return [self _formElementImpl]->length();
}

- (NSString *)name
{
    return [self _formElementImpl]->getAttribute(ATTR_NAME);
}

- (void)setName:(NSString *)name
{
    [self _formElementImpl]->setAttribute(ATTR_NAME, name);
}

- (NSString *)acceptCharset
{
    return [self _formElementImpl]->getAttribute(ATTR_ACCEPT_CHARSET);
}

- (void)setAcceptCharset:(NSString *)acceptCharset
{
    [self _formElementImpl]->setAttribute(ATTR_ACCEPT_CHARSET, acceptCharset);
}

- (NSString *)action
{
    return [self _formElementImpl]->getAttribute(ATTR_ACTION);
}

- (void)setAction:(NSString *)action
{
    [self _formElementImpl]->setAttribute(ATTR_ACTION, action);
}

- (NSString *)enctype
{
    return [self _formElementImpl]->getAttribute(ATTR_ENCTYPE);
}

- (void)setEnctype:(NSString *)enctype
{
    [self _formElementImpl]->setAttribute(ATTR_ENCTYPE, enctype);
}

- (NSString *)method
{
    return [self _formElementImpl]->getAttribute(ATTR_METHOD);
}

- (void)setMethod:(NSString *)method
{
    [self _formElementImpl]->setAttribute(ATTR_METHOD, method);
}

- (NSString *)target
{
    return [self _formElementImpl]->getAttribute(ATTR_TARGET);
}

- (void)setTarget:(NSString *)target
{
    [self _formElementImpl]->setAttribute(ATTR_TARGET, target);
}

- (void)submit
{
    [self _formElementImpl]->submit(false);
}

- (void)reset
{
    [self _formElementImpl]->reset();
}

@end

@implementation DOMHTMLFormElement (WebCoreInternal)

+ (DOMHTMLFormElement *)_formElementWithImpl:(HTMLFormElementImpl *)impl
{
    return static_cast<DOMHTMLFormElement *>([DOMNode _nodeWithImpl:impl]);
}

@end

@implementation DOMHTMLIsIndexElement

- (HTMLIsIndexElementImpl *)_isIndexElementImpl
{
    return static_cast<HTMLIsIndexElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWithImpl:[self _isIndexElementImpl]->form()];
}

- (NSString *)prompt
{
    return [self _isIndexElementImpl]->getAttribute(ATTR_PROMPT);
}

- (void)setPrompt:(NSString *)prompt
{
    [self _isIndexElementImpl]->setAttribute(ATTR_PROMPT, prompt);
}

@end

@implementation DOMHTMLSelectElement

- (HTMLSelectElementImpl *)_selectElementImpl
{
    return static_cast<HTMLSelectElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)type
{
    return [self _selectElementImpl]->type();
}

- (long)selectedIndex
{
    return [self _selectElementImpl]->selectedIndex();
}

- (void)setSelectedIndex:(long)selectedIndex
{
    [self _selectElementImpl]->setSelectedIndex(selectedIndex);
}

- (NSString *)value
{
    return [self _selectElementImpl]->value();
}

- (void)setValue:(NSString *)value
{
    DOMString s(value);
    [self _selectElementImpl]->setValue(s.implementation());
}

- (long)length
{
    return [self _selectElementImpl]->length();
}

- (void)setLength:(long)length
{
    // FIXME: Not yet clear what to do about this one.
    // There's some JavaScript-specific hackery in the JavaScript bindings for this.
    //[self _selectElementImpl]->setLength(length);
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWithImpl:[self _selectElementImpl]->form()];
}

- (DOMHTMLOptionsCollection *)options
{
    return [DOMHTMLOptionsCollection _optionsCollectionWithImpl:[self _selectElementImpl]->options()];
}

- (BOOL)disabled
{
    return ![self _selectElementImpl]->getAttribute(ATTR_DISABLED).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _selectElementImpl]->setAttribute(ATTR_DISABLED, disabled ? "" : 0);
}

- (BOOL)multiple
{
    return ![self _selectElementImpl]->getAttribute(ATTR_MULTIPLE).isNull();
}

- (void)setMultiple:(BOOL)multiple
{
    [self _selectElementImpl]->setAttribute(ATTR_MULTIPLE, multiple ? "" : 0);
}

- (NSString *)name
{
    return [self _selectElementImpl]->name();
}

- (void)setName:(NSString *)name
{
    [self _selectElementImpl]->setName(name);
}

- (long)size
{
    return [self _selectElementImpl]->getAttribute(ATTR_SIZE).toInt();
}

- (void)setSize:(long)size
{
	DOMString value(QString::number(size));
    [self _selectElementImpl]->setAttribute(ATTR_SIZE, value);
}

- (long)tabIndex
{
    return [self _selectElementImpl]->tabIndex();
}

- (void)setTabIndex:(long)tabIndex
{
    [self _selectElementImpl]->setTabIndex(tabIndex);
}

- (void)add:(DOMHTMLElement *)element :(DOMHTMLElement *)before
{
    [self _selectElementImpl]->add([element _HTMLElementImpl], [before _HTMLElementImpl]);
}

- (void)remove:(long)index
{
    [self _selectElementImpl]->remove(index);
}

- (void)blur
{
    [self _selectElementImpl]->blur();
}

- (void)focus
{
    [self _selectElementImpl]->focus();
}

@end

@implementation DOMHTMLOptGroupElement

- (HTMLOptGroupElementImpl *)_optGroupElementImpl
{
    return static_cast<HTMLOptGroupElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)disabled
{
    return ![self _optGroupElementImpl]->getAttribute(ATTR_DISABLED).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _optGroupElementImpl]->setAttribute(ATTR_DISABLED, disabled ? "" : 0);
}

- (NSString *)label
{
    return [self _optGroupElementImpl]->getAttribute(ATTR_LABEL);
}

- (void)setLabel:(NSString *)label
{
    [self _optGroupElementImpl]->setAttribute(ATTR_LABEL, label);
}

@end

@implementation DOMHTMLOptionElement

- (HTMLOptionElementImpl *)_optionElementImpl
{
    return static_cast<HTMLOptionElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWithImpl:[self _optionElementImpl]->form()];
}

- (BOOL)defaultSelected
{
    return ![self _optionElementImpl]->getAttribute(ATTR_SELECTED).isNull();
}

- (void)setDefaultSelected:(BOOL)defaultSelected
{
    [self _optionElementImpl]->setAttribute(ATTR_SELECTED, defaultSelected ? "" : 0);
}

- (NSString *)text
{
    return [self _optionElementImpl]->text();
}

- (long)index
{
    return [self _optionElementImpl]->index();
}

- (BOOL)disabled
{
    return ![self _optionElementImpl]->getAttribute(ATTR_DISABLED).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _optionElementImpl]->setAttribute(ATTR_DISABLED, disabled ? "" : 0);
}

- (NSString *)label
{
    return [self _optionElementImpl]->getAttribute(ATTR_LABEL);
}

- (void)setLabel:(NSString *)label
{
    [self _optionElementImpl]->setAttribute(ATTR_LABEL, label);
}

- (BOOL)selected
{
    return [self _optionElementImpl]->selected();
}

- (void)setSelected:(BOOL)selected
{
    [self _optionElementImpl]->setSelected(selected);
}

- (NSString *)value
{
    return [self _optionElementImpl]->value();
}

- (void)setValue:(NSString *)value
{
    DOMString string = value;
    [self _optionElementImpl]->setValue(string.implementation());
}

@end

@implementation DOMHTMLInputElement

- (HTMLInputElementImpl *)_inputElementImpl
{
    return static_cast<HTMLInputElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)defaultValue
{
    return [self _inputElementImpl]->getAttribute(ATTR_VALUE);
}

- (void)setDefaultValue:(NSString *)defaultValue
{
    [self _inputElementImpl]->setAttribute(ATTR_VALUE, defaultValue);
}

- (BOOL)defaultChecked
{
    return [self _inputElementImpl]->getAttribute(ATTR_CHECKED).isNull();
}

- (void)setDefaultChecked:(BOOL)defaultChecked
{
    [self _inputElementImpl]->setAttribute(ATTR_CHECKED, defaultChecked ? "" : 0);
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWithImpl:[self _inputElementImpl]->form()];
}

- (NSString *)accept
{
    return [self _inputElementImpl]->getAttribute(ATTR_ACCEPT);
}

- (void)setAccept:(NSString *)accept
{
    [self _inputElementImpl]->setAttribute(ATTR_ACCEPT, accept);
}

- (NSString *)accessKey
{
    return [self _inputElementImpl]->getAttribute(ATTR_ACCESSKEY);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _inputElementImpl]->setAttribute(ATTR_ACCESSKEY, accessKey);
}

- (NSString *)align
{
    return [self _inputElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _inputElementImpl]->setAttribute(ATTR_ALIGN, align);
}

- (NSString *)alt
{
    return [self _inputElementImpl]->getAttribute(ATTR_ALT);
}

- (void)setAlt:(NSString *)alt
{
    [self _inputElementImpl]->setAttribute(ATTR_ALT, alt);
}

- (BOOL)checked
{
    return [self _inputElementImpl]->checked();
}

- (void)setChecked:(BOOL)checked
{
    return [self _inputElementImpl]->setChecked(checked);
}

- (BOOL)disabled
{
    return [self _inputElementImpl]->getAttribute(ATTR_DISABLED).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _inputElementImpl]->setAttribute(ATTR_DISABLED, disabled ? "" : 0);
}

- (long)maxLength
{
    return [self _inputElementImpl]->getAttribute(ATTR_MAXLENGTH).toInt();
}

- (void)setMaxLength:(long)maxLength
{
    DOMString value(QString::number(maxLength));
    [self _inputElementImpl]->setAttribute(ATTR_MAXLENGTH, value);
}

- (NSString *)name
{
    return [self _inputElementImpl]->name();
}

- (void)setName:(NSString *)name
{
    [self _inputElementImpl]->setName(name);
}

- (BOOL)readOnly
{
    return [self _inputElementImpl]->getAttribute(ATTR_READONLY).isNull();
}

- (void)setReadOnly:(BOOL)readOnly
{
    [self _inputElementImpl]->setAttribute(ATTR_READONLY, readOnly ? "" : 0);
}

- (NSString *)size
{
    return [self _inputElementImpl]->getAttribute(ATTR_SIZE);
}

- (void)setSize:(NSString *)size
{
    [self _inputElementImpl]->setAttribute(ATTR_SIZE, size);
}

- (NSString *)src
{
    return [self _inputElementImpl]->src();
}

- (void)setSrc:(NSString *)src
{
    [self _inputElementImpl]->setAttribute(ATTR_SRC, src);
}

- (long)tabIndex
{
    return [self _inputElementImpl]->tabIndex();
}

- (void)setTabIndex:(long)tabIndex
{
    [self _inputElementImpl]->setTabIndex(tabIndex);
}

- (NSString *)type
{
    return [self _inputElementImpl]->type();
}

- (void)setType:(NSString *)type
{
    [self _inputElementImpl]->setType(type);
}

- (NSString *)useMap
{
    return [self _inputElementImpl]->getAttribute(ATTR_USEMAP);
}

- (void)setUseMap:(NSString *)useMap
{
    [self _inputElementImpl]->setAttribute(ATTR_USEMAP, useMap);
}

- (NSString *)value
{
    return [self _inputElementImpl]->value();
}

- (void)setValue:(NSString *)value
{
    [self _inputElementImpl]->setValue(value);
}

- (void)blur
{
    [self _inputElementImpl]->blur();
}

- (void)focus
{
    [self _inputElementImpl]->focus();
}

- (void)select
{
    [self _inputElementImpl]->select();
}

- (void)click
{
    [self _inputElementImpl]->click(false);
}

@end

@implementation DOMHTMLTextAreaElement

- (HTMLTextAreaElementImpl *)_textAreaElementImpl
{
    return static_cast<HTMLTextAreaElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)defaultValue
{
    return [self _textAreaElementImpl]->defaultValue();
}

- (void)setDefaultValue:(NSString *)defaultValue
{
    [self _textAreaElementImpl]->setDefaultValue(defaultValue);
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWithImpl:[self _textAreaElementImpl]->form()];
}

- (NSString *)accessKey
{
    return [self _textAreaElementImpl]->getAttribute(ATTR_ACCESSKEY);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _textAreaElementImpl]->setAttribute(ATTR_ACCESSKEY, accessKey);
}

- (long)cols
{
    return [self _textAreaElementImpl]->getAttribute(ATTR_COLS).toInt();
}

- (void)setCols:(long)cols
{
    DOMString value(QString::number(cols));
    [self _textAreaElementImpl]->setAttribute(ATTR_COLS, value);
}

- (BOOL)disabled
{
    return [self _textAreaElementImpl]->getAttribute(ATTR_DISABLED).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _textAreaElementImpl]->setAttribute(ATTR_DISABLED, disabled ? "" : 0);
}

- (NSString *)name
{
    return [self _textAreaElementImpl]->name();
}

- (void)setName:(NSString *)name
{
    [self _textAreaElementImpl]->setName(name);
}

- (BOOL)readOnly
{
    return [self _textAreaElementImpl]->getAttribute(ATTR_READONLY).isNull();
}

- (void)setReadOnly:(BOOL)readOnly
{
    [self _textAreaElementImpl]->setAttribute(ATTR_READONLY, readOnly ? "" : 0);
}

- (long)rows
{
    return [self _textAreaElementImpl]->getAttribute(ATTR_ROWS).toInt();
}

- (void)setRows:(long)rows
{
	DOMString value(QString::number(rows));
    [self _textAreaElementImpl]->setAttribute(ATTR_ROWS, value);
}

- (long)tabIndex
{
    return [self _textAreaElementImpl]->tabIndex();
}

- (void)setTabIndex:(long)tabIndex
{
    [self _textAreaElementImpl]->setTabIndex(tabIndex);
}

- (NSString *)type
{
    return [self _textAreaElementImpl]->type();
}

- (void)setType:(NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)value
{
    return [self _textAreaElementImpl]->value();
}

- (void)setValue:(NSString *)value
{
    [self _textAreaElementImpl]->setValue(value);
}

- (void)blur
{
    [self _textAreaElementImpl]->blur();
}

- (void)focus
{
    [self _textAreaElementImpl]->focus();
}

- (void)select
{
    [self _textAreaElementImpl]->select();
}

@end

@implementation DOMHTMLButtonElement

- (HTMLButtonElementImpl *)_buttonElementImpl
{
    return static_cast<HTMLButtonElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWithImpl:[self _buttonElementImpl]->form()];
}

- (NSString *)accessKey
{
    return [self _buttonElementImpl]->getAttribute(ATTR_ACCESSKEY);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _buttonElementImpl]->setAttribute(ATTR_ACCESSKEY, accessKey);
}

- (BOOL)disabled
{
    return [self _buttonElementImpl]->getAttribute(ATTR_DISABLED).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _buttonElementImpl]->setAttribute(ATTR_DISABLED, disabled ? "" : 0);
}

- (NSString *)name
{
    return [self _buttonElementImpl]->name();
}

- (void)setName:(NSString *)name
{
    [self _buttonElementImpl]->setName(name);
}

- (long)tabIndex
{
    return [self _buttonElementImpl]->tabIndex();
}

- (void)setTabIndex:(long)tabIndex
{
    [self _buttonElementImpl]->setTabIndex(tabIndex);
}

- (NSString *)type
{
    return [self _buttonElementImpl]->type();
}

- (NSString *)value
{
    return [self _buttonElementImpl]->getAttribute(ATTR_VALUE);
}

- (void)setValue:(NSString *)value
{
    [self _buttonElementImpl]->setAttribute(ATTR_VALUE, value);
}

@end

@implementation DOMHTMLLabelElement

- (HTMLLabelElementImpl *)_labelElementImpl
{
    return static_cast<HTMLLabelElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (DOMHTMLFormElement *)form
{
    ElementImpl *formElement = [self _labelElementImpl]->formElement();
    if (!formElement)
        return 0;
    return [DOMHTMLFormElement _formElementWithImpl:static_cast<HTMLGenericFormElementImpl *>(formElement)->form()];
}

- (NSString *)accessKey
{
    return [self _labelElementImpl]->getAttribute(ATTR_ACCESSKEY);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _labelElementImpl]->setAttribute(ATTR_ACCESSKEY, accessKey);
}

- (NSString *)htmlFor
{
    return [self _labelElementImpl]->getAttribute(ATTR_FOR);
}

- (void)setHtmlFor:(NSString *)htmlFor
{
    [self _labelElementImpl]->setAttribute(ATTR_FOR, htmlFor);
}

@end

@implementation DOMHTMLFieldSetElement

- (HTMLFieldSetElementImpl *)_fieldSetElementImpl
{
    return static_cast<HTMLFieldSetElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWithImpl:[self _fieldSetElementImpl]->form()];
}

@end

@implementation DOMHTMLLegendElement

- (HTMLLegendElementImpl *)_legendElementImpl
{
    return static_cast<HTMLLegendElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWithImpl:[self _legendElementImpl]->form()];
}

- (NSString *)accessKey
{
    return [self _legendElementImpl]->getAttribute(ATTR_ACCESSKEY);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _legendElementImpl]->setAttribute(ATTR_ACCESSKEY, accessKey);
}

- (NSString *)align
{
    return [self _legendElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _legendElementImpl]->setAttribute(ATTR_ALIGN, align);
}

@end

@implementation DOMHTMLUListElement

- (HTMLUListElementImpl *)_uListElementImpl
{
    return static_cast<HTMLUListElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)compact
{
    return [self _uListElementImpl]->getAttribute(ATTR_COMPACT).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _uListElementImpl]->setAttribute(ATTR_COMPACT, compact ? "" : 0);
}

- (NSString *)type
{
    return [self _uListElementImpl]->getAttribute(ATTR_TYPE);
}

- (void)setType:(NSString *)type
{
    [self _uListElementImpl]->setAttribute(ATTR_TYPE, type);
}

@end

@implementation DOMHTMLOListElement

- (HTMLOListElementImpl *)_oListElementImpl
{
    return static_cast<HTMLOListElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)compact
{
    return [self _oListElementImpl]->getAttribute(ATTR_COMPACT).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _oListElementImpl]->setAttribute(ATTR_COMPACT, compact ? "" : 0);
}

- (long)start
{
    return [self _oListElementImpl]->getAttribute(ATTR_START).toInt();
}

- (void)setStart:(long)start
{
	DOMString value(QString::number(start));
    [self _oListElementImpl]->setAttribute(ATTR_START, value);
}

- (NSString *)type
{
    return [self _oListElementImpl]->getAttribute(ATTR_TYPE);
}

- (void)setType:(NSString *)type
{
    [self _oListElementImpl]->setAttribute(ATTR_TYPE, type);
}

@end

@implementation DOMHTMLDListElement

- (HTMLDListElementImpl *)_dListElementImpl
{
    return static_cast<HTMLDListElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)compact
{
    return [self _dListElementImpl]->getAttribute(ATTR_COMPACT).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _dListElementImpl]->setAttribute(ATTR_COMPACT, compact ? "" : 0);
}

@end

@implementation DOMHTMLDirectoryElement

- (HTMLDirectoryElementImpl *)_directoryListElementImpl
{
    return static_cast<HTMLDirectoryElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)compact
{
    return [self _directoryListElementImpl]->getAttribute(ATTR_COMPACT).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _directoryListElementImpl]->setAttribute(ATTR_COMPACT, compact ? "" : 0);
}

@end

@implementation DOMHTMLMenuElement

- (HTMLMenuElementImpl *)_menuListElementImpl
{
    return static_cast<HTMLMenuElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)compact
{
    return [self _menuListElementImpl]->getAttribute(ATTR_COMPACT).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _menuListElementImpl]->setAttribute(ATTR_COMPACT, compact ? "" : 0);
}

@end

@implementation DOMHTMLLIElement

- (HTMLLIElementImpl *)_liElementImpl
{
    return static_cast<HTMLLIElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)type
{
    return [self _liElementImpl]->getAttribute(ATTR_TYPE);
}

- (void)setType:(NSString *)type
{
    [self _liElementImpl]->setAttribute(ATTR_TYPE, type);
}

- (long)value
{
    return [self _liElementImpl]->getAttribute(ATTR_START).toInt();
}

- (void)setValue:(long)value
{
	DOMString string(QString::number(value));
    [self _liElementImpl]->setAttribute(ATTR_VALUE, string);
}

@end

@implementation DOMHTMLQuoteElement

- (HTMLElementImpl *)_quoteElementImpl
{
    return static_cast<HTMLElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)cite
{
    return [self _quoteElementImpl]->getAttribute(ATTR_CITE);
}

- (void)setCite:(NSString *)cite
{
    [self _quoteElementImpl]->setAttribute(ATTR_CITE, cite);
}

@end

@implementation DOMHTMLDivElement

- (HTMLDivElementImpl *)_divElementImpl
{
    return static_cast<HTMLDivElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _divElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _divElementImpl]->setAttribute(ATTR_ALIGN, align);
}

@end

@implementation DOMHTMLParagraphElement

- (HTMLParagraphElementImpl *)_paragraphElementImpl
{
    return static_cast<HTMLParagraphElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _paragraphElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _paragraphElementImpl]->setAttribute(ATTR_ALIGN, align);
}

@end

@implementation DOMHTMLHeadingElement

- (HTMLHeadingElementImpl *)_headingElementImpl
{
    return static_cast<HTMLHeadingElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _headingElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _headingElementImpl]->setAttribute(ATTR_ALIGN, align);
}

@end

@implementation DOMHTMLPreElement

- (HTMLPreElementImpl *)_preElementImpl
{
    return static_cast<HTMLPreElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (long)width
{
    return [self _preElementImpl]->getAttribute(ATTR_WIDTH).toInt();
}

- (void)setWidth:(long)width
{
    DOMString string(QString::number(width));
    [self _preElementImpl]->setAttribute(ATTR_WIDTH, string);
}

@end

@implementation DOMHTMLBRElement

- (HTMLBRElementImpl *)_BRElementImpl
{
    return static_cast<HTMLBRElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)clear
{
    return [self _BRElementImpl]->getAttribute(ATTR_CLEAR);
}

- (void)setClear:(NSString *)clear
{
    [self _BRElementImpl]->setAttribute(ATTR_CLEAR, clear);
}

@end

@implementation DOMHTMLBaseFontElement

- (HTMLBaseFontElementImpl *)_baseFontElementImpl
{
    return static_cast<HTMLBaseFontElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)color
{
    return [self _baseFontElementImpl]->getAttribute(ATTR_COLOR);
}

- (void)setColor:(NSString *)color
{
    [self _baseFontElementImpl]->setAttribute(ATTR_COLOR, color);
}

- (NSString *)face
{
    return [self _baseFontElementImpl]->getAttribute(ATTR_FACE);
}

- (void)setFace:(NSString *)face
{
    [self _baseFontElementImpl]->setAttribute(ATTR_FACE, face);
}

- (NSString *)size
{
    return [self _baseFontElementImpl]->getAttribute(ATTR_SIZE);
}

- (void)setSize:(NSString *)size
{
    [self _baseFontElementImpl]->setAttribute(ATTR_SIZE, size);
}

@end

@implementation DOMHTMLFontElement

- (HTMLFontElementImpl *)_fontElementImpl
{
    return static_cast<HTMLFontElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)color
{
    return [self _fontElementImpl]->getAttribute(ATTR_COLOR);
}

- (void)setColor:(NSString *)color
{
    [self _fontElementImpl]->setAttribute(ATTR_COLOR, color);
}

- (NSString *)face
{
    return [self _fontElementImpl]->getAttribute(ATTR_FACE);
}

- (void)setFace:(NSString *)face
{
    [self _fontElementImpl]->setAttribute(ATTR_FACE, face);
}

- (NSString *)size
{
    return [self _fontElementImpl]->getAttribute(ATTR_SIZE);
}

- (void)setSize:(NSString *)size
{
    [self _fontElementImpl]->setAttribute(ATTR_SIZE, size);
}

@end

@implementation DOMHTMLHRElement

- (HTMLHRElementImpl *)_HRElementImpl
{
    return static_cast<HTMLHRElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _HRElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _HRElementImpl]->setAttribute(ATTR_ALIGN, align);
}

- (BOOL)noShade
{
    return [self _HRElementImpl]->getAttribute(ATTR_NOSHADE).isNull();
}

- (void)setNoShade:(BOOL)noShade
{
    [self _HRElementImpl]->setAttribute(ATTR_CHECKED, noShade ? "" : 0);
}

- (NSString *)size
{
    return [self _HRElementImpl]->getAttribute(ATTR_SIZE);
}

- (void)setSize:(NSString *)size
{
    [self _HRElementImpl]->setAttribute(ATTR_SIZE, size);
}

- (NSString *)width
{
    return [self _HRElementImpl]->getAttribute(ATTR_WIDTH);
}

- (void)setWidth:(NSString *)width
{
    [self _HRElementImpl]->setAttribute(ATTR_WIDTH, width);
}

@end

@implementation DOMHTMLModElement

- (HTMLElementImpl *)_modElementImpl
{
    return static_cast<HTMLElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)cite
{
    return [self _modElementImpl]->getAttribute(ATTR_CITE);
}

- (void)setCite:(NSString *)cite
{
    [self _modElementImpl]->setAttribute(ATTR_CITE, cite);
}

- (NSString *)dateTime
{
    return [self _modElementImpl]->getAttribute(ATTR_DATETIME);
}

- (void)setDateTime:(NSString *)dateTime
{
    [self _modElementImpl]->setAttribute(ATTR_DATETIME, dateTime);
}

@end

@implementation DOMHTMLAnchorElement

- (HTMLAnchorElementImpl *)_anchorElementImpl
{
    return static_cast<HTMLAnchorElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)accessKey
{
    return [self _anchorElementImpl]->getAttribute(ATTR_ACCESSKEY);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _anchorElementImpl]->setAttribute(ATTR_ACCESSKEY, accessKey);
}

- (NSString *)charset
{
    return [self _anchorElementImpl]->getAttribute(ATTR_CHARSET);
}

- (void)setCharset:(NSString *)charset
{
    [self _anchorElementImpl]->setAttribute(ATTR_CHARSET, charset);
}

- (NSString *)coords
{
    return [self _anchorElementImpl]->getAttribute(ATTR_COORDS);
}

- (void)setCoords:(NSString *)coords
{
    [self _anchorElementImpl]->setAttribute(ATTR_COORDS, coords);
}

- (NSString *)href
{
    return [self _anchorElementImpl]->href();
}

- (void)setHref:(NSString *)href
{
    [self _anchorElementImpl]->setAttribute(ATTR_HREF, href);
}

- (NSString *)hreflang
{
    return [self _anchorElementImpl]->getAttribute(ATTR_HREFLANG);
}

- (void)setHreflang:(NSString *)hreflang
{
    [self _anchorElementImpl]->setAttribute(ATTR_HREFLANG, hreflang);
}

- (NSString *)name
{
    return [self _anchorElementImpl]->getAttribute(ATTR_NAME);
}

- (void)setName:(NSString *)name
{
    [self _anchorElementImpl]->setAttribute(ATTR_NAME, name);
}

- (NSString *)rel
{
    return [self _anchorElementImpl]->getAttribute(ATTR_REL);
}

- (void)setRel:(NSString *)rel
{
    [self _anchorElementImpl]->setAttribute(ATTR_REL, rel);
}

- (NSString *)rev
{
    return [self _anchorElementImpl]->getAttribute(ATTR_REV);
}

- (void)setRev:(NSString *)rev
{
    [self _anchorElementImpl]->setAttribute(ATTR_REV, rev);
}

- (NSString *)shape
{
    return [self _anchorElementImpl]->getAttribute(ATTR_SHAPE);
}

- (void)setShape:(NSString *)shape
{
    [self _anchorElementImpl]->setAttribute(ATTR_SHAPE, shape);
}

- (long)tabIndex
{
    return [self _anchorElementImpl]->getAttribute(ATTR_TABINDEX).toInt();
}

- (void)setTabIndex:(long)tabIndex
{
	DOMString string(QString::number(tabIndex));
    [self _anchorElementImpl]->setAttribute(ATTR_TABINDEX, string);
}

- (NSString *)target
{
    return [self _anchorElementImpl]->getAttribute(ATTR_TARGET);
}

- (void)setTarget:(NSString *)target
{
    [self _anchorElementImpl]->setAttribute(ATTR_TARGET, target);
}

- (NSString *)type
{
    return [self _anchorElementImpl]->getAttribute(ATTR_TYPE);
}

- (void)setType:(NSString *)type
{
    [self _anchorElementImpl]->setAttribute(ATTR_TYPE, type);
}

- (void)blur
{
    HTMLAnchorElementImpl *impl = [self _anchorElementImpl];
    if (impl->getDocument()->focusNode() == impl)
        impl->getDocument()->setFocusNode(0);
}

- (void)focus
{
    HTMLAnchorElementImpl *impl = [self _anchorElementImpl];
    impl->getDocument()->setFocusNode(static_cast<ElementImpl*>(impl));
}

@end

@implementation DOMHTMLImageElement

- (HTMLImageElementImpl *)_imageElementImpl
{
    return static_cast<HTMLImageElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)name
{
    return [self _imageElementImpl]->getAttribute(ATTR_NAME);
}

- (void)setName:(NSString *)name
{
    [self _imageElementImpl]->setAttribute(ATTR_NAME, name);
}

- (NSString *)align
{
    return [self _imageElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _imageElementImpl]->setAttribute(ATTR_ALIGN, align);
}

- (NSString *)alt
{
    return [self _imageElementImpl]->getAttribute(ATTR_ALT);
}

- (void)setAlt:(NSString *)alt
{
    [self _imageElementImpl]->setAttribute(ATTR_ALT, alt);
}

- (NSString *)border
{
    return [self _imageElementImpl]->getAttribute(ATTR_BORDER);
}

- (void)setBorder:(NSString *)border
{
    [self _imageElementImpl]->setAttribute(ATTR_BORDER, border);
}

- (long)height
{
    return [self _imageElementImpl]->getAttribute(ATTR_HEIGHT).toInt();
}

- (void)setHeight:(long)height
{
    DOMString string(QString::number(height));
    [self _imageElementImpl]->setAttribute(ATTR_HEIGHT, string);
}

- (long)hspace
{
    return [self _imageElementImpl]->getAttribute(ATTR_HSPACE).toInt();
}

- (void)setHspace:(long)hspace
{
    DOMString string(QString::number(hspace));
    [self _imageElementImpl]->setAttribute(ATTR_HSPACE, string);
}

- (BOOL)isMap
{
    return [self _imageElementImpl]->getAttribute(ATTR_ISMAP).isNull();
}

- (void)setIsMap:(BOOL)isMap
{
    [self _imageElementImpl]->setAttribute(ATTR_ISMAP, isMap ? "" : 0);
}

- (NSString *)longDesc
{
    return [self _imageElementImpl]->getAttribute(ATTR_LONGDESC);
}

- (void)setLongDesc:(NSString *)longDesc
{
    [self _imageElementImpl]->setAttribute(ATTR_LONGDESC, longDesc);
}

- (NSString *)src
{
    return [self _imageElementImpl]->src();
}

- (void)setSrc:(NSString *)src
{
    [self _imageElementImpl]->setAttribute(ATTR_SRC, src);
}

- (NSString *)useMap
{
    return [self _imageElementImpl]->getAttribute(ATTR_USEMAP);
}

- (void)setUseMap:(NSString *)useMap
{
    [self _imageElementImpl]->setAttribute(ATTR_USEMAP, useMap);
}

- (long)vspace
{
    return [self _imageElementImpl]->getAttribute(ATTR_VSPACE).toInt();
}

- (void)setVspace:(long)vspace
{
    DOMString string(QString::number(vspace));
    [self _imageElementImpl]->setAttribute(ATTR_VSPACE, string);
}

- (long)width
{
    return [self _imageElementImpl]->getAttribute(ATTR_WIDTH).toInt();
}

- (void)setWidth:(long)width
{
    DOMString string(QString::number(width));
    [self _imageElementImpl]->setAttribute(ATTR_WIDTH, string);
}

@end

@implementation DOMHTMLObjectElement

- (HTMLObjectElementImpl *)_objectElementImpl
{
    return static_cast<HTMLObjectElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWithImpl:[self _objectElementImpl]->form()];
}

- (NSString *)code
{
    return [self _objectElementImpl]->getAttribute(ATTR_CODE);
}

- (void)setCode:(NSString *)code
{
    [self _objectElementImpl]->setAttribute(ATTR_CODE, code);
}

- (NSString *)align
{
    return [self _objectElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _objectElementImpl]->setAttribute(ATTR_ALIGN, align);
}

- (NSString *)archive
{
    return [self _objectElementImpl]->getAttribute(ATTR_ARCHIVE);
}

- (void)setArchive:(NSString *)archive
{
    [self _objectElementImpl]->setAttribute(ATTR_ARCHIVE, archive);
}

- (NSString *)border
{
    return [self _objectElementImpl]->getAttribute(ATTR_BORDER);
}

- (void)setBorder:(NSString *)border
{
    [self _objectElementImpl]->setAttribute(ATTR_BORDER, border);
}

- (NSString *)codeBase
{
    return [self _objectElementImpl]->getAttribute(ATTR_CODEBASE);
}

- (void)setCodeBase:(NSString *)codeBase
{
    [self _objectElementImpl]->setAttribute(ATTR_CODEBASE, codeBase);
}

- (NSString *)codeType
{
    return [self _objectElementImpl]->getAttribute(ATTR_CODETYPE);
}

- (void)setCodeType:(NSString *)codeType
{
    [self _objectElementImpl]->setAttribute(ATTR_CODETYPE, codeType);
}

- (NSString *)data
{
    return [self _objectElementImpl]->getAttribute(ATTR_DATA);
}

- (void)setData:(NSString *)data
{
    [self _objectElementImpl]->setAttribute(ATTR_DATA, data);
}

- (BOOL)declare
{
    return [self _objectElementImpl]->getAttribute(ATTR_DECLARE).isNull();
}

- (void)setDeclare:(BOOL)declare
{
    [self _objectElementImpl]->setAttribute(ATTR_DECLARE, declare ? "" : 0);
}

- (NSString *)height
{
    return [self _objectElementImpl]->getAttribute(ATTR_HEIGHT);
}

- (void)setHeight:(NSString *)height
{
    [self _objectElementImpl]->setAttribute(ATTR_HEIGHT, height);
}

- (long)hspace
{
    return [self _objectElementImpl]->getAttribute(ATTR_HSPACE).toInt();
}

- (void)setHspace:(long)hspace
{
    DOMString string(QString::number(hspace));
    [self _objectElementImpl]->setAttribute(ATTR_HSPACE, string);
}

- (NSString *)name
{
    return [self _objectElementImpl]->getAttribute(ATTR_NAME);
}

- (void)setName:(NSString *)name
{
    [self _objectElementImpl]->setAttribute(ATTR_NAME, name);
}

- (NSString *)standby
{
    return [self _objectElementImpl]->getAttribute(ATTR_STANDBY);
}

- (void)setStandby:(NSString *)standby
{
    [self _objectElementImpl]->setAttribute(ATTR_STANDBY, standby);
}

- (long)tabIndex
{
    return [self _objectElementImpl]->getAttribute(ATTR_TABINDEX).toInt();
}

- (void)setTabIndex:(long)tabIndex
{
    DOMString string(QString::number(tabIndex));
    [self _objectElementImpl]->setAttribute(ATTR_TABINDEX, string);
}

- (NSString *)type
{
    return [self _objectElementImpl]->getAttribute(ATTR_TYPE);
}

- (void)setType:(NSString *)type
{
    [self _objectElementImpl]->setAttribute(ATTR_TYPE, type);
}

- (NSString *)useMap
{
    return [self _objectElementImpl]->getAttribute(ATTR_USEMAP);
}

- (void)setUseMap:(NSString *)useMap
{
    [self _objectElementImpl]->setAttribute(ATTR_USEMAP, useMap);
}

- (long)vspace
{
    return [self _objectElementImpl]->getAttribute(ATTR_VSPACE).toInt();
}

- (void)setVspace:(long)vspace
{
    DOMString string(QString::number(vspace));
    [self _objectElementImpl]->setAttribute(ATTR_VSPACE, string);
}

- (NSString *)width
{
    return [self _objectElementImpl]->getAttribute(ATTR_WIDTH);
}

- (void)setWidth:(NSString *)width
{
    [self _objectElementImpl]->setAttribute(ATTR_WIDTH, width);
}

- (DOMDocument *)contentDocument
{
    return [DOMDocument _documentWithImpl:[self _objectElementImpl]->contentDocument()];
}

@end

@implementation DOMHTMLParamElement

- (HTMLParamElementImpl *)_paramElementImpl
{
    return static_cast<HTMLParamElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)name
{
    return [self _paramElementImpl]->getAttribute(ATTR_NAME);
}

- (void)setName:(NSString *)name
{
    [self _paramElementImpl]->setAttribute(ATTR_NAME, name);
}

- (NSString *)type
{
    return [self _paramElementImpl]->getAttribute(ATTR_TYPE);
}

- (void)setType:(NSString *)type
{
    [self _paramElementImpl]->setAttribute(ATTR_TYPE, type);
}

- (NSString *)value
{
    return [self _paramElementImpl]->getAttribute(ATTR_VALUE);
}

- (void)setValue:(NSString *)value
{
    [self _paramElementImpl]->setAttribute(ATTR_VALUE, value);
}

- (NSString *)valueType
{
    return [self _paramElementImpl]->getAttribute(ATTR_VALUETYPE);
}

- (void)setValueType:(NSString *)valueType
{
    [self _paramElementImpl]->setAttribute(ATTR_VALUETYPE, valueType);
}

@end

@implementation DOMHTMLAppletElement

- (HTMLAppletElementImpl *)_appletElementImpl
{
    return static_cast<HTMLAppletElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _appletElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _appletElementImpl]->setAttribute(ATTR_ALIGN, align);
}

- (NSString *)alt
{
    return [self _appletElementImpl]->getAttribute(ATTR_ALT);
}

- (void)setAlt:(NSString *)alt
{
    [self _appletElementImpl]->setAttribute(ATTR_ALT, alt);
}

- (NSString *)archive
{
    return [self _appletElementImpl]->getAttribute(ATTR_ARCHIVE);
}

- (void)setArchive:(NSString *)archive
{
    [self _appletElementImpl]->setAttribute(ATTR_ARCHIVE, archive);
}

- (NSString *)code
{
    return [self _appletElementImpl]->getAttribute(ATTR_CODE);
}

- (void)setCode:(NSString *)code
{
    [self _appletElementImpl]->setAttribute(ATTR_CODE, code);
}

- (NSString *)codeBase
{
    return [self _appletElementImpl]->getAttribute(ATTR_CODEBASE);
}

- (void)setCodeBase:(NSString *)codeBase
{
    [self _appletElementImpl]->setAttribute(ATTR_CODEBASE, codeBase);
}

- (NSString *)height
{
    return [self _appletElementImpl]->getAttribute(ATTR_HEIGHT);
}

- (void)setHeight:(NSString *)height
{
    [self _appletElementImpl]->setAttribute(ATTR_HEIGHT, height);
}

- (long)hspace
{
    return [self _appletElementImpl]->getAttribute(ATTR_HSPACE).toInt();
}

- (void)setHspace:(long)hspace
{
    DOMString string(QString::number(hspace));
    [self _appletElementImpl]->setAttribute(ATTR_HSPACE, string);
}

- (NSString *)name
{
    return [self _appletElementImpl]->getAttribute(ATTR_NAME);
}

- (void)setName:(NSString *)name
{
    [self _appletElementImpl]->setAttribute(ATTR_NAME, name);
}

- (NSString *)object
{
    return [self _appletElementImpl]->getAttribute(ATTR_OBJECT);
}

- (void)setObject:(NSString *)object
{
    [self _appletElementImpl]->setAttribute(ATTR_OBJECT, object);
}

- (long)vspace
{
    return [self _appletElementImpl]->getAttribute(ATTR_VSPACE).toInt();
}

- (void)setVspace:(long)vspace
{
    DOMString string(QString::number(vspace));
    [self _appletElementImpl]->setAttribute(ATTR_VSPACE, string);
}

- (NSString *)width
{
    return [self _appletElementImpl]->getAttribute(ATTR_WIDTH);
}

- (void)setWidth:(NSString *)width
{
    [self _appletElementImpl]->setAttribute(ATTR_WIDTH, width);
}

@end

@implementation DOMHTMLMapElement

- (HTMLMapElementImpl *)_mapElementImpl
{
    return static_cast<HTMLMapElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (DOMHTMLCollection *)areas
{
    HTMLCollectionImpl *collection = new HTMLCollectionImpl([self _mapElementImpl], HTMLCollectionImpl::MAP_AREAS);
    return [DOMHTMLCollection _collectionWithImpl:collection];
}

- (NSString *)name
{
    return [self _mapElementImpl]->getAttribute(ATTR_NAME);
}

- (void)setName:(NSString *)name
{
    [self _mapElementImpl]->setAttribute(ATTR_NAME, name);
}

@end

@implementation DOMHTMLAreaElement

- (HTMLAreaElementImpl *)_areaElementImpl
{
    return static_cast<HTMLAreaElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)accessKey
{
    return [self _areaElementImpl]->getAttribute(ATTR_ACCESSKEY);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _areaElementImpl]->setAttribute(ATTR_ACCESSKEY, accessKey);
}

- (NSString *)alt
{
    return [self _areaElementImpl]->getAttribute(ATTR_ALT);
}

- (void)setAlt:(NSString *)alt
{
    [self _areaElementImpl]->setAttribute(ATTR_ALT, alt);
}

- (NSString *)coords
{
    return [self _areaElementImpl]->getAttribute(ATTR_COORDS);
}

- (void)setCoords:(NSString *)coords
{
    [self _areaElementImpl]->setAttribute(ATTR_COORDS, coords);
}

- (NSString *)href
{
    return [self _areaElementImpl]->href();
}

- (void)setHref:(NSString *)href
{
    [self _areaElementImpl]->setAttribute(ATTR_HREF, href);
}

- (BOOL)noHref
{
    return [self _areaElementImpl]->getAttribute(ATTR_NOHREF).isNull();
}

- (void)setNoHref:(BOOL)noHref
{
    [self _areaElementImpl]->setAttribute(ATTR_NOHREF, noHref ? "" : 0);
}

- (NSString *)shape
{
    return [self _areaElementImpl]->getAttribute(ATTR_SHAPE);
}

- (void)setShape:(NSString *)shape
{
    [self _areaElementImpl]->setAttribute(ATTR_SHAPE, shape);
}

- (long)tabIndex
{
    return [self _areaElementImpl]->getAttribute(ATTR_TABINDEX).toInt();
}

- (void)setTabIndex:(long)tabIndex
{
    DOMString string(QString::number(tabIndex));
    [self _areaElementImpl]->setAttribute(ATTR_TABINDEX, string);
}

- (NSString *)target
{
    return [self _areaElementImpl]->getAttribute(ATTR_TARGET);
}

- (void)setTarget:(NSString *)target
{
    [self _areaElementImpl]->setAttribute(ATTR_TARGET, target);
}

@end

@implementation DOMHTMLScriptElement

- (HTMLScriptElementImpl *)_scriptElementImpl
{
    return static_cast<HTMLScriptElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)text
{
    return [self _scriptElementImpl]->getAttribute(ATTR_TEXT);
}

- (void)setText:(NSString *)text
{
    [self _scriptElementImpl]->setAttribute(ATTR_TEXT, text);
}

- (NSString *)htmlFor
{
    ASSERT_WITH_MESSAGE(0, "not implemented by khtml");
    return nil;
}

- (void)setHtmlFor:(NSString *)htmlFor
{
    ASSERT_WITH_MESSAGE(0, "not implemented by khtml");
}

- (NSString *)event
{
    ASSERT_WITH_MESSAGE(0, "not implemented by khtml");
    return nil;
}

- (void)setEvent:(NSString *)event
{
    ASSERT_WITH_MESSAGE(0, "not implemented by khtml");
}

- (NSString *)charset
{
    return [self _scriptElementImpl]->getAttribute(ATTR_CHARSET);
}

- (void)setCharset:(NSString *)charset
{
    [self _scriptElementImpl]->setAttribute(ATTR_CHARSET, charset);
}

- (BOOL)defer
{
    return [self _scriptElementImpl]->getAttribute(ATTR_DEFER).isNull();
}

- (void)setDefer:(BOOL)defer
{
    [self _scriptElementImpl]->setAttribute(ATTR_DEFER, defer ? "" : 0);
}

- (NSString *)src
{
    return [self _scriptElementImpl]->getAttribute(ATTR_SRC);
}

- (void)setSrc:(NSString *)src
{
    [self _scriptElementImpl]->setAttribute(ATTR_SRC, src);
}

- (NSString *)type
{
    return [self _scriptElementImpl]->getAttribute(ATTR_TYPE);
}

- (void)setType:(NSString *)type
{
    [self _scriptElementImpl]->setAttribute(ATTR_TYPE, type);
}

@end

@implementation DOMHTMLTableCaptionElement

- (NSString *)align
{
    return [self _tableCaptionElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _tableCaptionElementImpl]->setAttribute(ATTR_ALIGN, align);
}

@end

@implementation DOMHTMLTableCaptionElement (WebCoreInternal)

+ (DOMHTMLTableCaptionElement *)_tableCaptionElementWithImpl:(HTMLTableCaptionElementImpl *)impl
{
    return static_cast<DOMHTMLTableCaptionElement *>([DOMNode _nodeWithImpl:impl]);
}

- (HTMLTableCaptionElementImpl *)_tableCaptionElementImpl
{
    return static_cast<HTMLTableCaptionElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

@end

@implementation DOMHTMLTableSectionElement

- (NSString *)align
{
    return [self _tableSectionElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _tableSectionElementImpl]->setAttribute(ATTR_ALIGN, align);
}

- (NSString *)ch
{
    return [self _tableSectionElementImpl]->getAttribute(ATTR_CHAR);
}

- (void)setCh:(NSString *)ch
{
    [self _tableSectionElementImpl]->setAttribute(ATTR_CHAR, ch);
}

- (NSString *)chOff
{
    return [self _tableSectionElementImpl]->getAttribute(ATTR_CHAROFF);
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableSectionElementImpl]->setAttribute(ATTR_CHAROFF, chOff);
}

- (NSString *)vAlign
{
    return [self _tableSectionElementImpl]->getAttribute(ATTR_VALIGN);
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableSectionElementImpl]->setAttribute(ATTR_VALIGN, vAlign);
}

- (DOMHTMLCollection *)rows
{
    HTMLCollectionImpl *collection = new HTMLCollectionImpl([self _tableSectionElementImpl], HTMLCollectionImpl::TABLE_ROWS);
    return [DOMHTMLCollection _collectionWithImpl:collection];
}

- (DOMHTMLElement *)insertRow:(long)index
{
    int exceptioncode = 0;
    HTMLElementImpl *impl = [self _tableSectionElementImpl]->insertRow(index, exceptioncode);
    raiseOnDOMError(exceptioncode);
    return [DOMHTMLElement _elementWithImpl:impl];
}

- (void)deleteRow:(long)index
{
    int exceptioncode = 0;
    [self _tableSectionElementImpl]->deleteRow(index, exceptioncode);
    raiseOnDOMError(exceptioncode);
}

@end

@implementation DOMHTMLTableSectionElement (WebCoreInternal)

+ (DOMHTMLTableSectionElement *)_tableSectionElementWithImpl:(HTMLTableSectionElementImpl *)impl
{
    return static_cast<DOMHTMLTableSectionElement *>([DOMNode _nodeWithImpl:impl]);
}

- (HTMLTableSectionElementImpl *)_tableSectionElementImpl
{
    return static_cast<HTMLTableSectionElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

@end

@implementation DOMHTMLTableElement

- (DOMHTMLTableCaptionElement *)caption
{
    return [DOMHTMLTableCaptionElement _tableCaptionElementWithImpl:[self _tableElementImpl]->caption()];
}

- (void)setCaption:(DOMHTMLTableCaptionElement *)caption
{
    [self _tableElementImpl]->setCaption([caption _tableCaptionElementImpl]);
}

- (DOMHTMLTableSectionElement *)tHead
{
    return [DOMHTMLTableSectionElement _tableSectionElementWithImpl:[self _tableElementImpl]->tHead()];
}

- (void)setTHead:(DOMHTMLTableSectionElement *)tHead
{
    [self _tableElementImpl]->setTHead([tHead _tableSectionElementImpl]);
}

- (DOMHTMLTableSectionElement *)tFoot
{
    return [DOMHTMLTableSectionElement _tableSectionElementWithImpl:[self _tableElementImpl]->tFoot()];
}

- (void)setTFoot:(DOMHTMLTableSectionElement *)tFoot
{
    [self _tableElementImpl]->setTFoot([tFoot _tableSectionElementImpl]);
}

- (DOMHTMLCollection *)rows
{
    HTMLCollectionImpl *collection = new HTMLCollectionImpl([self _tableElementImpl], HTMLCollectionImpl::TABLE_ROWS);
    return [DOMHTMLCollection _collectionWithImpl:collection];
}

- (DOMHTMLCollection *)tBodies
{
    HTMLCollectionImpl *collection = new HTMLCollectionImpl([self _tableElementImpl], HTMLCollectionImpl::TABLE_TBODIES);
    return [DOMHTMLCollection _collectionWithImpl:collection];
}

- (NSString *)align
{
    return [self _tableElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _tableElementImpl]->setAttribute(ATTR_ALIGN, align);
}

- (NSString *)bgColor
{
    return [self _tableElementImpl]->getAttribute(ATTR_BGCOLOR);
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _tableElementImpl]->setAttribute(ATTR_BGCOLOR, bgColor);
}

- (NSString *)border
{
    return [self _tableElementImpl]->getAttribute(ATTR_BORDER);
}

- (void)setBorder:(NSString *)border
{
    [self _tableElementImpl]->setAttribute(ATTR_BORDER, border);
}

- (NSString *)cellPadding
{
    return [self _tableElementImpl]->getAttribute(ATTR_CELLPADDING);
}

- (void)setCellPadding:(NSString *)cellPadding
{
    [self _tableElementImpl]->setAttribute(ATTR_CELLPADDING, cellPadding);
}

- (NSString *)cellSpacing
{
    return [self _tableElementImpl]->getAttribute(ATTR_CELLSPACING);
}

- (void)setCellSpacing:(NSString *)cellSpacing
{
    [self _tableElementImpl]->setAttribute(ATTR_CELLSPACING, cellSpacing);
}

- (NSString *)frameBorders
{
    return [self _tableElementImpl]->getAttribute(ATTR_FRAME);
}

- (void)setFrameBorders:(NSString *)frameBorders
{
    [self _tableElementImpl]->setAttribute(ATTR_FRAME, frameBorders);
}

- (NSString *)rules
{
    return [self _tableElementImpl]->getAttribute(ATTR_RULES);
}

- (void)setRules:(NSString *)rules
{
    [self _tableElementImpl]->setAttribute(ATTR_RULES, rules);
}

- (NSString *)summary
{
    return [self _tableElementImpl]->getAttribute(ATTR_SUMMARY);
}

- (void)setSummary:(NSString *)summary
{
    [self _tableElementImpl]->setAttribute(ATTR_SUMMARY, summary);
}

- (NSString *)width
{
    return [self _tableElementImpl]->getAttribute(ATTR_WIDTH);
}

- (void)setWidth:(NSString *)width
{
    [self _tableElementImpl]->setAttribute(ATTR_WIDTH, width);
}

- (DOMHTMLElement *)createTHead
{
    HTMLTableSectionElementImpl *impl = static_cast<HTMLTableSectionElementImpl *>([self _tableElementImpl]->createTHead());
    return [DOMHTMLTableSectionElement _tableSectionElementWithImpl:impl];
}

- (void)deleteTHead
{
    [self _tableElementImpl]->deleteTHead();
}

- (DOMHTMLElement *)createTFoot
{
    HTMLTableSectionElementImpl *impl = static_cast<HTMLTableSectionElementImpl *>([self _tableElementImpl]->createTFoot());
    return [DOMHTMLTableSectionElement _tableSectionElementWithImpl:impl];
}

- (void)deleteTFoot
{
    [self _tableElementImpl]->deleteTFoot();
}

- (DOMHTMLElement *)createCaption
{
    HTMLTableCaptionElementImpl *impl = static_cast<HTMLTableCaptionElementImpl *>([self _tableElementImpl]->createCaption());
    return [DOMHTMLTableCaptionElement _tableCaptionElementWithImpl:impl];
}

- (void)deleteCaption
{
    [self _tableElementImpl]->deleteCaption();
}

- (DOMHTMLElement *)insertRow:(long)index
{
    int exceptioncode = 0;
    HTMLTableElementImpl *impl = static_cast<HTMLTableElementImpl *>([self _tableElementImpl]->insertRow(index, exceptioncode));
    raiseOnDOMError(exceptioncode);
    return [DOMHTMLTableElement _tableElementWithImpl:impl];
}

- (void)deleteRow:(long)index
{
    int exceptioncode = 0;
    [self _tableElementImpl]->deleteRow(index, exceptioncode);
    raiseOnDOMError(exceptioncode);
}

@end

@implementation DOMHTMLTableElement (WebCoreInternal)

+ (DOMHTMLTableElement *)_tableElementWithImpl:(HTMLTableElementImpl *)impl
{
    return static_cast<DOMHTMLTableElement *>([DOMNode _nodeWithImpl:impl]);
}

- (HTMLTableElementImpl *)_tableElementImpl
{
    return static_cast<HTMLTableElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

@end

@implementation DOMHTMLTableColElement

- (HTMLTableColElementImpl *)_tableColElementImpl
{
    return static_cast<HTMLTableColElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _tableColElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _tableColElementImpl]->setAttribute(ATTR_ALIGN, align);
}

- (NSString *)ch
{
    return [self _tableColElementImpl]->getAttribute(ATTR_CHAR);
}

- (void)setCh:(NSString *)ch
{
    [self _tableColElementImpl]->setAttribute(ATTR_CHAR, ch);
}

- (NSString *)chOff
{
    return [self _tableColElementImpl]->getAttribute(ATTR_CHAROFF);
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableColElementImpl]->setAttribute(ATTR_CHAROFF, chOff);
}

- (long)span
{
    return [self _tableColElementImpl]->getAttribute(ATTR_SPAN).toInt();
}

- (void)setSpan:(long)span
{
    DOMString string(QString::number(span));
    [self _tableColElementImpl]->setAttribute(ATTR_SPAN, string);
}

- (NSString *)vAlign
{
    return [self _tableColElementImpl]->getAttribute(ATTR_VALIGN);
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableColElementImpl]->setAttribute(ATTR_VALIGN, vAlign);
}

- (NSString *)width
{
    return [self _tableColElementImpl]->getAttribute(ATTR_WIDTH);
}

- (void)setWidth:(NSString *)width
{
    [self _tableColElementImpl]->setAttribute(ATTR_WIDTH, width);
}

@end

@implementation DOMHTMLTableRowElement

- (HTMLTableRowElementImpl *)_tableRowElementImpl
{
    return static_cast<HTMLTableRowElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (long)rowIndex
{
    return [self _tableRowElementImpl]->rowIndex();
}

- (long)sectionRowIndex
{
    return [self _tableRowElementImpl]->sectionRowIndex();
}

- (DOMHTMLCollection *)cells
{
    HTMLCollectionImpl *collection = new HTMLCollectionImpl([self _tableRowElementImpl], HTMLCollectionImpl::TR_CELLS);
    return [DOMHTMLCollection _collectionWithImpl:collection];
}

- (NSString *)align
{
    return [self _tableRowElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _tableRowElementImpl]->setAttribute(ATTR_ALIGN, align);
}

- (NSString *)bgColor
{
    return [self _tableRowElementImpl]->getAttribute(ATTR_BGCOLOR);
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _tableRowElementImpl]->setAttribute(ATTR_BGCOLOR, bgColor);
}

- (NSString *)ch
{
    return [self _tableRowElementImpl]->getAttribute(ATTR_CHAR);
}

- (void)setCh:(NSString *)ch
{
    [self _tableRowElementImpl]->setAttribute(ATTR_CHAR, ch);
}

- (NSString *)chOff
{
    return [self _tableRowElementImpl]->getAttribute(ATTR_CHAROFF);
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableRowElementImpl]->setAttribute(ATTR_CHAROFF, chOff);
}

- (NSString *)vAlign
{
    return [self _tableRowElementImpl]->getAttribute(ATTR_VALIGN);
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableRowElementImpl]->setAttribute(ATTR_VALIGN, vAlign);
}

- (DOMHTMLElement *)insertCell:(long)index
{
    int exceptioncode = 0;
    HTMLTableCellElementImpl *impl = static_cast<HTMLTableCellElementImpl *>([self _tableRowElementImpl]->insertCell(index, exceptioncode));
    raiseOnDOMError(exceptioncode);
    return [DOMHTMLTableCellElement _tableCellElementWithImpl:impl];
}

- (void)deleteCell:(long)index
{
    int exceptioncode = 0;
    [self _tableRowElementImpl]->deleteCell(index, exceptioncode);
    raiseOnDOMError(exceptioncode);
}

@end

@implementation DOMHTMLTableCellElement

- (long)cellIndex
{
    return [self _tableCellElementImpl]->cellIndex();
}

- (NSString *)abbr
{
    return [self _tableCellElementImpl]->getAttribute(ATTR_ABBR);
}

- (void)setAbbr:(NSString *)abbr
{
    [self _tableCellElementImpl]->setAttribute(ATTR_ABBR, abbr);
}

- (NSString *)align
{
    return [self _tableCellElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _tableCellElementImpl]->setAttribute(ATTR_ALIGN, align);
}

- (NSString *)axis
{
    return [self _tableCellElementImpl]->getAttribute(ATTR_AXIS);
}

- (void)setAxis:(NSString *)axis
{
    [self _tableCellElementImpl]->setAttribute(ATTR_AXIS, axis);
}

- (NSString *)bgColor
{
    return [self _tableCellElementImpl]->getAttribute(ATTR_BGCOLOR);
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _tableCellElementImpl]->setAttribute(ATTR_BGCOLOR, bgColor);
}

- (NSString *)ch
{
    return [self _tableCellElementImpl]->getAttribute(ATTR_CHAR);
}

- (void)setCh:(NSString *)ch
{
    [self _tableCellElementImpl]->setAttribute(ATTR_CHAR, ch);
}

- (NSString *)chOff
{
    return [self _tableCellElementImpl]->getAttribute(ATTR_CHAROFF);
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableCellElementImpl]->setAttribute(ATTR_CHAROFF, chOff);
}

- (long)colSpan
{
    return [self _tableCellElementImpl]->getAttribute(ATTR_COLSPAN).toInt();
}

- (void)setColSpan:(long)colSpan
{
    DOMString string(QString::number(colSpan));
    [self _tableCellElementImpl]->setAttribute(ATTR_COLSPAN, string);
}

- (NSString *)headers
{
    return [self _tableCellElementImpl]->getAttribute(ATTR_HEADERS);
}

- (void)setHeaders:(NSString *)headers
{
    [self _tableCellElementImpl]->setAttribute(ATTR_HEADERS, headers);
}

- (NSString *)height
{
    return [self _tableCellElementImpl]->getAttribute(ATTR_HEIGHT);
}

- (void)setHeight:(NSString *)height
{
    [self _tableCellElementImpl]->setAttribute(ATTR_HEIGHT, height);
}

- (BOOL)noWrap
{
    return [self _tableCellElementImpl]->getAttribute(ATTR_NOWRAP).isNull();
}

- (void)setNoWrap:(BOOL)noWrap
{
    [self _tableCellElementImpl]->setAttribute(ATTR_NOWRAP, noWrap ? "" : 0);
}

- (long)rowSpan
{
    return [self _tableCellElementImpl]->getAttribute(ATTR_ROWSPAN).toInt();
}

- (void)setRowSpan:(long)rowSpan
{
    DOMString string(QString::number(rowSpan));
    [self _tableCellElementImpl]->setAttribute(ATTR_ROWSPAN, string);
}

- (NSString *)scope
{
    return [self _tableCellElementImpl]->getAttribute(ATTR_SCOPE);
}

- (void)setScope:(NSString *)scope
{
    [self _tableCellElementImpl]->setAttribute(ATTR_SCOPE, scope);
}

- (NSString *)vAlign
{
    return [self _tableCellElementImpl]->getAttribute(ATTR_VALIGN);
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableCellElementImpl]->setAttribute(ATTR_VALIGN, vAlign);
}

- (NSString *)width
{
    return [self _tableCellElementImpl]->getAttribute(ATTR_WIDTH);
}

- (void)setWidth:(NSString *)width
{
    [self _tableCellElementImpl]->setAttribute(ATTR_WIDTH, width);
}

@end

@implementation DOMHTMLTableCellElement (WebCoreInternal)

+ (DOMHTMLTableCellElement *)_tableCellElementWithImpl:(HTMLTableCellElementImpl *)impl
{
    return static_cast<DOMHTMLTableCellElement *>([DOMNode _nodeWithImpl:impl]);
}

- (HTMLTableCellElementImpl *)_tableCellElementImpl
{
    return static_cast<HTMLTableCellElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

@end

@implementation DOMHTMLFrameSetElement

- (HTMLFrameSetElementImpl *)_frameSetElementImpl
{
    return static_cast<HTMLFrameSetElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)rows
{
    return [self _frameSetElementImpl]->getAttribute(ATTR_ROWS);
}

- (void)setRows:(NSString *)rows
{
    [self _frameSetElementImpl]->setAttribute(ATTR_ROWS, rows);
}

- (NSString *)cols
{
    return [self _frameSetElementImpl]->getAttribute(ATTR_COLS);
}

- (void)setCols:(NSString *)cols
{
    [self _frameSetElementImpl]->setAttribute(ATTR_COLS, cols);
}

@end

@implementation DOMHTMLFrameElement

- (HTMLFrameElementImpl *)_frameElementImpl
{
    return static_cast<HTMLFrameElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)frameBorder
{
    return [self _frameElementImpl]->getAttribute(ATTR_FRAMEBORDER);
}

- (void)setFrameBorder:(NSString *)frameBorder
{
    [self _frameElementImpl]->setAttribute(ATTR_FRAMEBORDER, frameBorder);
}

- (NSString *)longDesc
{
    return [self _frameElementImpl]->getAttribute(ATTR_LONGDESC);
}

- (void)setLongDesc:(NSString *)longDesc
{
    [self _frameElementImpl]->setAttribute(ATTR_LONGDESC, longDesc);
}

- (NSString *)marginHeight
{
    return [self _frameElementImpl]->getAttribute(ATTR_MARGINHEIGHT);
}

- (void)setMarginHeight:(NSString *)marginHeight
{
    [self _frameElementImpl]->setAttribute(ATTR_MARGINHEIGHT, marginHeight);
}

- (NSString *)marginWidth
{
    return [self _frameElementImpl]->getAttribute(ATTR_MARGINWIDTH);
}

- (void)setMarginWidth:(NSString *)marginWidth
{
    [self _frameElementImpl]->setAttribute(ATTR_MARGINWIDTH, marginWidth);
}

- (NSString *)name
{
    return [self _frameElementImpl]->getAttribute(ATTR_NAME);
}

- (void)setName:(NSString *)name
{
    [self _frameElementImpl]->setAttribute(ATTR_NAME, name);
}

- (BOOL)noResize
{
    return [self _frameElementImpl]->getAttribute(ATTR_NORESIZE).isNull();
}

- (void)setNoResize:(BOOL)noResize
{
    [self _frameElementImpl]->setAttribute(ATTR_NORESIZE, noResize ? "" : 0);
}

- (NSString *)scrolling
{
    return [self _frameElementImpl]->getAttribute(ATTR_SCROLLING);
}

- (void)setScrolling:(NSString *)scrolling
{
    [self _frameElementImpl]->setAttribute(ATTR_SCROLLING, scrolling);
}

- (NSString *)src
{
    return [self _frameElementImpl]->getAttribute(ATTR_SRC);
}

- (void)setSrc:(NSString *)src
{
    [self _frameElementImpl]->setAttribute(ATTR_SRC, src);
}

- (DOMDocument *)contentDocument
{
    return [DOMDocument _documentWithImpl:[self _frameElementImpl]->contentDocument()];
}

@end

@implementation DOMHTMLIFrameElement

- (HTMLIFrameElementImpl *)_IFrameElementImpl
{
    return static_cast<HTMLIFrameElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _IFrameElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _IFrameElementImpl]->setAttribute(ATTR_ALIGN, align);
}

- (NSString *)frameBorder
{
    return [self _IFrameElementImpl]->getAttribute(ATTR_FRAMEBORDER);
}

- (void)setFrameBorder:(NSString *)frameBorder
{
    [self _IFrameElementImpl]->setAttribute(ATTR_FRAMEBORDER, frameBorder);
}

- (NSString *)height
{
    return [self _IFrameElementImpl]->getAttribute(ATTR_HEIGHT);
}

- (void)setHeight:(NSString *)height
{
    [self _IFrameElementImpl]->setAttribute(ATTR_HEIGHT, height);
}

- (NSString *)longDesc
{
    return [self _IFrameElementImpl]->getAttribute(ATTR_LONGDESC);
}

- (void)setLongDesc:(NSString *)longDesc
{
    [self _IFrameElementImpl]->setAttribute(ATTR_LONGDESC, longDesc);
}

- (NSString *)marginHeight
{
    return [self _IFrameElementImpl]->getAttribute(ATTR_MARGINHEIGHT);
}

- (void)setMarginHeight:(NSString *)marginHeight
{
    [self _IFrameElementImpl]->setAttribute(ATTR_MARGINHEIGHT, marginHeight);
}

- (NSString *)marginWidth
{
    return [self _IFrameElementImpl]->getAttribute(ATTR_MARGINWIDTH);
}

- (void)setMarginWidth:(NSString *)marginWidth
{
    [self _IFrameElementImpl]->setAttribute(ATTR_MARGINWIDTH, marginWidth);
}

- (NSString *)name
{
    return [self _IFrameElementImpl]->getAttribute(ATTR_NAME);
}

- (void)setName:(NSString *)name
{
    [self _IFrameElementImpl]->setAttribute(ATTR_NAME, name);
}

- (BOOL)noResize
{
    return [self _IFrameElementImpl]->getAttribute(ATTR_NORESIZE).isNull();
}

- (void)setNoResize:(BOOL)noResize
{
    [self _IFrameElementImpl]->setAttribute(ATTR_NORESIZE, noResize ? "" : 0);
}

- (NSString *)scrolling
{
    return [self _IFrameElementImpl]->getAttribute(ATTR_SCROLLING);
}

- (void)setScrolling:(NSString *)scrolling
{
    [self _IFrameElementImpl]->setAttribute(ATTR_SCROLLING, scrolling);
}

- (NSString *)src
{
    return [self _IFrameElementImpl]->getAttribute(ATTR_SRC);
}

- (void)setSrc:(NSString *)src
{
    [self _IFrameElementImpl]->setAttribute(ATTR_SRC, src);
}

- (NSString *)width
{
    return [self _IFrameElementImpl]->getAttribute(ATTR_WIDTH);
}

- (void)setWidth:(NSString *)width
{
    [self _IFrameElementImpl]->setAttribute(ATTR_WIDTH, width);
}

- (DOMDocument *)contentDocument
{
    return [DOMDocument _documentWithImpl:[self _IFrameElementImpl]->contentDocument()];
}

@end

#pragma mark DOM EXTENSIONS

@implementation DOMHTMLEmbedElement

- (HTMLEmbedElementImpl *)_embedElementImpl
{
    return static_cast<HTMLEmbedElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _embedElementImpl]->getAttribute(ATTR_ALIGN);
}

- (void)setAlign:(NSString *)align
{
    [self _embedElementImpl]->setAttribute(ATTR_ALIGN, align);
}

- (long)height
{
    return [self _embedElementImpl]->getAttribute(ATTR_HEIGHT).toInt();
}

- (void)setHeight:(long)height
{
    DOMString string(QString::number(height));
    [self _embedElementImpl]->setAttribute(ATTR_HEIGHT, string);
}

- (NSString *)name
{
    return [self _embedElementImpl]->getAttribute(ATTR_NAME);
}

- (void)setName:(NSString *)name
{
    [self _embedElementImpl]->setAttribute(ATTR_NAME, name);
}

- (NSString *)src
{
    return [self _embedElementImpl]->getAttribute(ATTR_SRC);
}

- (void)setSrc:(NSString *)src
{
    [self _embedElementImpl]->setAttribute(ATTR_SRC, src);
}

- (NSString *)type
{
    return [self _embedElementImpl]->getAttribute(ATTR_TYPE);
}

- (void)setType:(NSString *)type
{
    [self _embedElementImpl]->setAttribute(ATTR_TYPE, type);
}

- (long)width
{
    return [self _embedElementImpl]->getAttribute(ATTR_WIDTH).toInt();
}

- (void)setWidth:(long)width
{
    DOMString string(QString::number(width));
    [self _embedElementImpl]->setAttribute(ATTR_WIDTH, string);
}

@end
