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

#include "config.h"
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
#import "dom_elementimpl.h"
#import "dom_nodeimpl.h"
#import "markup.h"

#import "DOMExtensions.h"
#import "DOMInternal.h"
#import "DOMHTMLInternal.h"
#import <kxmlcore/Assertions.h>
#import "KWQFoundationExtras.h"

using namespace DOM::HTMLNames;

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

// FIXME: This code should be using the impl methods instead of doing so many get/setAttribute calls.
// FIXME: This code should be generated.

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

- (unsigned)length
{
    return [self _collectionImpl]->length();
}

- (DOMNode *)item:(unsigned)index
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

- (unsigned)length
{
    return [self _optionsCollectionImpl]->length();
}

- (void)setLength:(unsigned)length
{
    [self _optionsCollectionImpl]->setLength(length);
}

- (DOMNode *)item:(unsigned)index
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
    return [self _HTMLElementImpl]->getAttribute(idAttr);
}

- (void)setIdName:(NSString *)idName
{
    [self _HTMLElementImpl]->setAttribute(idAttr, idName);
}

- (NSString *)title
{
    return [self _HTMLElementImpl]->title();
}

- (void)setTitle:(NSString *)title
{
    [self _HTMLElementImpl]->setTitle(title);
}

- (NSString *)lang
{
    return [self _HTMLElementImpl]->lang();
}

- (void)setLang:(NSString *)lang
{
    [self _HTMLElementImpl]->setLang(lang);
}

- (NSString *)dir
{
    return [self _HTMLElementImpl]->dir();
}

- (void)setDir:(NSString *)dir
{
    [self _HTMLElementImpl]->setDir(dir);
}

- (NSString *)className
{
    return [self _HTMLElementImpl]->className();
}

- (void)setClassName:(NSString *)className
{
    [self _HTMLElementImpl]->setClassName(className);
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
    return [self _HTMLHtmlElementImpl]->version();
}

- (void)setVersion:(NSString *)version
{
    [self _HTMLHtmlElementImpl]->setVersion(version);
}

@end

@implementation DOMHTMLHeadElement

- (HTMLHeadElementImpl *)_headElementImpl
{
    return static_cast<HTMLHeadElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)profile
{
    return [self _headElementImpl]->profile();
}

- (void)setProfile:(NSString *)profile
{
    [self _headElementImpl]->setProfile(profile);
}

@end

@implementation DOMHTMLLinkElement

- (HTMLLinkElementImpl *)_linkElementImpl
{
    return static_cast<HTMLLinkElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)disabled
{
    return ![self _linkElementImpl]->disabled();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _linkElementImpl]->setDisabled(disabled);
}

- (NSString *)charset
{
    return [self _linkElementImpl]->charset();
}

- (void)setCharset:(NSString *)charset
{
    [self _linkElementImpl]->setCharset(charset);
}

- (NSString *)href
{
    return [self _linkElementImpl]->href();
}

- (void)setHref:(NSString *)href
{
    [self _linkElementImpl]->href();
}

- (NSString *)hreflang
{
    return [self _linkElementImpl]->hreflang();
}

- (void)setHreflang:(NSString *)hreflang
{
    [self _linkElementImpl]->setHreflang(hreflang);
}

- (NSString *)media
{
    return [self _linkElementImpl]->getAttribute(mediaAttr);
}

- (void)setMedia:(NSString *)media
{
    [self _linkElementImpl]->setAttribute(mediaAttr, media);
}

- (NSString *)rel
{
    return [self _linkElementImpl]->getAttribute(relAttr);
}

- (void)setRel:(NSString *)rel
{
    [self _linkElementImpl]->setAttribute(relAttr, rel);
}

- (NSString *)rev
{
    return [self _linkElementImpl]->getAttribute(revAttr);
}

- (void)setRev:(NSString *)rev
{
    [self _linkElementImpl]->setAttribute(revAttr, rev);
}

- (NSString *)target
{
    return [self _linkElementImpl]->getAttribute(targetAttr);
}

- (void)setTarget:(NSString *)target
{
    [self _linkElementImpl]->setAttribute(targetAttr, target);
}

- (NSString *)type
{
    return [self _linkElementImpl]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _linkElementImpl]->setAttribute(typeAttr, type);
}

@end

@implementation DOMHTMLTitleElement

- (HTMLTitleElementImpl *)_titleElementImpl
{
    return static_cast<HTMLTitleElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)text
{
    return [self _titleElementImpl]->getAttribute(textAttr);
}

- (void)setText:(NSString *)text
{
    [self _titleElementImpl]->setAttribute(textAttr, text);
}

@end

@implementation DOMHTMLMetaElement

- (HTMLMetaElementImpl *)_metaElementImpl
{
    return static_cast<HTMLMetaElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)content
{
    return [self _metaElementImpl]->getAttribute(contentAttr);
}

- (void)setContent:(NSString *)content
{
    [self _metaElementImpl]->setAttribute(contentAttr, content);
}

- (NSString *)httpEquiv
{
    return [self _metaElementImpl]->getAttribute(http_equivAttr);
}

- (void)setHttpEquiv:(NSString *)httpEquiv
{
    [self _metaElementImpl]->setAttribute(http_equivAttr, httpEquiv);
}

- (NSString *)name
{
    return [self _metaElementImpl]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _metaElementImpl]->setAttribute(nameAttr, name);
}

- (NSString *)scheme
{
    return [self _metaElementImpl]->getAttribute(schemeAttr);
}

- (void)setScheme:(NSString *)scheme
{
    [self _metaElementImpl]->setAttribute(schemeAttr, scheme);
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
    [self _baseElementImpl]->setAttribute(hrefAttr, href);
}

- (NSString *)target
{
    return [self _baseElementImpl]->getAttribute(targetAttr);
}

- (void)setTarget:(NSString *)target
{
    [self _baseElementImpl]->setAttribute(schemeAttr, target);
}

@end

@implementation DOMHTMLStyleElement

- (HTMLStyleElementImpl *)_styleElementImpl
{
    return static_cast<HTMLStyleElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)disabled
{
    return ![self _styleElementImpl]->getAttribute(disabledAttr).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _styleElementImpl]->setAttribute(disabledAttr, disabled ? "" : 0);
}

- (NSString *)media
{
    return [self _styleElementImpl]->getAttribute(mediaAttr);
}

- (void)setMedia:(NSString *)media
{
    [self _styleElementImpl]->setAttribute(mediaAttr, media);
}

- (NSString *)type
{
    return [self _styleElementImpl]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _styleElementImpl]->setAttribute(typeAttr, type);
}

@end

@implementation DOMHTMLBodyElement

- (HTMLBodyElementImpl *)_bodyElementImpl
{
    return static_cast<HTMLBodyElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)aLink
{
    return [self _bodyElementImpl]->getAttribute(alinkAttr);
}

- (void)setALink:(NSString *)aLink
{
    [self _bodyElementImpl]->setAttribute(alinkAttr, aLink);
}

- (NSString *)background
{
    return [self _bodyElementImpl]->getAttribute(backgroundAttr);
}

- (void)setBackground:(NSString *)background
{
    [self _bodyElementImpl]->setAttribute(backgroundAttr, background);
}

- (NSString *)bgColor
{
    return [self _bodyElementImpl]->getAttribute(bgcolorAttr);
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _bodyElementImpl]->setAttribute(bgcolorAttr, bgColor);
}

- (NSString *)link
{
    return [self _bodyElementImpl]->getAttribute(linkAttr);
}

- (void)setLink:(NSString *)link
{
    [self _bodyElementImpl]->setAttribute(linkAttr, link);
}

- (NSString *)text
{
    return [self _bodyElementImpl]->getAttribute(textAttr);
}

- (void)setText:(NSString *)text
{
    [self _bodyElementImpl]->setAttribute(textAttr, text);
}

- (NSString *)vLink
{
    return [self _bodyElementImpl]->getAttribute(vlinkAttr);
}

