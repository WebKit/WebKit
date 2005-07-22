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
using DOM::HTMLAttributes;
using DOM::HTMLTags;
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
    return [self _HTMLElementImpl]->getAttribute(HTMLAttributes::idAttr());
}

- (void)setIdName:(NSString *)idName
{
    [self _HTMLElementImpl]->setAttribute(HTMLAttributes::idAttr(), idName);
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
    return [self _linkElementImpl]->getAttribute(HTMLAttributes::media());
}

- (void)setMedia:(NSString *)media
{
    [self _linkElementImpl]->setAttribute(HTMLAttributes::media(), media);
}

- (NSString *)rel
{
    return [self _linkElementImpl]->getAttribute(HTMLAttributes::rel());
}

- (void)setRel:(NSString *)rel
{
    [self _linkElementImpl]->setAttribute(HTMLAttributes::rel(), rel);
}

- (NSString *)rev
{
    return [self _linkElementImpl]->getAttribute(HTMLAttributes::rev());
}

- (void)setRev:(NSString *)rev
{
    [self _linkElementImpl]->setAttribute(HTMLAttributes::rev(), rev);
}

- (NSString *)target
{
    return [self _linkElementImpl]->getAttribute(HTMLAttributes::target());
}

- (void)setTarget:(NSString *)target
{
    [self _linkElementImpl]->setAttribute(HTMLAttributes::target(), target);
}

- (NSString *)type
{
    return [self _linkElementImpl]->getAttribute(HTMLAttributes::type());
}

- (void)setType:(NSString *)type
{
    [self _linkElementImpl]->setAttribute(HTMLAttributes::type(), type);
}

@end

@implementation DOMHTMLTitleElement

- (HTMLTitleElementImpl *)_titleElementImpl
{
    return static_cast<HTMLTitleElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)text
{
    return [self _titleElementImpl]->getAttribute(HTMLAttributes::text());
}

- (void)setText:(NSString *)text
{
    [self _titleElementImpl]->setAttribute(HTMLAttributes::text(), text);
}

@end

@implementation DOMHTMLMetaElement

- (HTMLMetaElementImpl *)_metaElementImpl
{
    return static_cast<HTMLMetaElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)content
{
    return [self _metaElementImpl]->getAttribute(HTMLAttributes::content());
}

- (void)setContent:(NSString *)content
{
    [self _metaElementImpl]->setAttribute(HTMLAttributes::content(), content);
}

- (NSString *)httpEquiv
{
    return [self _metaElementImpl]->getAttribute(HTMLAttributes::http_equiv());
}

- (void)setHttpEquiv:(NSString *)httpEquiv
{
    [self _metaElementImpl]->setAttribute(HTMLAttributes::http_equiv(), httpEquiv);
}

- (NSString *)name
{
    return [self _metaElementImpl]->getAttribute(HTMLAttributes::name());
}

- (void)setName:(NSString *)name
{
    [self _metaElementImpl]->setAttribute(HTMLAttributes::name(), name);
}

- (NSString *)scheme
{
    return [self _metaElementImpl]->getAttribute(HTMLAttributes::scheme());
}

- (void)setScheme:(NSString *)scheme
{
    [self _metaElementImpl]->setAttribute(HTMLAttributes::scheme(), scheme);
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
    [self _baseElementImpl]->setAttribute(HTMLAttributes::href(), href);
}

- (NSString *)target
{
    return [self _baseElementImpl]->getAttribute(HTMLAttributes::target());
}

- (void)setTarget:(NSString *)target
{
    [self _baseElementImpl]->setAttribute(HTMLAttributes::scheme(), target);
}

@end

@implementation DOMHTMLStyleElement

- (HTMLStyleElementImpl *)_styleElementImpl
{
    return static_cast<HTMLStyleElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)disabled
{
    return ![self _styleElementImpl]->getAttribute(HTMLAttributes::disabled()).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _styleElementImpl]->setAttribute(HTMLAttributes::disabled(), disabled ? "" : 0);
}

- (NSString *)media
{
    return [self _styleElementImpl]->getAttribute(HTMLAttributes::media());
}

- (void)setMedia:(NSString *)media
{
    [self _styleElementImpl]->setAttribute(HTMLAttributes::media(), media);
}

- (NSString *)type
{
    return [self _styleElementImpl]->getAttribute(HTMLAttributes::type());
}

- (void)setType:(NSString *)type
{
    [self _styleElementImpl]->setAttribute(HTMLAttributes::type(), type);
}

@end

@implementation DOMHTMLBodyElement

- (HTMLBodyElementImpl *)_bodyElementImpl
{
    return static_cast<HTMLBodyElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)aLink
{
    return [self _bodyElementImpl]->getAttribute(HTMLAttributes::alink());
}

- (void)setALink:(NSString *)aLink
{
    [self _bodyElementImpl]->setAttribute(HTMLAttributes::alink(), aLink);
}

- (NSString *)background
{
    return [self _bodyElementImpl]->getAttribute(HTMLAttributes::background());
}

- (void)setBackground:(NSString *)background
{
    [self _bodyElementImpl]->setAttribute(HTMLAttributes::background(), background);
}

- (NSString *)bgColor
{
    return [self _bodyElementImpl]->getAttribute(HTMLAttributes::bgcolor());
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _bodyElementImpl]->setAttribute(HTMLAttributes::bgcolor(), bgColor);
}

- (NSString *)link
{
    return [self _bodyElementImpl]->getAttribute(HTMLAttributes::link());
}

- (void)setLink:(NSString *)link
{
    [self _bodyElementImpl]->setAttribute(HTMLAttributes::link(), link);
}

- (NSString *)text
{
    return [self _bodyElementImpl]->getAttribute(HTMLAttributes::text());
}

- (void)setText:(NSString *)text
{
    [self _bodyElementImpl]->setAttribute(HTMLAttributes::text(), text);
}

- (NSString *)vLink
{
    return [self _bodyElementImpl]->getAttribute(HTMLAttributes::vlink());
}

- (void)setVLink:(NSString *)vLink
{
    [self _bodyElementImpl]->setAttribute(HTMLAttributes::vlink(), vLink);
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
    return [self _formElementImpl]->getAttribute(HTMLAttributes::name());
}

- (void)setName:(NSString *)name
{
    [self _formElementImpl]->setAttribute(HTMLAttributes::name(), name);
}

- (NSString *)acceptCharset
{
    return [self _formElementImpl]->getAttribute(HTMLAttributes::accept_charset());
}

- (void)setAcceptCharset:(NSString *)acceptCharset
{
    [self _formElementImpl]->setAttribute(HTMLAttributes::accept_charset(), acceptCharset);
}

- (NSString *)action
{
    return [self _formElementImpl]->getAttribute(HTMLAttributes::action());
}

- (void)setAction:(NSString *)action
{
    [self _formElementImpl]->setAttribute(HTMLAttributes::action(), action);
}

- (NSString *)enctype
{
    return [self _formElementImpl]->getAttribute(HTMLAttributes::enctype());
}

- (void)setEnctype:(NSString *)enctype
{
    [self _formElementImpl]->setAttribute(HTMLAttributes::enctype(), enctype);
}

- (NSString *)method
{
    return [self _formElementImpl]->getAttribute(HTMLAttributes::method());
}

- (void)setMethod:(NSString *)method
{
    [self _formElementImpl]->setAttribute(HTMLAttributes::method(), method);
}

- (NSString *)target
{
    return [self _formElementImpl]->getAttribute(HTMLAttributes::target());
}

- (void)setTarget:(NSString *)target
{
    [self _formElementImpl]->setAttribute(HTMLAttributes::target(), target);
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

- (long)size
{
    return [self _selectElementImpl]->size();
}

- (void)setSize:(long)size
{
    [self _selectElementImpl]->setSize(size);
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

- (long)index
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

- (long)maxLength
{
    return [self _inputElementImpl]->maxLength();
}

- (void)setMaxLength:(long)maxLength
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

- (unsigned long)size
{
    return [self _inputElementImpl]->size();
}

- (void)setSize:(unsigned long)size
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

@implementation DOMHTMLInputElement (DOMHTMLInputElementExtensions)

- (BOOL)isTextField
{
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
    return [self _textAreaElementImpl]->getAttribute(HTMLAttributes::accesskey());
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _textAreaElementImpl]->setAttribute(HTMLAttributes::accesskey(), accessKey);
}

- (long)cols
{
    return [self _textAreaElementImpl]->getAttribute(HTMLAttributes::cols()).toInt();
}

- (void)setCols:(long)cols
{
    DOMString value(QString::number(cols));
    [self _textAreaElementImpl]->setAttribute(HTMLAttributes::cols(), value);
}

- (BOOL)disabled
{
    return [self _textAreaElementImpl]->getAttribute(HTMLAttributes::disabled()).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _textAreaElementImpl]->setAttribute(HTMLAttributes::disabled(), disabled ? "" : 0);
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
    return [self _textAreaElementImpl]->getAttribute(HTMLAttributes::readonly()).isNull();
}

- (void)setReadOnly:(BOOL)readOnly
{
    [self _textAreaElementImpl]->setAttribute(HTMLAttributes::readonly(), readOnly ? "" : 0);
}

- (long)rows
{
    return [self _textAreaElementImpl]->getAttribute(HTMLAttributes::rows()).toInt();
}

- (void)setRows:(long)rows
{
	DOMString value(QString::number(rows));
    [self _textAreaElementImpl]->setAttribute(HTMLAttributes::rows(), value);
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
    return [self _buttonElementImpl]->getAttribute(HTMLAttributes::accesskey());
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _buttonElementImpl]->setAttribute(HTMLAttributes::accesskey(), accessKey);
}

- (BOOL)disabled
{
    return [self _buttonElementImpl]->getAttribute(HTMLAttributes::disabled()).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _buttonElementImpl]->setAttribute(HTMLAttributes::disabled(), disabled ? "" : 0);
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
    return [self _buttonElementImpl]->getAttribute(HTMLAttributes::value());
}

- (void)setValue:(NSString *)value
{
    [self _buttonElementImpl]->setAttribute(HTMLAttributes::value(), value);
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
    return [self _labelElementImpl]->getAttribute(HTMLAttributes::accesskey());
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _labelElementImpl]->setAttribute(HTMLAttributes::accesskey(), accessKey);
}

- (NSString *)htmlFor
{
    return [self _labelElementImpl]->getAttribute(HTMLAttributes::forAttr());
}

- (void)setHtmlFor:(NSString *)htmlFor
{
    [self _labelElementImpl]->setAttribute(HTMLAttributes::forAttr(), htmlFor);
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
    return [self _legendElementImpl]->getAttribute(HTMLAttributes::accesskey());
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _legendElementImpl]->setAttribute(HTMLAttributes::accesskey(), accessKey);
}

- (NSString *)align
{
    return [self _legendElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _legendElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

@end

@implementation DOMHTMLUListElement

- (HTMLUListElementImpl *)_uListElementImpl
{
    return static_cast<HTMLUListElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)compact
{
    return [self _uListElementImpl]->getAttribute(HTMLAttributes::compact()).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _uListElementImpl]->setAttribute(HTMLAttributes::compact(), compact ? "" : 0);
}

- (NSString *)type
{
    return [self _uListElementImpl]->getAttribute(HTMLAttributes::type());
}

- (void)setType:(NSString *)type
{
    [self _uListElementImpl]->setAttribute(HTMLAttributes::type(), type);
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

- (long)start
{
    return [self _oListElementImpl]->getAttribute(HTMLAttributes::start()).toInt();
}

- (void)setStart:(long)start
{
	DOMString value(QString::number(start));
    [self _oListElementImpl]->setAttribute(HTMLAttributes::start(), value);
}

- (NSString *)type
{
    return [self _oListElementImpl]->getAttribute(HTMLAttributes::type());
}

- (void)setType:(NSString *)type
{
    [self _oListElementImpl]->setAttribute(HTMLAttributes::type(), type);
}

@end

@implementation DOMHTMLDListElement

- (HTMLDListElementImpl *)_dListElementImpl
{
    return static_cast<HTMLDListElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)compact
{
    return [self _dListElementImpl]->getAttribute(HTMLAttributes::compact()).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _dListElementImpl]->setAttribute(HTMLAttributes::compact(), compact ? "" : 0);
}

@end

@implementation DOMHTMLDirectoryElement

- (HTMLDirectoryElementImpl *)_directoryListElementImpl
{
    return static_cast<HTMLDirectoryElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)compact
{
    return [self _directoryListElementImpl]->getAttribute(HTMLAttributes::compact()).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _directoryListElementImpl]->setAttribute(HTMLAttributes::compact(), compact ? "" : 0);
}