- (void)setVLink:(NSString *)vLink
{
    [self _bodyElementImpl]->setAttribute(vlinkAttr, vLink);
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

- (int)length
{
    return [self _formElementImpl]->length();
}

- (NSString *)name
{
    return [self _formElementImpl]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _formElementImpl]->setAttribute(nameAttr, name);
}

- (NSString *)acceptCharset
{
    return [self _formElementImpl]->getAttribute(accept_charsetAttr);
}

- (void)setAcceptCharset:(NSString *)acceptCharset
{
    [self _formElementImpl]->setAttribute(accept_charsetAttr, acceptCharset);
}

- (NSString *)action
{
    return [self _formElementImpl]->getAttribute(actionAttr);
}

- (void)setAction:(NSString *)action
{
    [self _formElementImpl]->setAttribute(actionAttr, action);
}

- (NSString *)enctype
{
    return [self _formElementImpl]->getAttribute(enctypeAttr);
}

- (void)setEnctype:(NSString *)enctype
{
    [self _formElementImpl]->setAttribute(enctypeAttr, enctype);
}

- (NSString *)method
{
    return [self _formElementImpl]->getAttribute(methodAttr);
}

- (void)setMethod:(NSString *)method
{
    [self _formElementImpl]->setAttribute(methodAttr, method);
}

- (NSString *)target
{
    return [self _formElementImpl]->getAttribute(targetAttr);
}

- (void)setTarget:(NSString *)target
{
    [self _formElementImpl]->setAttribute(targetAttr, target);
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
    return [self _isIndexElementImpl]->prompt();
}

- (void)setPrompt:(NSString *)prompt
{
    [self _isIndexElementImpl]->setPrompt(prompt);
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

- (int)selectedIndex
{
    return [self _selectElementImpl]->selectedIndex();
}

- (void)setSelectedIndex:(int)selectedIndex
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
    [self _selectElementImpl]->setValue(s.impl());
}

- (int)length
{
    return [self _selectElementImpl]->length();
}

- (void)setLength:(int)length
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
    return ![self _selectElementImpl]->disabled();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _selectElementImpl]->setDisabled(disabled);
}

- (BOOL)multiple
{
    return ![self _selectElementImpl]->multiple();
}

- (void)setMultiple:(BOOL)multiple
{
    [self _selectElementImpl]->setMultiple(multiple);
}

- (NSString *)name
{
    return [self _selectElementImpl]->name();
}

- (void)setName:(NSString *)name
{
    [self _selectElementImpl]->setName(name);
}

- (int)size
{
    return [self _selectElementImpl]->size();
}

- (void)setSize:(int)size
{
    [self _selectElementImpl]->setSize(size);
}

- (int)tabIndex
{
    return [self _selectElementImpl]->tabIndex();
}

- (void)setTabIndex:(int)tabIndex
{
    [self _selectElementImpl]->setTabIndex(tabIndex);
}

- (void)add:(DOMHTMLElement *)element :(DOMHTMLElement *)before
{
    int exceptionCode = 0;
    [self _selectElementImpl]->add([element _HTMLElementImpl], [before _HTMLElementImpl], exceptionCode);
    raiseOnDOMError(exceptionCode);
}

- (void)remove:(int)index
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
    return ![self _optGroupElementImpl]->disabled();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _optGroupElementImpl]->setDisabled(disabled);
}

- (NSString *)label
{
    return [self _optGroupElementImpl]->label();
}

- (void)setLabel:(NSString *)label
{
    [self _optGroupElementImpl]->setLabel(label);
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
    return ![self _optionElementImpl]->defaultSelected();
}

- (void)setDefaultSelected:(BOOL)defaultSelected
{
    [self _optionElementImpl]->setDefaultSelected(defaultSelected);
}

- (NSString *)text
{
    return [self _optionElementImpl]->text();
}

- (int)index
{
    return [self _optionElementImpl]->index();
}

- (BOOL)disabled
{
    return ![self _optionElementImpl]->disabled();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _optionElementImpl]->setDisabled(disabled);
}

- (NSString *)label
{
    return [self _optionElementImpl]->label();
}

- (void)setLabel:(NSString *)label
{
    [self _optionElementImpl]->setLabel(label);
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
    [self _optionElementImpl]->setValue(string.impl());
}

@end

@implementation DOMHTMLInputElement

- (HTMLInputElementImpl *)_inputElementImpl
{
    return static_cast<HTMLInputElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)defaultValue
{
    return [self _inputElementImpl]->defaultValue();
}

- (void)setDefaultValue:(NSString *)defaultValue
{
    [self _inputElementImpl]->setDefaultValue(defaultValue);
}

- (BOOL)defaultChecked
{
    return [self _inputElementImpl]->defaultChecked();
}

- (void)setDefaultChecked:(BOOL)defaultChecked
{
    [self _inputElementImpl]->setDefaultChecked(defaultChecked);
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWithImpl:[self _inputElementImpl]->form()];
}

- (NSString *)accept
{
    return [self _inputElementImpl]->accept();
}

- (void)setAccept:(NSString *)accept
{
    [self _inputElementImpl]->setAccept(accept);
}

- (NSString *)accessKey
{
    return [self _inputElementImpl]->accessKey();
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _inputElementImpl]->setAccessKey(accessKey);
}

- (NSString *)align
{
    return [self _inputElementImpl]->align();
}

- (void)setAlign:(NSString *)align
{
    [self _inputElementImpl]->setAlign(align);
}

- (NSString *)alt
{
    return [self _inputElementImpl]->alt();
}

- (void)setAlt:(NSString *)alt
{
    [self _inputElementImpl]->setAlt(alt);
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
    return [self _inputElementImpl]->disabled();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _inputElementImpl]->setDisabled(disabled);
}

- (int)maxLength
{
    return [self _inputElementImpl]->maxLength();
}

- (void)setMaxLength:(int)maxLength
{
    [self _inputElementImpl]->setMaxLength(maxLength);
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
    return [self _inputElementImpl]->readOnly();
}

- (void)setReadOnly:(BOOL)readOnly
{
    [self _inputElementImpl]->setReadOnly(readOnly);
}

- (unsigned)size
{
    return [self _inputElementImpl]->size();
}

- (void)setSize:(unsigned)size
{
    [self _inputElementImpl]->setSize(size);
}

- (NSString *)src
{
    return [self _inputElementImpl]->src();
}

- (void)setSrc:(NSString *)src
{
    [self _inputElementImpl]->setSrc(src);
}

- (int)tabIndex
{
    return [self _inputElementImpl]->tabIndex();
}

- (void)setTabIndex:(int)tabIndex
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
    return [self _inputElementImpl]->useMap();
}

- (void)setUseMap:(NSString *)useMap
{
    [self _inputElementImpl]->setUseMap(useMap);
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
    return [self _textAreaElementImpl]->getAttribute(accesskeyAttr);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _textAreaElementImpl]->setAttribute(accesskeyAttr, accessKey);
}

- (int)cols
{
    return [self _textAreaElementImpl]->getAttribute(colsAttr).toInt();
}

- (void)setCols:(int)cols
{
    DOMString value(QString::number(cols));
    [self _textAreaElementImpl]->setAttribute(colsAttr, value);
}

- (BOOL)disabled
{
    return [self _textAreaElementImpl]->getAttribute(disabledAttr).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _textAreaElementImpl]->setAttribute(disabledAttr, disabled ? "" : 0);
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
    return [self _textAreaElementImpl]->getAttribute(readonlyAttr).isNull();
}

- (void)setReadOnly:(BOOL)readOnly
{
    [self _textAreaElementImpl]->setAttribute(readonlyAttr, readOnly ? "" : 0);
}

- (int)rows
{
    return [self _textAreaElementImpl]->getAttribute(rowsAttr).toInt();
}

- (void)setRows:(int)rows
{
	DOMString value(QString::number(rows));
    [self _textAreaElementImpl]->setAttribute(rowsAttr, value);
}

- (int)tabIndex
{
    return [self _textAreaElementImpl]->tabIndex();
}

- (void)setTabIndex:(int)tabIndex
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
    return [self _buttonElementImpl]->getAttribute(accesskeyAttr);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _buttonElementImpl]->setAttribute(accesskeyAttr, accessKey);
}

- (BOOL)disabled
{
    return [self _buttonElementImpl]->getAttribute(disabledAttr).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _buttonElementImpl]->setAttribute(disabledAttr, disabled ? "" : 0);
}

- (NSString *)name
{
    return [self _buttonElementImpl]->name();
}

- (void)setName:(NSString *)name
{
    [self _buttonElementImpl]->setName(name);
}

- (int)tabIndex
{
    return [self _buttonElementImpl]->tabIndex();
}

- (void)setTabIndex:(int)tabIndex
{
    [self _buttonElementImpl]->setTabIndex(tabIndex);
}

- (NSString *)type
{
    return [self _buttonElementImpl]->type();
}

- (NSString *)value
{
    return [self _buttonElementImpl]->getAttribute(valueAttr);
}

- (void)setValue:(NSString *)value
{
    [self _buttonElementImpl]->setAttribute(valueAttr, value);
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
    return [self _labelElementImpl]->getAttribute(accesskeyAttr);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _labelElementImpl]->setAttribute(accesskeyAttr, accessKey);
}

- (NSString *)htmlFor
{
    return [self _labelElementImpl]->getAttribute(forAttr);
}

- (void)setHtmlFor:(NSString *)htmlFor
{
    [self _labelElementImpl]->setAttribute(forAttr, htmlFor);
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
    return [self _legendElementImpl]->getAttribute(accesskeyAttr);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _legendElementImpl]->setAttribute(accesskeyAttr, accessKey);
}

- (NSString *)align
{
    return [self _legendElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _legendElementImpl]->setAttribute(alignAttr, align);
}

@end

@implementation DOMHTMLUListElement

- (HTMLUListElementImpl *)_uListElementImpl
{
    return static_cast<HTMLUListElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)compact
{
    return [self _uListElementImpl]->getAttribute(compactAttr).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _uListElementImpl]->setAttribute(compactAttr, compact ? "" : 0);
}

- (NSString *)type
{
    return [self _uListElementImpl]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _uListElementImpl]->setAttribute(typeAttr, type);
}

@end

@implementation DOMHTMLOListElement

- (HTMLOListElementImpl *)_oListElementImpl
{
    return static_cast<HTMLOListElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)compact
{
    return [self _oListElementImpl]->compact();
}

- (void)setCompact:(BOOL)compact
{
    [self _oListElementImpl]->setCompact(compact);
}

- (int)start
{
    return [self _oListElementImpl]->getAttribute(startAttr).toInt();
}

- (void)setStart:(int)start
{
	DOMString value(QString::number(start));
    [self _oListElementImpl]->setAttribute(startAttr, value);
}

- (NSString *)type
{
    return [self _oListElementImpl]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _oListElementImpl]->setAttribute(typeAttr, type);
}

@end

@implementation DOMHTMLDListElement

- (HTMLDListElementImpl *)_dListElementImpl
{
    return static_cast<HTMLDListElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)compact
{
    return [self _dListElementImpl]->getAttribute(compactAttr).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _dListElementImpl]->setAttribute(compactAttr, compact ? "" : 0);
}

@end

@implementation DOMHTMLDirectoryElement

- (HTMLDirectoryElementImpl *)_directoryListElementImpl
{
    return static_cast<HTMLDirectoryElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)compact
{
    return [self _directoryListElementImpl]->getAttribute(compactAttr).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _directoryListElementImpl]->setAttribute(compactAttr, compact ? "" : 0);
}

@end

@implementation DOMHTMLMenuElement

- (HTMLMenuElementImpl *)_menuListElementImpl
{
    return static_cast<HTMLMenuElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)compact
{
    return [self _menuListElementImpl]->getAttribute(compactAttr).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _menuListElementImpl]->setAttribute(compactAttr, compact ? "" : 0);
}

@end

@implementation DOMHTMLLIElement

- (HTMLLIElementImpl *)_liElementImpl
{
    return static_cast<HTMLLIElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)type
{
    return [self _liElementImpl]->type();
}

- (void)setType:(NSString *)type
{
    [self _liElementImpl]->setType(type);
}

- (int)value
{
    return [self _liElementImpl]->value();
}

- (void)setValue:(int)value
{
    [self _liElementImpl]->setValue(value);
}

@end

@implementation DOMHTMLQuoteElement

- (HTMLElementImpl *)_quoteElementImpl
{
    return static_cast<HTMLElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)cite
{
    return [self _quoteElementImpl]->getAttribute(citeAttr);
}

- (void)setCite:(NSString *)cite
{
    [self _quoteElementImpl]->setAttribute(citeAttr, cite);
}

@end

@implementation DOMHTMLDivElement

- (HTMLDivElementImpl *)_divElementImpl
{
    return static_cast<HTMLDivElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _divElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _divElementImpl]->setAttribute(alignAttr, align);
}

@end

@implementation DOMHTMLParagraphElement

- (HTMLParagraphElementImpl *)_paragraphElementImpl
{
    return static_cast<HTMLParagraphElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _paragraphElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _paragraphElementImpl]->setAttribute(alignAttr, align);
}

@end

@implementation DOMHTMLHeadingElement

- (HTMLHeadingElementImpl *)_headingElementImpl
{
    return static_cast<HTMLHeadingElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _headingElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _headingElementImpl]->setAttribute(alignAttr, align);
}

@end

@implementation DOMHTMLPreElement

- (HTMLPreElementImpl *)_preElementImpl
{
    return static_cast<HTMLPreElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (int)width
{
    return [self _preElementImpl]->getAttribute(widthAttr).toInt();
}

- (void)setWidth:(int)width
{
    DOMString string(QString::number(width));
    [self _preElementImpl]->setAttribute(widthAttr, string);
}

@end

@implementation DOMHTMLBRElement

- (HTMLBRElementImpl *)_BRElementImpl
{
    return static_cast<HTMLBRElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)clear
{
    return [self _BRElementImpl]->getAttribute(clearAttr);
}

- (void)setClear:(NSString *)clear
{
    [self _BRElementImpl]->setAttribute(clearAttr, clear);
}

@end

@implementation DOMHTMLBaseFontElement

- (HTMLBaseFontElementImpl *)_baseFontElementImpl
{
    return static_cast<HTMLBaseFontElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)color
{
    return [self _baseFontElementImpl]->getAttribute(colorAttr);
}

- (void)setColor:(NSString *)color
{
    [self _baseFontElementImpl]->setAttribute(colorAttr, color);
}

- (NSString *)face
{
    return [self _baseFontElementImpl]->getAttribute(faceAttr);
}

- (void)setFace:(NSString *)face
{
    [self _baseFontElementImpl]->setAttribute(faceAttr, face);
}

- (NSString *)size
{
    return [self _baseFontElementImpl]->getAttribute(sizeAttr);
}

- (void)setSize:(NSString *)size
{
    [self _baseFontElementImpl]->setAttribute(sizeAttr, size);
}

@end

@implementation DOMHTMLFontElement

- (HTMLFontElementImpl *)_fontElementImpl
{
    return static_cast<HTMLFontElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)color
{
    return [self _fontElementImpl]->getAttribute(colorAttr);
}

- (void)setColor:(NSString *)color
{
    [self _fontElementImpl]->setAttribute(colorAttr, color);
}

- (NSString *)face
{
    return [self _fontElementImpl]->getAttribute(faceAttr);
}

- (void)setFace:(NSString *)face
{
    [self _fontElementImpl]->setAttribute(faceAttr, face);
}

- (NSString *)size
{
    return [self _fontElementImpl]->getAttribute(sizeAttr);
}

- (void)setSize:(NSString *)size
{
    [self _fontElementImpl]->setAttribute(sizeAttr, size);
}

@end

@implementation DOMHTMLHRElement

- (HTMLHRElementImpl *)_HRElementImpl
{
    return static_cast<HTMLHRElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _HRElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _HRElementImpl]->setAttribute(alignAttr, align);
}