@end

@implementation DOMHTMLMenuElement

- (HTMLMenuElementImpl *)_menuListElementImpl
{
    return static_cast<HTMLMenuElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (BOOL)compact
{
    return [self _menuListElementImpl]->getAttribute(HTMLAttributes::compact()).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _menuListElementImpl]->setAttribute(HTMLAttributes::compact(), compact ? "" : 0);
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

- (long)value
{
    return [self _liElementImpl]->value();
}

- (void)setValue:(long)value
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
    return [self _quoteElementImpl]->getAttribute(HTMLAttributes::cite());
}

- (void)setCite:(NSString *)cite
{
    [self _quoteElementImpl]->setAttribute(HTMLAttributes::cite(), cite);
}

@end

@implementation DOMHTMLDivElement

- (HTMLDivElementImpl *)_divElementImpl
{
    return static_cast<HTMLDivElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _divElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _divElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

@end

@implementation DOMHTMLParagraphElement

- (HTMLParagraphElementImpl *)_paragraphElementImpl
{
    return static_cast<HTMLParagraphElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _paragraphElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _paragraphElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

@end

@implementation DOMHTMLHeadingElement

- (HTMLHeadingElementImpl *)_headingElementImpl
{
    return static_cast<HTMLHeadingElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _headingElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _headingElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

@end

@implementation DOMHTMLPreElement

- (HTMLPreElementImpl *)_preElementImpl
{
    return static_cast<HTMLPreElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (long)width
{
    return [self _preElementImpl]->getAttribute(HTMLAttributes::width()).toInt();
}

- (void)setWidth:(long)width
{
    DOMString string(QString::number(width));
    [self _preElementImpl]->setAttribute(HTMLAttributes::width(), string);
}

@end

@implementation DOMHTMLBRElement

- (HTMLBRElementImpl *)_BRElementImpl
{
    return static_cast<HTMLBRElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)clear
{
    return [self _BRElementImpl]->getAttribute(HTMLAttributes::clear());
}

- (void)setClear:(NSString *)clear
{
    [self _BRElementImpl]->setAttribute(HTMLAttributes::clear(), clear);
}

@end

@implementation DOMHTMLBaseFontElement

- (HTMLBaseFontElementImpl *)_baseFontElementImpl
{
    return static_cast<HTMLBaseFontElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)color
{
    return [self _baseFontElementImpl]->getAttribute(HTMLAttributes::color());
}

- (void)setColor:(NSString *)color
{
    [self _baseFontElementImpl]->setAttribute(HTMLAttributes::color(), color);
}

- (NSString *)face
{
    return [self _baseFontElementImpl]->getAttribute(HTMLAttributes::face());
}

- (void)setFace:(NSString *)face
{
    [self _baseFontElementImpl]->setAttribute(HTMLAttributes::face(), face);
}

- (NSString *)size
{
    return [self _baseFontElementImpl]->getAttribute(HTMLAttributes::size());
}

- (void)setSize:(NSString *)size
{
    [self _baseFontElementImpl]->setAttribute(HTMLAttributes::size(), size);
}

@end

@implementation DOMHTMLFontElement

- (HTMLFontElementImpl *)_fontElementImpl
{
    return static_cast<HTMLFontElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)color
{
    return [self _fontElementImpl]->getAttribute(HTMLAttributes::color());
}

- (void)setColor:(NSString *)color
{
    [self _fontElementImpl]->setAttribute(HTMLAttributes::color(), color);
}

- (NSString *)face
{
    return [self _fontElementImpl]->getAttribute(HTMLAttributes::face());
}

- (void)setFace:(NSString *)face
{
    [self _fontElementImpl]->setAttribute(HTMLAttributes::face(), face);
}

- (NSString *)size
{
    return [self _fontElementImpl]->getAttribute(HTMLAttributes::size());
}

- (void)setSize:(NSString *)size
{
    [self _fontElementImpl]->setAttribute(HTMLAttributes::size(), size);
}

@end

@implementation DOMHTMLHRElement

- (HTMLHRElementImpl *)_HRElementImpl
{
    return static_cast<HTMLHRElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _HRElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _HRElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

- (BOOL)noShade
{
    return [self _HRElementImpl]->getAttribute(HTMLAttributes::noshade()).isNull();
}

- (void)setNoShade:(BOOL)noShade
{
    [self _HRElementImpl]->setAttribute(HTMLAttributes::noshade(), noShade ? "" : 0);
}

- (NSString *)size
{
    return [self _HRElementImpl]->getAttribute(HTMLAttributes::size());
}

- (void)setSize:(NSString *)size
{
    [self _HRElementImpl]->setAttribute(HTMLAttributes::size(), size);
}

- (NSString *)width
{
    return [self _HRElementImpl]->getAttribute(HTMLAttributes::width());
}

- (void)setWidth:(NSString *)width
{
    [self _HRElementImpl]->setAttribute(HTMLAttributes::width(), width);
}

@end

@implementation DOMHTMLModElement

- (HTMLElementImpl *)_modElementImpl
{
    return static_cast<HTMLElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)cite
{
    return [self _modElementImpl]->getAttribute(HTMLAttributes::cite());
}

- (void)setCite:(NSString *)cite
{
    [self _modElementImpl]->setAttribute(HTMLAttributes::cite(), cite);
}

- (NSString *)dateTime
{
    return [self _modElementImpl]->getAttribute(HTMLAttributes::datetime());
}

- (void)setDateTime:(NSString *)dateTime
{
    [self _modElementImpl]->setAttribute(HTMLAttributes::datetime(), dateTime);
}

@end

@implementation DOMHTMLAnchorElement

- (HTMLAnchorElementImpl *)_anchorElementImpl
{
    return static_cast<HTMLAnchorElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)accessKey
{
    return [self _anchorElementImpl]->getAttribute(HTMLAttributes::accesskey());
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _anchorElementImpl]->setAttribute(HTMLAttributes::accesskey(), accessKey);
}

- (NSString *)charset
{
    return [self _anchorElementImpl]->getAttribute(HTMLAttributes::charset());
}

- (void)setCharset:(NSString *)charset
{
    [self _anchorElementImpl]->setAttribute(HTMLAttributes::charset(), charset);
}

- (NSString *)coords
{
    return [self _anchorElementImpl]->getAttribute(HTMLAttributes::coords());
}

- (void)setCoords:(NSString *)coords
{
    [self _anchorElementImpl]->setAttribute(HTMLAttributes::coords(), coords);
}

- (NSString *)href
{
    return [self _anchorElementImpl]->href();
}

- (void)setHref:(NSString *)href
{
    [self _anchorElementImpl]->setAttribute(HTMLAttributes::href(), href);
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

- (long)tabIndex
{
    return [self _anchorElementImpl]->tabIndex();
}

- (void)setTabIndex:(long)tabIndex
{
    [self _anchorElementImpl]->setTabIndex(tabIndex);
}

- (NSString *)target
{
    return [self _anchorElementImpl]->getAttribute(HTMLAttributes::target());
}

- (void)setTarget:(NSString *)target
{
    [self _anchorElementImpl]->setAttribute(HTMLAttributes::target(), target);
}

- (NSString *)type
{
    return [self _anchorElementImpl]->getAttribute(HTMLAttributes::type());
}

- (void)setType:(NSString *)type
{
    [self _anchorElementImpl]->setAttribute(HTMLAttributes::type(), type);
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
    return [self _imageElementImpl]->getAttribute(HTMLAttributes::name());
}

- (void)setName:(NSString *)name
{
    [self _imageElementImpl]->setAttribute(HTMLAttributes::name(), name);
}

- (NSString *)align
{
    return [self _imageElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _imageElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

- (NSString *)alt
{
    return [self _imageElementImpl]->getAttribute(HTMLAttributes::alt());
}

- (void)setAlt:(NSString *)alt
{
    [self _imageElementImpl]->setAttribute(HTMLAttributes::alt(), alt);
}

- (NSString *)border
{
    return [self _imageElementImpl]->getAttribute(HTMLAttributes::border());
}

- (void)setBorder:(NSString *)border
{
    [self _imageElementImpl]->setAttribute(HTMLAttributes::border(), border);
}

- (long)height
{
    return [self _imageElementImpl]->getAttribute(HTMLAttributes::height()).toInt();
}

- (void)setHeight:(long)height
{
    DOMString string(QString::number(height));
    [self _imageElementImpl]->setAttribute(HTMLAttributes::height(), string);
}

- (long)hspace
{
    return [self _imageElementImpl]->getAttribute(HTMLAttributes::hspace()).toInt();
}

- (void)setHspace:(long)hspace
{
    DOMString string(QString::number(hspace));
    [self _imageElementImpl]->setAttribute(HTMLAttributes::hspace(), string);
}

- (BOOL)isMap
{
    return [self _imageElementImpl]->getAttribute(HTMLAttributes::ismap()).isNull();
}

- (void)setIsMap:(BOOL)isMap
{
    [self _imageElementImpl]->setAttribute(HTMLAttributes::ismap(), isMap ? "" : 0);
}

- (NSString *)longDesc
{
    return [self _imageElementImpl]->getAttribute(HTMLAttributes::longdesc());
}

- (void)setLongDesc:(NSString *)longDesc
{
    [self _imageElementImpl]->setAttribute(HTMLAttributes::longdesc(), longDesc);
}

- (NSString *)src
{
    return [self _imageElementImpl]->src();
}

- (void)setSrc:(NSString *)src
{
    [self _imageElementImpl]->setAttribute(HTMLAttributes::src(), src);
}

- (NSString *)useMap
{
    return [self _imageElementImpl]->getAttribute(HTMLAttributes::usemap());
}

- (void)setUseMap:(NSString *)useMap
{
    [self _imageElementImpl]->setAttribute(HTMLAttributes::usemap(), useMap);
}

- (long)vspace
{
    return [self _imageElementImpl]->getAttribute(HTMLAttributes::vspace()).toInt();
}

- (void)setVspace:(long)vspace
{
    DOMString string(QString::number(vspace));
    [self _imageElementImpl]->setAttribute(HTMLAttributes::vspace(), string);
}

- (long)width
{
    return [self _imageElementImpl]->getAttribute(HTMLAttributes::width()).toInt();
}

- (void)setWidth:(long)width
{
    DOMString string(QString::number(width));
    [self _imageElementImpl]->setAttribute(HTMLAttributes::width(), string);
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
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::code());
}

- (void)setCode:(NSString *)code
{
    [self _objectElementImpl]->setAttribute(HTMLAttributes::code(), code);
}

- (NSString *)align
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _objectElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

- (NSString *)archive
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::archive());
}

- (void)setArchive:(NSString *)archive
{
    [self _objectElementImpl]->setAttribute(HTMLAttributes::archive(), archive);
}

- (NSString *)border
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::border());
}

- (void)setBorder:(NSString *)border
{
    [self _objectElementImpl]->setAttribute(HTMLAttributes::border(), border);
}

- (NSString *)codeBase
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::codebase());
}