- (BOOL)noShade
{
    return [self _HRElementImpl]->getAttribute(noshadeAttr).isNull();
}

- (void)setNoShade:(BOOL)noShade
{
    [self _HRElementImpl]->setAttribute(noshadeAttr, noShade ? "" : 0);
}

- (NSString *)size
{
    return [self _HRElementImpl]->getAttribute(sizeAttr);
}

- (void)setSize:(NSString *)size
{
    [self _HRElementImpl]->setAttribute(sizeAttr, size);
}

- (NSString *)width
{
    return [self _HRElementImpl]->getAttribute(widthAttr);
}

- (void)setWidth:(NSString *)width
{
    [self _HRElementImpl]->setAttribute(widthAttr, width);
}

@end

@implementation DOMHTMLModElement

- (HTMLElementImpl *)_modElementImpl
{
    return static_cast<HTMLElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)cite
{
    return [self _modElementImpl]->getAttribute(citeAttr);
}

- (void)setCite:(NSString *)cite
{
    [self _modElementImpl]->setAttribute(citeAttr, cite);
}

- (NSString *)dateTime
{
    return [self _modElementImpl]->getAttribute(datetimeAttr);
}

- (void)setDateTime:(NSString *)dateTime
{
    [self _modElementImpl]->setAttribute(datetimeAttr, dateTime);
}

@end

@implementation DOMHTMLAnchorElement

- (HTMLAnchorElementImpl *)_anchorElementImpl
{
    return static_cast<HTMLAnchorElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)accessKey
{
    return [self _anchorElementImpl]->getAttribute(accesskeyAttr);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _anchorElementImpl]->setAttribute(accesskeyAttr, accessKey);
}

- (NSString *)charset
{
    return [self _anchorElementImpl]->getAttribute(charsetAttr);
}

- (void)setCharset:(NSString *)charset
{
    [self _anchorElementImpl]->setAttribute(charsetAttr, charset);
}

- (NSString *)coords
{
    return [self _anchorElementImpl]->getAttribute(coordsAttr);
}

- (void)setCoords:(NSString *)coords
{
    [self _anchorElementImpl]->setAttribute(coordsAttr, coords);
}

- (NSString *)href
{
    return [self _anchorElementImpl]->href();
}

- (void)setHref:(NSString *)href
{
    [self _anchorElementImpl]->setAttribute(hrefAttr, href);
}

- (NSString *)hreflang
{
    return [self _anchorElementImpl]->hreflang();
}

- (void)setHreflang:(NSString *)hreflang
{
    [self _anchorElementImpl]->setHreflang(hreflang);
}

- (NSString *)name
{
    return [self _anchorElementImpl]->name();
}

- (void)setName:(NSString *)name
{
    [self _anchorElementImpl]->setName(name);
}

- (NSString *)rel
{
    return [self _anchorElementImpl]->rel();
}

- (void)setRel:(NSString *)rel
{
    [self _anchorElementImpl]->setRel(rel);
}

- (NSString *)rev
{
    return [self _anchorElementImpl]->rev();
}

- (void)setRev:(NSString *)rev
{
    [self _anchorElementImpl]->setRev(rev);
}

- (NSString *)shape
{
    return [self _anchorElementImpl]->shape();
}

- (void)setShape:(NSString *)shape
{
    [self _anchorElementImpl]->setShape(shape);
}

- (int)tabIndex
{
    return [self _anchorElementImpl]->tabIndex();
}

- (void)setTabIndex:(int)tabIndex
{
    [self _anchorElementImpl]->setTabIndex(tabIndex);
}

- (NSString *)target
{
    return [self _anchorElementImpl]->getAttribute(targetAttr);
}

- (void)setTarget:(NSString *)target
{
    [self _anchorElementImpl]->setAttribute(targetAttr, target);
}

- (NSString *)type
{
    return [self _anchorElementImpl]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _anchorElementImpl]->setAttribute(typeAttr, type);
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
    return [self _imageElementImpl]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _imageElementImpl]->setAttribute(nameAttr, name);
}

- (NSString *)align
{
    return [self _imageElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _imageElementImpl]->setAttribute(alignAttr, align);
}

- (NSString *)alt
{
    return [self _imageElementImpl]->getAttribute(altAttr);
}

- (void)setAlt:(NSString *)alt
{
    [self _imageElementImpl]->setAttribute(altAttr, alt);
}

- (NSString *)border
{
    return [self _imageElementImpl]->getAttribute(borderAttr);
}

- (void)setBorder:(NSString *)border
{
    [self _imageElementImpl]->setAttribute(borderAttr, border);
}

- (int)height
{
    return [self _imageElementImpl]->getAttribute(heightAttr).toInt();
}

- (void)setHeight:(int)height
{
    DOMString string(QString::number(height));
    [self _imageElementImpl]->setAttribute(heightAttr, string);
}

- (int)hspace
{
    return [self _imageElementImpl]->getAttribute(hspaceAttr).toInt();
}

- (void)setHspace:(int)hspace
{
    DOMString string(QString::number(hspace));
    [self _imageElementImpl]->setAttribute(hspaceAttr, string);
}

- (BOOL)isMap
{
    return [self _imageElementImpl]->getAttribute(ismapAttr).isNull();
}

- (void)setIsMap:(BOOL)isMap
{
    [self _imageElementImpl]->setAttribute(ismapAttr, isMap ? "" : 0);
}

- (NSString *)longDesc
{
    return [self _imageElementImpl]->getAttribute(longdescAttr);
}

- (void)setLongDesc:(NSString *)longDesc
{
    [self _imageElementImpl]->setAttribute(longdescAttr, longDesc);
}

- (NSString *)src
{
    return [self _imageElementImpl]->src();
}

- (void)setSrc:(NSString *)src
{
    [self _imageElementImpl]->setAttribute(srcAttr, src);
}

- (NSString *)useMap
{
    return [self _imageElementImpl]->getAttribute(usemapAttr);
}

- (void)setUseMap:(NSString *)useMap
{
    [self _imageElementImpl]->setAttribute(usemapAttr, useMap);
}

- (int)vspace
{
    return [self _imageElementImpl]->getAttribute(vspaceAttr).toInt();
}

- (void)setVspace:(int)vspace
{
    DOMString string(QString::number(vspace));
    [self _imageElementImpl]->setAttribute(vspaceAttr, string);
}

- (int)width
{
    return [self _imageElementImpl]->getAttribute(widthAttr).toInt();
}

- (void)setWidth:(int)width
{
    DOMString string(QString::number(width));
    [self _imageElementImpl]->setAttribute(widthAttr, string);
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
    return [self _objectElementImpl]->getAttribute(codeAttr);
}

- (void)setCode:(NSString *)code
{
    [self _objectElementImpl]->setAttribute(codeAttr, code);
}

- (NSString *)align
{
    return [self _objectElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _objectElementImpl]->setAttribute(alignAttr, align);
}

- (NSString *)archive
{
    return [self _objectElementImpl]->getAttribute(archiveAttr);
}

- (void)setArchive:(NSString *)archive
{
    [self _objectElementImpl]->setAttribute(archiveAttr, archive);
}

- (NSString *)border
{
    return [self _objectElementImpl]->getAttribute(borderAttr);
}

- (void)setBorder:(NSString *)border
{
    [self _objectElementImpl]->setAttribute(borderAttr, border);
}

- (NSString *)codeBase
{
    return [self _objectElementImpl]->getAttribute(codebaseAttr);
}

- (void)setCodeBase:(NSString *)codeBase
{
    [self _objectElementImpl]->setAttribute(codebaseAttr, codeBase);
}

- (NSString *)codeType
{
    return [self _objectElementImpl]->getAttribute(codetypeAttr);
}

- (void)setCodeType:(NSString *)codeType
{
    [self _objectElementImpl]->setAttribute(codetypeAttr, codeType);
}