- (void)setCodeBase:(NSString *)codeBase
{
    [self _objectElementImpl]->setAttribute(HTMLAttributes::codebase(), codeBase);
}

- (NSString *)codeType
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::codetype());
}

- (void)setCodeType:(NSString *)codeType
{
    [self _objectElementImpl]->setAttribute(HTMLAttributes::codetype(), codeType);
}

- (NSString *)data
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::data());
}

- (void)setData:(NSString *)data
{
    [self _objectElementImpl]->setAttribute(HTMLAttributes::data(), data);
}

- (BOOL)declare
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::declare()).isNull();
}

- (void)setDeclare:(BOOL)declare
{
    [self _objectElementImpl]->setAttribute(HTMLAttributes::declare(), declare ? "" : 0);
}

- (NSString *)height
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::height());
}

- (void)setHeight:(NSString *)height
{
    [self _objectElementImpl]->setAttribute(HTMLAttributes::height(), height);
}

- (long)hspace
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::hspace()).toInt();
}

- (void)setHspace:(long)hspace
{
    DOMString string(QString::number(hspace));
    [self _objectElementImpl]->setAttribute(HTMLAttributes::hspace(), string);
}

- (NSString *)name
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::name());
}

- (void)setName:(NSString *)name
{
    [self _objectElementImpl]->setAttribute(HTMLAttributes::name(), name);
}

- (NSString *)standby
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::standby());
}

- (void)setStandby:(NSString *)standby
{
    [self _objectElementImpl]->setAttribute(HTMLAttributes::standby(), standby);
}