- (NSString *)data
{
    return [self _objectElementImpl]->getAttribute(dataAttr);
}

- (void)setData:(NSString *)data
{
    [self _objectElementImpl]->setAttribute(dataAttr, data);
}

- (BOOL)declare
{
    return [self _objectElementImpl]->getAttribute(declareAttr).isNull();
}

- (void)setDeclare:(BOOL)declare
{
    [self _objectElementImpl]->setAttribute(declareAttr, declare ? "" : 0);
}

- (NSString *)height
{
    return [self _objectElementImpl]->getAttribute(heightAttr);
}

- (void)setHeight:(NSString *)height
{
    [self _objectElementImpl]->setAttribute(heightAttr, height);
}

- (int)hspace
{
    return [self _objectElementImpl]->getAttribute(hspaceAttr).toInt();
}

- (void)setHspace:(int)hspace
{
    DOMString string(QString::number(hspace));
    [self _objectElementImpl]->setAttribute(hspaceAttr, string);
}

- (NSString *)name
{
    return [self _objectElementImpl]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _objectElementImpl]->setAttribute(nameAttr, name);
}

- (NSString *)standby
{
    return [self _objectElementImpl]->getAttribute(standbyAttr);
}

- (void)setStandby:(NSString *)standby
{
    [self _objectElementImpl]->setAttribute(standbyAttr, standby);
}

- (int)tabIndex
{
    return [self _objectElementImpl]->getAttribute(tabindexAttr).toInt();
}

- (void)setTabIndex:(int)tabIndex
{
    DOMString string(QString::number(tabIndex));
    [self _objectElementImpl]->setAttribute(tabindexAttr, string);
}

- (NSString *)type
{
    return [self _objectElementImpl]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _objectElementImpl]->setAttribute(typeAttr, type);
}

- (NSString *)useMap
{
    return [self _objectElementImpl]->getAttribute(usemapAttr);
}

- (void)setUseMap:(NSString *)useMap
{
    [self _objectElementImpl]->setAttribute(usemapAttr, useMap);
}

- (int)vspace
{
    return [self _objectElementImpl]->getAttribute(vspaceAttr).toInt();
}

- (void)setVspace:(int)vspace
{
    DOMString string(QString::number(vspace));
    [self _objectElementImpl]->setAttribute(vspaceAttr, string);
}

- (NSString *)width
{
    return [self _objectElementImpl]->getAttribute(widthAttr);
}

- (void)setWidth:(NSString *)width
{
    [self _objectElementImpl]->setAttribute(widthAttr, width);
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
    return [self _paramElementImpl]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _paramElementImpl]->setAttribute(nameAttr, name);
}

- (NSString *)type
{
    return [self _paramElementImpl]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _paramElementImpl]->setAttribute(typeAttr, type);
}

- (NSString *)value
{
    return [self _paramElementImpl]->getAttribute(valueAttr);
}

- (void)setValue:(NSString *)value
{
    [self _paramElementImpl]->setAttribute(valueAttr, value);
}

- (NSString *)valueType
{
    return [self _paramElementImpl]->getAttribute(valuetypeAttr);
}

- (void)setValueType:(NSString *)valueType
{
    [self _paramElementImpl]->setAttribute(valuetypeAttr, valueType);
}

@end

@implementation DOMHTMLAppletElement

- (HTMLAppletElementImpl *)_appletElementImpl
{
    return static_cast<HTMLAppletElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _appletElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _appletElementImpl]->setAttribute(alignAttr, align);
}

- (NSString *)alt
{
    return [self _appletElementImpl]->getAttribute(altAttr);
}

- (void)setAlt:(NSString *)alt
{
    [self _appletElementImpl]->setAttribute(altAttr, alt);
}

- (NSString *)archive
{
    return [self _appletElementImpl]->getAttribute(archiveAttr);
}

- (void)setArchive:(NSString *)archive
{
    [self _appletElementImpl]->setAttribute(archiveAttr, archive);
}

- (NSString *)code
{
    return [self _appletElementImpl]->getAttribute(codeAttr);
}

- (void)setCode:(NSString *)code
{
    [self _appletElementImpl]->setAttribute(codeAttr, code);
}

- (NSString *)codeBase
{
    return [self _appletElementImpl]->getAttribute(codebaseAttr);
}

- (void)setCodeBase:(NSString *)codeBase
{
    [self _appletElementImpl]->setAttribute(codebaseAttr, codeBase);
}

- (NSString *)height
{
    return [self _appletElementImpl]->getAttribute(heightAttr);
}

- (void)setHeight:(NSString *)height
{
    [self _appletElementImpl]->setAttribute(heightAttr, height);
}

- (int)hspace
{
    return [self _appletElementImpl]->getAttribute(hspaceAttr).toInt();
}

- (void)setHspace:(int)hspace
{
    DOMString string(QString::number(hspace));
    [self _appletElementImpl]->setAttribute(hspaceAttr, string);
}

- (NSString *)name
{
    return [self _appletElementImpl]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _appletElementImpl]->setAttribute(nameAttr, name);
}

- (NSString *)object
{
    return [self _appletElementImpl]->getAttribute(objectAttr);
}

- (void)setObject:(NSString *)object
{
    [self _appletElementImpl]->setAttribute(objectAttr, object);
}

- (int)vspace
{
    return [self _appletElementImpl]->getAttribute(vspaceAttr).toInt();
}

- (void)setVspace:(int)vspace
{
    DOMString string(QString::number(vspace));
    [self _appletElementImpl]->setAttribute(vspaceAttr, string);
}

- (NSString *)width
{
    return [self _appletElementImpl]->getAttribute(widthAttr);
}

- (void)setWidth:(NSString *)width
{
    [self _appletElementImpl]->setAttribute(widthAttr, width);
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
    return [self _mapElementImpl]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _mapElementImpl]->setAttribute(nameAttr, name);
}

@end

@implementation DOMHTMLAreaElement

- (HTMLAreaElementImpl *)_areaElementImpl
{
    return static_cast<HTMLAreaElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)accessKey
{
    return [self _areaElementImpl]->getAttribute(accesskeyAttr);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _areaElementImpl]->setAttribute(accesskeyAttr, accessKey);
}

- (NSString *)alt
{
    return [self _areaElementImpl]->getAttribute(altAttr);
}

- (void)setAlt:(NSString *)alt
{
    [self _areaElementImpl]->setAttribute(altAttr, alt);
}

- (NSString *)coords
{
    return [self _areaElementImpl]->getAttribute(coordsAttr);
}

- (void)setCoords:(NSString *)coords
{
    [self _areaElementImpl]->setAttribute(coordsAttr, coords);
}

- (NSString *)href
{
    return [self _areaElementImpl]->href();
}

- (void)setHref:(NSString *)href
{
    [self _areaElementImpl]->setAttribute(hrefAttr, href);
}

- (BOOL)noHref
{
    return [self _areaElementImpl]->getAttribute(nohrefAttr).isNull();
}

- (void)setNoHref:(BOOL)noHref
{
    [self _areaElementImpl]->setAttribute(nohrefAttr, noHref ? "" : 0);
}

- (NSString *)shape
{
    return [self _areaElementImpl]->getAttribute(shapeAttr);
}

- (void)setShape:(NSString *)shape
{
    [self _areaElementImpl]->setAttribute(shapeAttr, shape);
}

- (int)tabIndex
{
    return [self _areaElementImpl]->getAttribute(tabindexAttr).toInt();
}

- (void)setTabIndex:(int)tabIndex
{
    DOMString string(QString::number(tabIndex));
    [self _areaElementImpl]->setAttribute(tabindexAttr, string);
}

- (NSString *)target
{
    return [self _areaElementImpl]->getAttribute(targetAttr);
}

- (void)setTarget:(NSString *)target
{
    [self _areaElementImpl]->setAttribute(targetAttr, target);
}

@end

@implementation DOMHTMLScriptElement