- (long)tabIndex
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::tabindex()).toInt();
}

- (void)setTabIndex:(long)tabIndex
{
    DOMString string(QString::number(tabIndex));
    [self _objectElementImpl]->setAttribute(HTMLAttributes::tabindex(), string);
}

- (NSString *)type
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::type());
}

- (void)setType:(NSString *)type
{
    [self _objectElementImpl]->setAttribute(HTMLAttributes::type(), type);
}

- (NSString *)useMap
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::usemap());
}

- (void)setUseMap:(NSString *)useMap
{
    [self _objectElementImpl]->setAttribute(HTMLAttributes::usemap(), useMap);
}

- (long)vspace
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::vspace()).toInt();
}

- (void)setVspace:(long)vspace
{
    DOMString string(QString::number(vspace));
    [self _objectElementImpl]->setAttribute(HTMLAttributes::vspace(), string);
}

- (NSString *)width
{
    return [self _objectElementImpl]->getAttribute(HTMLAttributes::width());
}

- (void)setWidth:(NSString *)width
{
    [self _objectElementImpl]->setAttribute(HTMLAttributes::width(), width);
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
    return [self _paramElementImpl]->getAttribute(HTMLAttributes::name());
}

- (void)setName:(NSString *)name
{
    [self _paramElementImpl]->setAttribute(HTMLAttributes::name(), name);
}

- (NSString *)type
{
    return [self _paramElementImpl]->getAttribute(HTMLAttributes::type());
}

- (void)setType:(NSString *)type
{
    [self _paramElementImpl]->setAttribute(HTMLAttributes::type(), type);
}

- (NSString *)value
{
    return [self _paramElementImpl]->getAttribute(HTMLAttributes::value());
}

- (void)setValue:(NSString *)value
{
    [self _paramElementImpl]->setAttribute(HTMLAttributes::value(), value);
}

- (NSString *)valueType
{
    return [self _paramElementImpl]->getAttribute(HTMLAttributes::valuetype());
}

- (void)setValueType:(NSString *)valueType
{
    [self _paramElementImpl]->setAttribute(HTMLAttributes::valuetype(), valueType);
}

@end

@implementation DOMHTMLAppletElement