- (HTMLScriptElementImpl *)_scriptElementImpl
{
    return static_cast<HTMLScriptElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)text
{
    return [self _scriptElementImpl]->getAttribute(textAttr);
}

- (void)setText:(NSString *)text
{
    [self _scriptElementImpl]->setAttribute(textAttr, text);
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
    return [self _scriptElementImpl]->getAttribute(charsetAttr);
}

- (void)setCharset:(NSString *)charset
{
    [self _scriptElementImpl]->setAttribute(charsetAttr, charset);
}

- (BOOL)defer
{
    return [self _scriptElementImpl]->getAttribute(deferAttr).isNull();
}

- (void)setDefer:(BOOL)defer
{
    [self _scriptElementImpl]->setAttribute(deferAttr, defer ? "" : 0);
}

- (NSString *)src
{
    return [self _scriptElementImpl]->getAttribute(srcAttr);
}

- (void)setSrc:(NSString *)src
{
    [self _scriptElementImpl]->setAttribute(srcAttr, src);
}

- (NSString *)type
{
    return [self _scriptElementImpl]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _scriptElementImpl]->setAttribute(typeAttr, type);
}

@end

@implementation DOMHTMLTableCaptionElement

- (NSString *)align
{
    return [self _tableCaptionElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _tableCaptionElementImpl]->setAttribute(alignAttr, align);
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
    return [self _tableSectionElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _tableSectionElementImpl]->setAttribute(alignAttr, align);
}

- (NSString *)ch
{
    return [self _tableSectionElementImpl]->getAttribute(charoffAttr);
}

- (void)setCh:(NSString *)ch
{
    [self _tableSectionElementImpl]->setAttribute(charoffAttr, ch);
}

- (NSString *)chOff
{
    return [self _tableSectionElementImpl]->getAttribute(charoffAttr);
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableSectionElementImpl]->setAttribute(charoffAttr, chOff);
}

- (NSString *)vAlign
{
    return [self _tableSectionElementImpl]->getAttribute(valignAttr);
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableSectionElementImpl]->setAttribute(valignAttr, vAlign);
}

- (DOMHTMLCollection *)rows
{
    HTMLCollectionImpl *collection = new HTMLCollectionImpl([self _tableSectionElementImpl], HTMLCollectionImpl::TABLE_ROWS);
    return [DOMHTMLCollection _collectionWithImpl:collection];
}

- (DOMHTMLElement *)insertRow:(int)index
{
    int exceptioncode = 0;
    HTMLElementImpl *impl = [self _tableSectionElementImpl]->insertRow(index, exceptioncode);
    raiseOnDOMError(exceptioncode);
    return [DOMHTMLElement _elementWithImpl:impl];
}

- (void)deleteRow:(int)index
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
    return [self _tableElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _tableElementImpl]->setAttribute(alignAttr, align);
}

- (NSString *)bgColor
{
    return [self _tableElementImpl]->getAttribute(bgcolorAttr);
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _tableElementImpl]->setAttribute(bgcolorAttr, bgColor);
}

- (NSString *)border
{
    return [self _tableElementImpl]->getAttribute(borderAttr);
}

- (void)setBorder:(NSString *)border
{
    [self _tableElementImpl]->setAttribute(borderAttr, border);
}

- (NSString *)cellPadding
{
    return [self _tableElementImpl]->getAttribute(cellpaddingAttr);
}

- (void)setCellPadding:(NSString *)cellPadding
{
    [self _tableElementImpl]->setAttribute(cellpaddingAttr, cellPadding);
}

- (NSString *)cellSpacing
{
    return [self _tableElementImpl]->getAttribute(cellspacingAttr);
}

- (void)setCellSpacing:(NSString *)cellSpacing
{
    [self _tableElementImpl]->setAttribute(cellspacingAttr, cellSpacing);
}

- (NSString *)frameBorders
{
    return [self _tableElementImpl]->getAttribute(frameAttr);
}

- (void)setFrameBorders:(NSString *)frameBorders
{
    [self _tableElementImpl]->setAttribute(frameAttr, frameBorders);
}

- (NSString *)rules
{
    return [self _tableElementImpl]->getAttribute(rulesAttr);
}

- (void)setRules:(NSString *)rules
{
    [self _tableElementImpl]->setAttribute(rulesAttr, rules);
}

- (NSString *)summary
{
    return [self _tableElementImpl]->getAttribute(summaryAttr);
}

- (void)setSummary:(NSString *)summary
{
    [self _tableElementImpl]->setAttribute(summaryAttr, summary);
}

- (NSString *)width
{
    return [self _tableElementImpl]->getAttribute(widthAttr);
}

- (void)setWidth:(NSString *)width
{
    [self _tableElementImpl]->setAttribute(widthAttr, width);
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

- (DOMHTMLElement *)insertRow:(int)index
{
    int exceptioncode = 0;
    HTMLTableElementImpl *impl = static_cast<HTMLTableElementImpl *>([self _tableElementImpl]->insertRow(index, exceptioncode));
    raiseOnDOMError(exceptioncode);
    return [DOMHTMLTableElement _tableElementWithImpl:impl];
}

- (void)deleteRow:(int)index
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
    return [self _tableColElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _tableColElementImpl]->setAttribute(alignAttr, align);
}

- (NSString *)ch
{
    return [self _tableColElementImpl]->getAttribute(charoffAttr);
}

- (void)setCh:(NSString *)ch
{
    [self _tableColElementImpl]->setAttribute(charoffAttr, ch);
}

- (NSString *)chOff
{
    return [self _tableColElementImpl]->getAttribute(charoffAttr);
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableColElementImpl]->setAttribute(charoffAttr, chOff);
}

- (int)span
{
    return [self _tableColElementImpl]->getAttribute(spanAttr).toInt();
}

- (void)setSpan:(int)span
{
    DOMString string(QString::number(span));
    [self _tableColElementImpl]->setAttribute(spanAttr, string);
}

- (NSString *)vAlign
{
    return [self _tableColElementImpl]->getAttribute(valignAttr);
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableColElementImpl]->setAttribute(valignAttr, vAlign);
}

- (NSString *)width
{
    return [self _tableColElementImpl]->getAttribute(widthAttr);
}

- (void)setWidth:(NSString *)width
{
    [self _tableColElementImpl]->setAttribute(widthAttr, width);
}

@end

@implementation DOMHTMLTableRowElement

- (HTMLTableRowElementImpl *)_tableRowElementImpl
{
    return static_cast<HTMLTableRowElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (int)rowIndex
{
    return [self _tableRowElementImpl]->rowIndex();
}

- (int)sectionRowIndex
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
    return [self _tableRowElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _tableRowElementImpl]->setAttribute(alignAttr, align);
}

- (NSString *)bgColor
{
    return [self _tableRowElementImpl]->getAttribute(bgcolorAttr);
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _tableRowElementImpl]->setAttribute(bgcolorAttr, bgColor);
}

- (NSString *)ch
{
    return [self _tableRowElementImpl]->getAttribute(charoffAttr);
}

- (void)setCh:(NSString *)ch
{
    [self _tableRowElementImpl]->setAttribute(charoffAttr, ch);
}

- (NSString *)chOff
{
    return [self _tableRowElementImpl]->getAttribute(charoffAttr);
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableRowElementImpl]->setAttribute(charoffAttr, chOff);
}

- (NSString *)vAlign
{
    return [self _tableRowElementImpl]->getAttribute(valignAttr);
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableRowElementImpl]->setAttribute(valignAttr, vAlign);
}

- (DOMHTMLElement *)insertCell:(int)index
{
    int exceptioncode = 0;
    HTMLTableCellElementImpl *impl = static_cast<HTMLTableCellElementImpl *>([self _tableRowElementImpl]->insertCell(index, exceptioncode));
    raiseOnDOMError(exceptioncode);
    return [DOMHTMLTableCellElement _tableCellElementWithImpl:impl];
}

- (void)deleteCell:(int)index
{
    int exceptioncode = 0;
    [self _tableRowElementImpl]->deleteCell(index, exceptioncode);
    raiseOnDOMError(exceptioncode);
}