- (HTMLAppletElementImpl *)_appletElementImpl
{
    return static_cast<HTMLAppletElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)align
{
    return [self _appletElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _appletElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

- (NSString *)alt
{
    return [self _appletElementImpl]->getAttribute(HTMLAttributes::alt());
}

- (void)setAlt:(NSString *)alt
{
    [self _appletElementImpl]->setAttribute(HTMLAttributes::alt(), alt);
}

- (NSString *)archive
{
    return [self _appletElementImpl]->getAttribute(HTMLAttributes::archive());
}

- (void)setArchive:(NSString *)archive
{
    [self _appletElementImpl]->setAttribute(HTMLAttributes::archive(), archive);
}

- (NSString *)code
{
    return [self _appletElementImpl]->getAttribute(HTMLAttributes::code());
}

- (void)setCode:(NSString *)code
{
    [self _appletElementImpl]->setAttribute(HTMLAttributes::code(), code);
}

- (NSString *)codeBase
{
    return [self _appletElementImpl]->getAttribute(HTMLAttributes::codebase());
}

- (void)setCodeBase:(NSString *)codeBase
{
    [self _appletElementImpl]->setAttribute(HTMLAttributes::codebase(), codeBase);
}

- (NSString *)height
{
    return [self _appletElementImpl]->getAttribute(HTMLAttributes::height());
}

- (void)setHeight:(NSString *)height
{
    [self _appletElementImpl]->setAttribute(HTMLAttributes::height(), height);
}

- (long)hspace
{
    return [self _appletElementImpl]->getAttribute(HTMLAttributes::hspace()).toInt();
}

- (void)setHspace:(long)hspace
{
    DOMString string(QString::number(hspace));
    [self _appletElementImpl]->setAttribute(HTMLAttributes::hspace(), string);
}

- (NSString *)name
{
    return [self _appletElementImpl]->getAttribute(HTMLAttributes::name());
}

- (void)setName:(NSString *)name
{
    [self _appletElementImpl]->setAttribute(HTMLAttributes::name(), name);
}

- (NSString *)object
{
    return [self _appletElementImpl]->getAttribute(HTMLAttributes::object());
}

- (void)setObject:(NSString *)object
{
    [self _appletElementImpl]->setAttribute(HTMLAttributes::object(), object);
}

- (long)vspace
{
    return [self _appletElementImpl]->getAttribute(HTMLAttributes::vspace()).toInt();
}

- (void)setVspace:(long)vspace
{
    DOMString string(QString::number(vspace));
    [self _appletElementImpl]->setAttribute(HTMLAttributes::vspace(), string);
}

- (NSString *)width
{
    return [self _appletElementImpl]->getAttribute(HTMLAttributes::width());
}

- (void)setWidth:(NSString *)width
{
    [self _appletElementImpl]->setAttribute(HTMLAttributes::width(), width);
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
    return [self _mapElementImpl]->getAttribute(HTMLAttributes::name());
}

- (void)setName:(NSString *)name
{
    [self _mapElementImpl]->setAttribute(HTMLAttributes::name(), name);
}

@end

@implementation DOMHTMLAreaElement

- (HTMLAreaElementImpl *)_areaElementImpl
{
    return static_cast<HTMLAreaElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)accessKey
{
    return [self _areaElementImpl]->getAttribute(HTMLAttributes::accesskey());
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _areaElementImpl]->setAttribute(HTMLAttributes::accesskey(), accessKey);
}

- (NSString *)alt
{
    return [self _areaElementImpl]->getAttribute(HTMLAttributes::alt());
}

- (void)setAlt:(NSString *)alt
{
    [self _areaElementImpl]->setAttribute(HTMLAttributes::alt(), alt);
}

- (NSString *)coords
{
    return [self _areaElementImpl]->getAttribute(HTMLAttributes::coords());
}

- (void)setCoords:(NSString *)coords
{
    [self _areaElementImpl]->setAttribute(HTMLAttributes::coords(), coords);
}

- (NSString *)href
{
    return [self _areaElementImpl]->href();
}

- (void)setHref:(NSString *)href
{
    [self _areaElementImpl]->setAttribute(HTMLAttributes::href(), href);
}

- (BOOL)noHref
{
    return [self _areaElementImpl]->getAttribute(HTMLAttributes::nohref()).isNull();
}

- (void)setNoHref:(BOOL)noHref
{
    [self _areaElementImpl]->setAttribute(HTMLAttributes::nohref(), noHref ? "" : 0);
}

- (NSString *)shape
{
    return [self _areaElementImpl]->getAttribute(HTMLAttributes::shape());
}

- (void)setShape:(NSString *)shape
{
    [self _areaElementImpl]->setAttribute(HTMLAttributes::shape(), shape);
}

- (long)tabIndex
{
    return [self _areaElementImpl]->getAttribute(HTMLAttributes::tabindex()).toInt();
}

- (void)setTabIndex:(long)tabIndex
{
    DOMString string(QString::number(tabIndex));
    [self _areaElementImpl]->setAttribute(HTMLAttributes::tabindex(), string);
}

- (NSString *)target
{
    return [self _areaElementImpl]->getAttribute(HTMLAttributes::target());
}

- (void)setTarget:(NSString *)target
{
    [self _areaElementImpl]->setAttribute(HTMLAttributes::target(), target);
}

@end

@implementation DOMHTMLScriptElement

- (HTMLScriptElementImpl *)_scriptElementImpl
{
    return static_cast<HTMLScriptElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)text
{
    return [self _scriptElementImpl]->getAttribute(HTMLAttributes::text());
}

- (void)setText:(NSString *)text
{
    [self _scriptElementImpl]->setAttribute(HTMLAttributes::text(), text);
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
    return [self _scriptElementImpl]->getAttribute(HTMLAttributes::charset());
}

- (void)setCharset:(NSString *)charset
{
    [self _scriptElementImpl]->setAttribute(HTMLAttributes::charset(), charset);
}

- (BOOL)defer
{
    return [self _scriptElementImpl]->getAttribute(HTMLAttributes::defer()).isNull();
}

- (void)setDefer:(BOOL)defer
{
    [self _scriptElementImpl]->setAttribute(HTMLAttributes::defer(), defer ? "" : 0);
}

- (NSString *)src
{
    return [self _scriptElementImpl]->getAttribute(HTMLAttributes::src());
}

- (void)setSrc:(NSString *)src
{
    [self _scriptElementImpl]->setAttribute(HTMLAttributes::src(), src);
}

- (NSString *)type
{
    return [self _scriptElementImpl]->getAttribute(HTMLAttributes::type());
}

- (void)setType:(NSString *)type
{
    [self _scriptElementImpl]->setAttribute(HTMLAttributes::type(), type);
}

@end

@implementation DOMHTMLTableCaptionElement

- (NSString *)align
{
    return [self _tableCaptionElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _tableCaptionElementImpl]->setAttribute(HTMLAttributes::align(), align);
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
    return [self _tableSectionElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _tableSectionElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

- (NSString *)ch
{
    return [self _tableSectionElementImpl]->getAttribute(HTMLAttributes::charoff());
}

- (void)setCh:(NSString *)ch
{
    [self _tableSectionElementImpl]->setAttribute(HTMLAttributes::charoff(), ch);
}

- (NSString *)chOff
{
    return [self _tableSectionElementImpl]->getAttribute(HTMLAttributes::charoff());
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableSectionElementImpl]->setAttribute(HTMLAttributes::charoff(), chOff);
}

- (NSString *)vAlign
{
    return [self _tableSectionElementImpl]->getAttribute(HTMLAttributes::valign());
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableSectionElementImpl]->setAttribute(HTMLAttributes::valign(), vAlign);
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
    return [self _tableElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _tableElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

- (NSString *)bgColor
{
    return [self _tableElementImpl]->getAttribute(HTMLAttributes::bgcolor());
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _tableElementImpl]->setAttribute(HTMLAttributes::bgcolor(), bgColor);
}

- (NSString *)border
{
    return [self _tableElementImpl]->getAttribute(HTMLAttributes::border());
}

- (void)setBorder:(NSString *)border
{
    [self _tableElementImpl]->setAttribute(HTMLAttributes::border(), border);
}

- (NSString *)cellPadding
{
    return [self _tableElementImpl]->getAttribute(HTMLAttributes::cellpadding());
}

- (void)setCellPadding:(NSString *)cellPadding
{
    [self _tableElementImpl]->setAttribute(HTMLAttributes::cellpadding(), cellPadding);
}

- (NSString *)cellSpacing
{
    return [self _tableElementImpl]->getAttribute(HTMLAttributes::cellspacing());
}

- (void)setCellSpacing:(NSString *)cellSpacing
{
    [self _tableElementImpl]->setAttribute(HTMLAttributes::cellspacing(), cellSpacing);
}

- (NSString *)frameBorders
{
    return [self _tableElementImpl]->getAttribute(HTMLAttributes::frame());
}

- (void)setFrameBorders:(NSString *)frameBorders
{
    [self _tableElementImpl]->setAttribute(HTMLAttributes::frame(), frameBorders);
}

- (NSString *)rules
{
    return [self _tableElementImpl]->getAttribute(HTMLAttributes::rules());
}

- (void)setRules:(NSString *)rules
{
    [self _tableElementImpl]->setAttribute(HTMLAttributes::rules(), rules);
}

- (NSString *)summary
{
    return [self _tableElementImpl]->getAttribute(HTMLAttributes::summary());
}

- (void)setSummary:(NSString *)summary
{
    [self _tableElementImpl]->setAttribute(HTMLAttributes::summary(), summary);
}

- (NSString *)width
{
    return [self _tableElementImpl]->getAttribute(HTMLAttributes::width());
}

- (void)setWidth:(NSString *)width
{
    [self _tableElementImpl]->setAttribute(HTMLAttributes::width(), width);
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
    return [self _tableColElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _tableColElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

- (NSString *)ch
{
    return [self _tableColElementImpl]->getAttribute(HTMLAttributes::charoff());
}

- (void)setCh:(NSString *)ch
{
    [self _tableColElementImpl]->setAttribute(HTMLAttributes::charoff(), ch);
}

- (NSString *)chOff
{
    return [self _tableColElementImpl]->getAttribute(HTMLAttributes::charoff());
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableColElementImpl]->setAttribute(HTMLAttributes::charoff(), chOff);
}

- (long)span
{
    return [self _tableColElementImpl]->getAttribute(HTMLAttributes::span()).toInt();
}

- (void)setSpan:(long)span
{
    DOMString string(QString::number(span));
    [self _tableColElementImpl]->setAttribute(HTMLAttributes::span(), string);
}

- (NSString *)vAlign
{
    return [self _tableColElementImpl]->getAttribute(HTMLAttributes::valign());
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableColElementImpl]->setAttribute(HTMLAttributes::valign(), vAlign);
}

- (NSString *)width
{
    return [self _tableColElementImpl]->getAttribute(HTMLAttributes::width());
}

- (void)setWidth:(NSString *)width
{
    [self _tableColElementImpl]->setAttribute(HTMLAttributes::width(), width);
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
    return [self _tableRowElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _tableRowElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

- (NSString *)bgColor
{
    return [self _tableRowElementImpl]->getAttribute(HTMLAttributes::bgcolor());
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _tableRowElementImpl]->setAttribute(HTMLAttributes::bgcolor(), bgColor);
}

- (NSString *)ch
{
    return [self _tableRowElementImpl]->getAttribute(HTMLAttributes::charoff());
}

- (void)setCh:(NSString *)ch
{
    [self _tableRowElementImpl]->setAttribute(HTMLAttributes::charoff(), ch);
}

- (NSString *)chOff
{
    return [self _tableRowElementImpl]->getAttribute(HTMLAttributes::charoff());
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableRowElementImpl]->setAttribute(HTMLAttributes::charoff(), chOff);
}

- (NSString *)vAlign
{
    return [self _tableRowElementImpl]->getAttribute(HTMLAttributes::valign());
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableRowElementImpl]->setAttribute(HTMLAttributes::valign(), vAlign);
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
    return [self _tableCellElementImpl]->getAttribute(HTMLAttributes::abbr());
}

- (void)setAbbr:(NSString *)abbr
{
    [self _tableCellElementImpl]->setAttribute(HTMLAttributes::abbr(), abbr);
}

- (NSString *)align
{
    return [self _tableCellElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _tableCellElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

- (NSString *)axis
{
    return [self _tableCellElementImpl]->getAttribute(HTMLAttributes::axis());
}

- (void)setAxis:(NSString *)axis
{
    [self _tableCellElementImpl]->setAttribute(HTMLAttributes::axis(), axis);
}

- (NSString *)bgColor
{
    return [self _tableCellElementImpl]->getAttribute(HTMLAttributes::bgcolor());
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _tableCellElementImpl]->setAttribute(HTMLAttributes::bgcolor(), bgColor);
}

- (NSString *)ch
{
    return [self _tableCellElementImpl]->getAttribute(HTMLAttributes::charoff());
}

- (void)setCh:(NSString *)ch
{
    [self _tableCellElementImpl]->setAttribute(HTMLAttributes::charoff(), ch);
}

- (NSString *)chOff
{
    return [self _tableCellElementImpl]->getAttribute(HTMLAttributes::charoff());
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableCellElementImpl]->setAttribute(HTMLAttributes::charoff(), chOff);
}

- (long)colSpan
{
    return [self _tableCellElementImpl]->getAttribute(HTMLAttributes::colspan()).toInt();
}

- (void)setColSpan:(long)colSpan
{
    DOMString string(QString::number(colSpan));
    [self _tableCellElementImpl]->setAttribute(HTMLAttributes::colspan(), string);
}

- (NSString *)headers
{
    return [self _tableCellElementImpl]->getAttribute(HTMLAttributes::headers());
}

- (void)setHeaders:(NSString *)headers
{
    [self _tableCellElementImpl]->setAttribute(HTMLAttributes::headers(), headers);
}

- (NSString *)height
{
    return [self _tableCellElementImpl]->getAttribute(HTMLAttributes::height());
}

- (void)setHeight:(NSString *)height
{
    [self _tableCellElementImpl]->setAttribute(HTMLAttributes::height(), height);
}

- (BOOL)noWrap
{
    return [self _tableCellElementImpl]->getAttribute(HTMLAttributes::nowrap()).isNull();
}

- (void)setNoWrap:(BOOL)noWrap
{
    [self _tableCellElementImpl]->setAttribute(HTMLAttributes::nowrap(), noWrap ? "" : 0);
}

- (long)rowSpan
{
    return [self _tableCellElementImpl]->getAttribute(HTMLAttributes::rowspan()).toInt();
}

- (void)setRowSpan:(long)rowSpan
{
    DOMString string(QString::number(rowSpan));
    [self _tableCellElementImpl]->setAttribute(HTMLAttributes::rowspan(), string);
}

- (NSString *)scope
{
    return [self _tableCellElementImpl]->getAttribute(HTMLAttributes::scope());
}

- (void)setScope:(NSString *)scope
{
    [self _tableCellElementImpl]->setAttribute(HTMLAttributes::scope(), scope);
}

- (NSString *)vAlign
{
    return [self _tableCellElementImpl]->getAttribute(HTMLAttributes::valign());
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableCellElementImpl]->setAttribute(HTMLAttributes::valign(), vAlign);
}