@end

@implementation DOMHTMLTableCellElement

- (int)cellIndex
{
    return [self _tableCellElementImpl]->cellIndex();
}

- (NSString *)abbr
{
    return [self _tableCellElementImpl]->getAttribute(abbrAttr);
}

- (void)setAbbr:(NSString *)abbr
{
    [self _tableCellElementImpl]->setAttribute(abbrAttr, abbr);
}

- (NSString *)align
{
    return [self _tableCellElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _tableCellElementImpl]->setAttribute(alignAttr, align);
}

- (NSString *)axis
{
    return [self _tableCellElementImpl]->getAttribute(axisAttr);
}

- (void)setAxis:(NSString *)axis
{
    [self _tableCellElementImpl]->setAttribute(axisAttr, axis);
}

- (NSString *)bgColor
{
    return [self _tableCellElementImpl]->getAttribute(bgcolorAttr);
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _tableCellElementImpl]->setAttribute(bgcolorAttr, bgColor);
}

- (NSString *)ch
{
    return [self _tableCellElementImpl]->getAttribute(charoffAttr);
}

- (void)setCh:(NSString *)ch
{
    [self _tableCellElementImpl]->setAttribute(charoffAttr, ch);
}

- (NSString *)chOff
{
    return [self _tableCellElementImpl]->getAttribute(charoffAttr);
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableCellElementImpl]->setAttribute(charoffAttr, chOff);
}

- (int)colSpan
{
    return [self _tableCellElementImpl]->getAttribute(colspanAttr).toInt();
}

- (void)setColSpan:(int)colSpan
{
    DOMString string(QString::number(colSpan));
    [self _tableCellElementImpl]->setAttribute(colspanAttr, string);
}

- (NSString *)headers
{
    return [self _tableCellElementImpl]->getAttribute(headersAttr);
}

- (void)setHeaders:(NSString *)headers
{
    [self _tableCellElementImpl]->setAttribute(headersAttr, headers);
}

- (NSString *)height
{
    return [self _tableCellElementImpl]->getAttribute(heightAttr);
}

- (void)setHeight:(NSString *)height
{
    [self _tableCellElementImpl]->setAttribute(heightAttr, height);
}

- (BOOL)noWrap
{
    return [self _tableCellElementImpl]->getAttribute(nowrapAttr).isNull();
}

- (void)setNoWrap:(BOOL)noWrap
{
    [self _tableCellElementImpl]->setAttribute(nowrapAttr, noWrap ? "" : 0);
}

- (int)rowSpan
{
    return [self _tableCellElementImpl]->getAttribute(rowspanAttr).toInt();
}

- (void)setRowSpan:(int)rowSpan
{
    DOMString string(QString::number(rowSpan));
    [self _tableCellElementImpl]->setAttribute(rowspanAttr, string);
}

- (NSString *)scope
{
    return [self _tableCellElementImpl]->getAttribute(scopeAttr);
}

- (void)setScope:(NSString *)scope
{
    [self _tableCellElementImpl]->setAttribute(scopeAttr, scope);
}

- (NSString *)vAlign
{
    return [self _tableCellElementImpl]->getAttribute(valignAttr);
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableCellElementImpl]->setAttribute(valignAttr, vAlign);
}

- (NSString *)width
{
    return [self _tableCellElementImpl]->getAttribute(widthAttr);
}

- (void)setWidth:(NSString *)width
{
    [self _tableCellElementImpl]->setAttribute(widthAttr, width);
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
    return [self _frameSetElementImpl]->getAttribute(rowsAttr);
}

- (void)setRows:(NSString *)rows
{
    [self _frameSetElementImpl]->setAttribute(rowsAttr, rows);
}

- (NSString *)cols
{
    return [self _frameSetElementImpl]->getAttribute(colsAttr);
}

- (void)setCols:(NSString *)cols
{
    [self _frameSetElementImpl]->setAttribute(colsAttr, cols);
}

@end

@implementation DOMHTMLFrameElement

- (HTMLFrameElementImpl *)_frameElementImpl
{
    return static_cast<HTMLFrameElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)frameBorder
{
    return [self _frameElementImpl]->getAttribute(frameborderAttr);
}

- (void)setFrameBorder:(NSString *)frameBorder
{
    [self _frameElementImpl]->setAttribute(frameborderAttr, frameBorder);
}

- (NSString *)longDesc
{
    return [self _frameElementImpl]->getAttribute(longdescAttr);
}

- (void)setLongDesc:(NSString *)longDesc
{
    [self _frameElementImpl]->setAttribute(longdescAttr, longDesc);
}

- (NSString *)marginHeight
{
    return [self _frameElementImpl]->getAttribute(marginheightAttr);
}

- (void)setMarginHeight:(NSString *)marginHeight
{
    [self _frameElementImpl]->setAttribute(marginheightAttr, marginHeight);
}

- (NSString *)marginWidth
{
    return [self _frameElementImpl]->getAttribute(marginwidthAttr);
}

- (void)setMarginWidth:(NSString *)marginWidth
{
    [self _frameElementImpl]->setAttribute(marginwidthAttr, marginWidth);
}

- (NSString *)name
{
    return [self _frameElementImpl]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _frameElementImpl]->setAttribute(nameAttr, name);
}

- (BOOL)noResize
{
    return [self _frameElementImpl]->getAttribute(noresizeAttr).isNull();
}

- (void)setNoResize:(BOOL)noResize
{
    [self _frameElementImpl]->setAttribute(noresizeAttr, noResize ? "" : 0);
}

- (NSString *)scrolling
{
    return [self _frameElementImpl]->getAttribute(scrollingAttr);
}

- (void)setScrolling:(NSString *)scrolling
{
    [self _frameElementImpl]->setAttribute(scrollingAttr, scrolling);
}

- (NSString *)src
{
    return [self _frameElementImpl]->getAttribute(srcAttr);
}

- (void)setSrc:(NSString *)src
{
    [self _frameElementImpl]->setAttribute(srcAttr, src);
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
    return [self _IFrameElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _IFrameElementImpl]->setAttribute(alignAttr, align);
}

- (NSString *)frameBorder
{
    return [self _IFrameElementImpl]->getAttribute(frameborderAttr);
}

- (void)setFrameBorder:(NSString *)frameBorder
{
    [self _IFrameElementImpl]->setAttribute(frameborderAttr, frameBorder);
}

- (NSString *)height
{
    return [self _IFrameElementImpl]->getAttribute(heightAttr);
}

- (void)setHeight:(NSString *)height
{
    [self _IFrameElementImpl]->setAttribute(heightAttr, height);
}

- (NSString *)longDesc
{
    return [self _IFrameElementImpl]->getAttribute(longdescAttr);
}

- (void)setLongDesc:(NSString *)longDesc
{
    [self _IFrameElementImpl]->setAttribute(longdescAttr, longDesc);
}

- (NSString *)marginHeight
{
    return [self _IFrameElementImpl]->getAttribute(marginheightAttr);
}

- (void)setMarginHeight:(NSString *)marginHeight
{
    [self _IFrameElementImpl]->setAttribute(marginheightAttr, marginHeight);
}

- (NSString *)marginWidth
{
    return [self _IFrameElementImpl]->getAttribute(marginwidthAttr);
}

- (void)setMarginWidth:(NSString *)marginWidth
{
    [self _IFrameElementImpl]->setAttribute(marginwidthAttr, marginWidth);
}

- (NSString *)name
{
    return [self _IFrameElementImpl]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _IFrameElementImpl]->setAttribute(nameAttr, name);
}

- (BOOL)noResize
{
    return [self _IFrameElementImpl]->getAttribute(noresizeAttr).isNull();
}

- (void)setNoResize:(BOOL)noResize
{
    [self _IFrameElementImpl]->setAttribute(noresizeAttr, noResize ? "" : 0);
}

- (NSString *)scrolling
{
    return [self _IFrameElementImpl]->getAttribute(scrollingAttr);
}