- (NSString *)width
{
    return [self _tableCellElementImpl]->getAttribute(HTMLAttributes::width());
}

- (void)setWidth:(NSString *)width
{
    [self _tableCellElementImpl]->setAttribute(HTMLAttributes::width(), width);
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
    return [self _frameSetElementImpl]->getAttribute(HTMLAttributes::rows());
}

- (void)setRows:(NSString *)rows
{
    [self _frameSetElementImpl]->setAttribute(HTMLAttributes::rows(), rows);
}

- (NSString *)cols
{
    return [self _frameSetElementImpl]->getAttribute(HTMLAttributes::cols());
}

- (void)setCols:(NSString *)cols
{
    [self _frameSetElementImpl]->setAttribute(HTMLAttributes::cols(), cols);
}

@end

@implementation DOMHTMLFrameElement

- (HTMLFrameElementImpl *)_frameElementImpl
{
    return static_cast<HTMLFrameElementImpl *>(DOM_cast<NodeImpl *>(_internal));
}

- (NSString *)frameBorder
{
    return [self _frameElementImpl]->getAttribute(HTMLAttributes::frameborder());
}

- (void)setFrameBorder:(NSString *)frameBorder
{
    [self _frameElementImpl]->setAttribute(HTMLAttributes::frameborder(), frameBorder);
}

- (NSString *)longDesc
{
    return [self _frameElementImpl]->getAttribute(HTMLAttributes::longdesc());
}