- (void)setScrolling:(NSString *)scrolling
{
    [self _IFrameElementImpl]->setAttribute(scrollingAttr, scrolling);
}

- (NSString *)src
{
    return [self _IFrameElementImpl]->getAttribute(srcAttr);
}

- (void)setSrc:(NSString *)src
{
    [self _IFrameElementImpl]->setAttribute(srcAttr, src);
}

- (NSString *)width
{
    return [self _IFrameElementImpl]->getAttribute(widthAttr);
}

- (void)setWidth:(NSString *)width
{
    [self _IFrameElementImpl]->setAttribute(widthAttr, width);
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
    return [self _embedElementImpl]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _embedElementImpl]->setAttribute(alignAttr, align);
}

- (int)height
{
    return [self _embedElementImpl]->getAttribute(heightAttr).toInt();
}

- (void)setHeight:(int)height
{
    DOMString string(QString::number(height));
    [self _embedElementImpl]->setAttribute(heightAttr, string);
}

- (NSString *)name
{
    return [self _embedElementImpl]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _embedElementImpl]->setAttribute(nameAttr, name);
}

- (NSString *)src
{
    return [self _embedElementImpl]->getAttribute(srcAttr);
}

- (void)setSrc:(NSString *)src
{
    [self _embedElementImpl]->setAttribute(srcAttr, src);
}

- (NSString *)type
{
    return [self _embedElementImpl]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _embedElementImpl]->setAttribute(typeAttr, type);
}

- (int)width
{
    return [self _embedElementImpl]->getAttribute(widthAttr).toInt();
}

- (void)setWidth:(int)width
{
    DOMString string(QString::number(width));
    [self _embedElementImpl]->setAttribute(widthAttr, string);
}

@end

// These #imports and "usings" are used only by viewForElement and should be deleted 
// when that function goes away.
#import "render_object.h"
#import "render_replaced.h"
using khtml::RenderObject;
using khtml::RenderWidget;

// This function is used only by the two FormAutoFillTransition categories, and will go away
// as soon as possible.
static NSView *viewForElement(DOMElement *element)
{
    RenderObject *renderer = [element _elementImpl]->renderer();
    if (renderer && renderer->isWidget()) {
        QWidget *widget = static_cast<const RenderWidget *>(renderer)->widget();
        if (widget) {
            widget->populate();
            return widget->getView();
        }
    }
    return nil;
}

@implementation DOMHTMLInputElement(FormAutoFillTransition)

- (NSString *)_displayedValue
{
    // Seems like we could just call [_element value] here but doing so messes up autofill (when you type in
    // a form and it autocompletes, the autocompleted part of the text isn't left selected; instead the insertion
    // point appears between what you typed and the autocompleted part of the text). I think the DOM element's
    // stored value isn't updated until the text in the field is committed when losing focus or perhaps on Enter.
    // Maybe when we switch over to not using NSTextField here then [_element value] will be good enough and
    // we can get rid of this method.
    return [(NSTextField *)viewForElement(self) stringValue];
}

- (BOOL)_isTextField
{
    // We could make this public API as-is, or we could change it into a method that returns whether
    // the element is a text field or a button or ... ?
    static NSArray *textInputTypes = nil;
#ifndef NDEBUG
    static NSArray *nonTextInputTypes = nil;
#endif
    
    NSString *type = [self type];
    
    // No type at all is treated as text type
    if ([type length] == 0)
        return YES;
    
    if (textInputTypes == nil)
        textInputTypes = [[NSSet alloc] initWithObjects:@"text", @"password", @"search", nil];
    
    BOOL isText = [textInputTypes containsObject:[type lowercaseString]];
    
#ifndef NDEBUG
    if (nonTextInputTypes == nil)
        nonTextInputTypes = [[NSSet alloc] initWithObjects:@"isindex", @"checkbox", @"radio", @"submit", @"reset", @"file", @"hidden", @"image", @"button", @"range", nil];
    
    // Catch cases where a new input type has been added that's not in these lists.
    ASSERT(isText || [nonTextInputTypes containsObject:[type lowercaseString]]);
#endif    
    
    return isText;
}

- (void)_setDisplayedValue:(NSString *)newValue
{
    // This method is used by autofill and needs to work even when the field is currently being edited.
    NSTextField *field = (NSTextField *)viewForElement(self);
    NSText *fieldEditor = [field currentEditor];
    if (fieldEditor != nil) {
        [fieldEditor setString:newValue];
        [(NSTextView *)fieldEditor didChangeText];
    } else {
        // Not currently being edited, so we can set the string the simple way. Note that we still can't
        // just use [self setValue:] here because it would break background-color-setting in the current
        // autofill code. When we've adopted a new way to set the background color to indicate autofilled
        // fields, then this case at least can probably change to [self setValue:].
        [field setStringValue:newValue];
    }
}

- (NSRect)_rectOnScreen
{
    // Returns bounding rect of text field, in screen coordinates.
    // FIXME: need a way to determine bounding rect for DOMElements before we can convert this code to
    // not use views. Hyatt says we need to add offsetLeft/offsetTop/width/height to DOMExtensions, but
    // then callers would need to walk up the offsetParent chain to determine real coordinates. So we
    // probably need to (also?) add some call(s) to get the absolute origin.
    NSTextField *field = (NSTextField *)viewForElement(self);
    ASSERT(field != nil);
    NSRect result = [field bounds];
    result = [field convertRect:result toView:nil];
    result.origin = [[field window] convertBaseToScreen:result.origin];
    return result;
}

- (void)_replaceCharactersInRange:(NSRange)targetRange withString:(NSString *)replacementString selectingFromIndex:(int)index
{
    NSText *fieldEditor = [(NSTextField *)viewForElement(self) currentEditor];
    [fieldEditor replaceCharactersInRange:targetRange withString:replacementString];
    
    NSRange selectRange;
    selectRange.location = index;
    selectRange.length = [[self _displayedValue] length] - selectRange.location;
    [fieldEditor setSelectedRange:selectRange];
    
    [(NSTextView *)fieldEditor didChangeText];
}

- (NSRange)_selectedRange
{    
    NSText *editor = [(NSTextField *)viewForElement(self) currentEditor];
    if (editor == nil) {
        return NSMakeRange(NSNotFound, 0);
    }
    
    return [editor selectedRange];
}    

- (void)_setBackgroundColor:(NSColor *)color
{
    // We currently have no DOM-aware way of setting the background color of a text field.
    // Safari autofill uses this method, which is fragile because there's no guarantee that
    // the color set here won't be clobbered by other WebCore code. However, it works OK for
    // Safari autofill's purposes right now. When we switch over to using DOMElements for
    // form controls, we'll probably need a CSS pseudoclass to make this work.
    [(NSTextField *)viewForElement(self) setBackgroundColor:color];
}

@end

@implementation DOMHTMLSelectElement(FormAutoFillTransition)

- (void)_activateItemAtIndex:(int)index
{
    NSPopUpButton *popUp = (NSPopUpButton *)viewForElement(self);
    [popUp selectItemAtIndex:index];
    // Must do this to simulate same side effect as if user made the choice
    [NSApp sendAction:[popUp action] to:[popUp target] from:popUp];
}

- (NSArray *)_optionLabels
{    
    // FIXME 4197997: This code should work, and when it does we can eliminate this method entirely
    // and just have the only current caller (in Safari autofill code) embed this code directly.
    // But at the moment -[DOMHTMLSelectElement options] always returns an empty collection.
#if 0
    DOMHTMLOptionsCollection *options = [self options];
    NSMutableArray *optionLabels = [NSMutableArray array];
    int itemCount = [options length];
    int itemIndex;
    for (itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
        [optionLabels addObject:[(DOMHTMLOptionElement *)[options item:itemIndex] label]];
    }
    return optionLabels;
#endif
    
    // Due to the DOM API brokenness, for now we have to get the titles from the view
    return [(NSPopUpButton *)viewForElement(self) itemTitles];
}

@end