- (void)setLongDesc:(NSString *)longDesc
{
    [self _frameElementImpl]->setAttribute(HTMLAttributes::longdesc(), longDesc);
}

- (NSString *)marginHeight
{
    return [self _frameElementImpl]->getAttribute(HTMLAttributes::marginheight());
}

- (void)setMarginHeight:(NSString *)marginHeight
{
    [self _frameElementImpl]->setAttribute(HTMLAttributes::marginheight(), marginHeight);
}

- (NSString *)marginWidth
{
    return [self _frameElementImpl]->getAttribute(HTMLAttributes::marginwidth());
}

- (void)setMarginWidth:(NSString *)marginWidth
{
    [self _frameElementImpl]->setAttribute(HTMLAttributes::marginwidth(), marginWidth);
}

- (NSString *)name
{
    return [self _frameElementImpl]->getAttribute(HTMLAttributes::name());
}

- (void)setName:(NSString *)name
{
    [self _frameElementImpl]->setAttribute(HTMLAttributes::name(), name);
}

- (BOOL)noResize
{
    return [self _frameElementImpl]->getAttribute(HTMLAttributes::noresize()).isNull();
}

- (void)setNoResize:(BOOL)noResize
{
    [self _frameElementImpl]->setAttribute(HTMLAttributes::noresize(), noResize ? "" : 0);
}

- (NSString *)scrolling
{
    return [self _frameElementImpl]->getAttribute(HTMLAttributes::scrolling());
}

- (void)setScrolling:(NSString *)scrolling
{
    [self _frameElementImpl]->setAttribute(HTMLAttributes::scrolling(), scrolling);
}

- (NSString *)src
{
    return [self _frameElementImpl]->getAttribute(HTMLAttributes::src());
}

- (void)setSrc:(NSString *)src
{
    [self _frameElementImpl]->setAttribute(HTMLAttributes::src(), src);
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
    return [self _IFrameElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _IFrameElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

- (NSString *)frameBorder
{
    return [self _IFrameElementImpl]->getAttribute(HTMLAttributes::frameborder());
}

- (void)setFrameBorder:(NSString *)frameBorder
{
    [self _IFrameElementImpl]->setAttribute(HTMLAttributes::frameborder(), frameBorder);
}

- (NSString *)height
{
    return [self _IFrameElementImpl]->getAttribute(HTMLAttributes::height());
}

- (void)setHeight:(NSString *)height
{
    [self _IFrameElementImpl]->setAttribute(HTMLAttributes::height(), height);
}

- (NSString *)longDesc
{
    return [self _IFrameElementImpl]->getAttribute(HTMLAttributes::longdesc());
}

- (void)setLongDesc:(NSString *)longDesc
{
    [self _IFrameElementImpl]->setAttribute(HTMLAttributes::longdesc(), longDesc);
}

- (NSString *)marginHeight
{
    return [self _IFrameElementImpl]->getAttribute(HTMLAttributes::marginheight());
}

- (void)setMarginHeight:(NSString *)marginHeight
{
    [self _IFrameElementImpl]->setAttribute(HTMLAttributes::marginheight(), marginHeight);
}

- (NSString *)marginWidth
{
    return [self _IFrameElementImpl]->getAttribute(HTMLAttributes::marginwidth());
}

- (void)setMarginWidth:(NSString *)marginWidth
{
    [self _IFrameElementImpl]->setAttribute(HTMLAttributes::marginwidth(), marginWidth);
}

- (NSString *)name
{
    return [self _IFrameElementImpl]->getAttribute(HTMLAttributes::name());
}

- (void)setName:(NSString *)name
{
    [self _IFrameElementImpl]->setAttribute(HTMLAttributes::name(), name);
}

- (BOOL)noResize
{
    return [self _IFrameElementImpl]->getAttribute(HTMLAttributes::noresize()).isNull();
}

- (void)setNoResize:(BOOL)noResize
{
    [self _IFrameElementImpl]->setAttribute(HTMLAttributes::noresize(), noResize ? "" : 0);
}

- (NSString *)scrolling
{
    return [self _IFrameElementImpl]->getAttribute(HTMLAttributes::scrolling());
}

- (void)setScrolling:(NSString *)scrolling
{
    [self _IFrameElementImpl]->setAttribute(HTMLAttributes::scrolling(), scrolling);
}

- (NSString *)src
{
    return [self _IFrameElementImpl]->getAttribute(HTMLAttributes::src());
}

- (void)setSrc:(NSString *)src
{
    [self _IFrameElementImpl]->setAttribute(HTMLAttributes::src(), src);
}

- (NSString *)width
{
    return [self _IFrameElementImpl]->getAttribute(HTMLAttributes::width());
}

- (void)setWidth:(NSString *)width
{
    [self _IFrameElementImpl]->setAttribute(HTMLAttributes::width(), width);
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
    return [self _embedElementImpl]->getAttribute(HTMLAttributes::align());
}

- (void)setAlign:(NSString *)align
{
    [self _embedElementImpl]->setAttribute(HTMLAttributes::align(), align);
}

- (long)height
{
    return [self _embedElementImpl]->getAttribute(HTMLAttributes::height()).toInt();
}

- (void)setHeight:(long)height
{
    DOMString string(QString::number(height));
    [self _embedElementImpl]->setAttribute(HTMLAttributes::height(), string);
}

- (NSString *)name
{
    return [self _embedElementImpl]->getAttribute(HTMLAttributes::name());
}

- (void)setName:(NSString *)name
{
    [self _embedElementImpl]->setAttribute(HTMLAttributes::name(), name);
}

- (NSString *)src
{
    return [self _embedElementImpl]->getAttribute(HTMLAttributes::src());
}

- (void)setSrc:(NSString *)src
{
    [self _embedElementImpl]->setAttribute(HTMLAttributes::src(), src);
}

- (NSString *)type
{
    return [self _embedElementImpl]->getAttribute(HTMLAttributes::type());
}

- (void)setType:(NSString *)type
{
    [self _embedElementImpl]->setAttribute(HTMLAttributes::type(), type);
}

- (long)width
{
    return [self _embedElementImpl]->getAttribute(HTMLAttributes::width()).toInt();
}

- (void)setWidth:(long)width
{
    DOMString string(QString::number(width));
    [self _embedElementImpl]->setAttribute(HTMLAttributes::width(), string);
}

@end
