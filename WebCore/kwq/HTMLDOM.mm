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

#import <dom/html_element.h>
#import <html/html_baseimpl.h>
#import <html/html_documentimpl.h>
#import <html/html_elementimpl.h>
#import <html/html_headimpl.h>
#import <html/html_miscimpl.h>
#import <misc/htmlattrs.h>
#import <xml/dom_elementimpl.h>
#import <xml/dom_nodeimpl.h>

#import "DOMInternal.h"
#import "KWQAssertions.h"

using DOM::ElementImpl;
using DOM::HTMLBaseElementImpl;
using DOM::HTMLBodyElementImpl;
using DOM::HTMLCollectionImpl;
using DOM::HTMLDocumentImpl;
using DOM::HTMLElementImpl;
using DOM::HTMLHeadElementImpl;
using DOM::HTMLHtmlElementImpl;
using DOM::HTMLLinkElementImpl;
using DOM::HTMLMetaElementImpl;
using DOM::HTMLStyleElementImpl;
using DOM::HTMLTitleElementImpl;
using DOM::NameNodeListImpl;
using DOM::NodeImpl;

@interface DOMHTMLCollection (HTMLCollectionInternal)
+ (DOMHTMLCollection *)_collectionWithImpl:(HTMLCollectionImpl *)impl;
@end;

@interface DOMHTMLElement (HTMLElementInternal)
+ (DOMHTMLElement *)_elementWithImpl:(HTMLElementImpl *)impl;
- (HTMLElementImpl *)_HTMLElementImpl;
@end;

@implementation DOMHTMLCollection

- (id)_initWithCollectionImpl:(HTMLCollectionImpl *)impl
{
    ASSERT(impl);
    
    [super _init];
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setDOMWrapperForImpl(self, impl);
    return self;
}

+ (DOMHTMLCollection *)_collectionWithImpl:(HTMLCollectionImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithCollectionImpl:impl] autorelease];
}

- (HTMLCollectionImpl *)_collectionImpl
{
    return reinterpret_cast<HTMLCollectionImpl *>(_internal);
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

@implementation DOMHTMLElement

- (id)_initWithElementImpl:(HTMLElementImpl *)impl
{
    ASSERT(impl);
    
    [super _init];
    _internal = reinterpret_cast<DOMObjectInternal *>(impl);
    impl->ref();
    setDOMWrapperForImpl(self, impl);
    return self;
}

+ (DOMHTMLElement *)_elementWithImpl:(HTMLElementImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapperForImpl(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithElementImpl:impl] autorelease];
}

- (HTMLElementImpl *)_HTMLElementImpl
{
    return reinterpret_cast<HTMLElementImpl *>(_internal);
}

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

@implementation DOMHTMLDocument

- (HTMLDocumentImpl *)_HTMLDocumentImpl
{
    return reinterpret_cast<HTMLDocumentImpl *>(_internal);
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

@implementation DOMHTMLHtmlElement

- (HTMLHtmlElementImpl *)_HTMLHtmlElementImpl
{
    return reinterpret_cast<HTMLHtmlElementImpl *>(_internal);
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
    return reinterpret_cast<HTMLHeadElementImpl *>(_internal);
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
    return reinterpret_cast<HTMLLinkElementImpl *>(_internal);
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
    return [self _linkElementImpl]->getAttribute(ATTR_HREF);
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
    return reinterpret_cast<HTMLTitleElementImpl *>(_internal);
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
    return reinterpret_cast<HTMLMetaElementImpl *>(_internal);
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
    return reinterpret_cast<HTMLBaseElementImpl *>(_internal);
}

- (NSString *)href
{
    return [self _baseElementImpl]->getAttribute(ATTR_HREF);
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
    return reinterpret_cast<HTMLStyleElementImpl *>(_internal);
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
    return reinterpret_cast<HTMLBodyElementImpl *>(_internal);
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

- (DOMHTMLCollection *)elements
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (long)length
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setName:(NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)acceptCharset
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAcceptCharset:(NSString *)acceptCharset
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)action
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAction:(NSString *)action
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)enctype
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setEnctype:(NSString *)enctype
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)method
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setMethod:(NSString *)method
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)target
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setTarget:(NSString *)target
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)submit
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)reset
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLIsIndexElement

- (NSString *)form
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (NSString *)prompt
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setPrompt:(NSString *)prompt
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLSelectElement

- (NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (long)selectedIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setSelectedIndex:(long)selectedIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)value
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setValue:(NSString *)value
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)length
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (DOMHTMLFormElement *)form
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (DOMHTMLOptionsCollection *)options
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setDisabled:(BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)multiple
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setMultiple:(BOOL)multiple
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setName:(NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)size
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setSize:(long)size
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setTabIndex:(long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)add:(DOMHTMLElement *)element :(DOMHTMLElement *)before
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)remove:(long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)blur
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)focus
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLOptGroupElement

- (BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setDisabled:(BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)label
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setLabel:(NSString *)label
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLOptionElement

- (DOMHTMLFormElement *)form
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (BOOL)defaultSelected
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setDefaultSelected:(BOOL)defaultSelected
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)text
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setIndex:(long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setDisabled:(BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)label
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setLabel:(NSString *)label
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)selected
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setSelected:(BOOL)selected
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)value
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setValue:(NSString *)value
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLInputElement

- (NSString *)defaultValue
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setDefaultValue:(NSString *)defaultValue
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)defaultChecked
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setDefaultChecked:(BOOL)defaultChecked
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMHTMLFormElement *)form
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (NSString *)accept
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAccept:(NSString *)accept
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)accessKey
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAccessKey:(NSString *)accessKey
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)alt
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlt:(NSString *)alt
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)checked
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setChecked:(BOOL)checked
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setDisabled:(BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)maxLength
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setMaxLength:(long)maxLength
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setName:(NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)readOnly
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setReadOnly:(BOOL)readOnly
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)size
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setSize:(NSString *)size
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)src
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setSrc:(NSString *)src
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setTabIndex:(long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setType:(NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)useMap
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setUseMap:(NSString *)useMap
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)value
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setValue:(NSString *)value
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)blur
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)focus
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)select
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)click
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLTextAreaElement

- (NSString *)defaultValue
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setDefaultValue:(NSString *)defaultValue
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMHTMLFormElement *)form
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (NSString *)accessKey
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAccessKey:(NSString *)accessKey
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)cols
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setCols:(long)cols
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setDisabled:(BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setName:(NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)readOnly
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setReadOnly:(BOOL)readOnly
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)rows
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setRows:(long)rows
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setTabIndex:(long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setType:(NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)value
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setValue:(NSString *)value
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)blur
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)focus
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)select
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLButtonElement

- (DOMHTMLFormElement *)form
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (NSString *)accessKey
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAccessKey:(NSString *)accessKey
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setDisabled:(BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setName:(NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setTabIndex:(long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (NSString *)value
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setValue:(NSString *)value
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLLabelElement

- (DOMHTMLFormElement *)form
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (NSString *)accessKey
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAccessKey:(NSString *)accessKey
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)htmlFor
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHtmlFor:(NSString *)htmlFor
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLFieldSetElement

- (DOMHTMLFormElement *)form
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

@end

@implementation DOMHTMLLegendElement

- (DOMHTMLFormElement *)form
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (NSString *)accessKey
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAccessKey:(NSString *)accessKey
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLUListElement

- (BOOL)compact
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setCompact:(BOOL)compact
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setType:(NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLOListElement

- (BOOL)compact
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setCompact:(BOOL)compact
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)start
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setStart:(long)start
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setType:(NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLDListElement

- (BOOL)compact
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setCompact:(BOOL)compact
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLDirectoryElement

- (BOOL)compact
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setCompact:(BOOL)compact
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLMenuElement

- (BOOL)compact
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setCompact:(BOOL)compact
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLLIElement

- (NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setType:(NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)value
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setValue:(long)value
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLDivElement

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLParagraphElement

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLHeadingElement

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLQuoteElement

- (NSString *)cite
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCite:(NSString *)cite
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLPreElement

- (long)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setWidth:(long)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLBRElement

- (NSString *)clear
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setClear:(NSString *)clear
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLBaseFontElement

- (NSString *)color
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setColor:(NSString *)color
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)face
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setFace:(NSString *)face
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)size
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setSize:(NSString *)size
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLFontElement

- (NSString *)color
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setColor:(NSString *)color
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)face
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setFace:(NSString *)face
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)size
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setSize:(NSString *)size
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLHRElement

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)noShade
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setNoShade:(BOOL)noShade
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)size
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setSize:(NSString *)size
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setWidth:(NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLModElement

- (NSString *)cite
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCite:(NSString *)cite
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)dateTime
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setDateTime:(NSString *)dateTime
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLAnchorElement

- (NSString *)accessKey
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAccessKey:(NSString *)accessKey
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)charset
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCharset:(NSString *)charset
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)coords
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCoords:(NSString *)coords
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)href
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHref:(NSString *)href
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)hreflang
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHreflang:(NSString *)hreflang
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setName:(NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)rel
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setRel:(NSString *)rel
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)rev
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setRev:(NSString *)rev
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)shape
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setShape:(NSString *)shape
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setTabIndex:(long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)target
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setTarget:(NSString *)target
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setType:(NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)blur
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)focus
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLImageElement

- (NSString *)lowSrc
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setLowSrc:(NSString *)lowSrc
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setName:(NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)alt
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlt:(NSString *)alt
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)border
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setBorder:(NSString *)border
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)height
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHeight:(NSString *)height
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)hspace
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHspace:(NSString *)hspace
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)isMap
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setIsMap:(BOOL)isMap
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)longDesc
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setLongDesc:(NSString *)longDesc
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)src
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setSrc:(NSString *)src
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)useMap
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setUseMap:(NSString *)useMap
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)vspace
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setVspace:(NSString *)vspace
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setWidth:(NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLObjectElement

- (DOMHTMLFormElement *)form
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (NSString *)code
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCode:(NSString *)code
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)archive
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setArchive:(NSString *)archive
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)border
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setBorder:(NSString *)border
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)codeBase
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCodeBase:(NSString *)codeBase
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)codeType
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCodeType:(NSString *)codeType
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)data
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setData:(NSString *)data
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)declare
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setDeclare:(BOOL)declare
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)height
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHeight:(NSString *)height
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)hspace
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHspace:(long)hspace
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setName:(NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)standby
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setStandby:(NSString *)standby
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setTabIndex:(long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setType:(NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)useMap
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setUseMap:(NSString *)useMap
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)vspace
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setVspace:(long)vspace
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setWidth:(NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMDocument *)contentDocument
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

@end

@implementation DOMHTMLParamElement

- (NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setName:(NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setType:(NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)value
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setValue:(NSString *)value
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)valueType
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setValueType:(NSString *)valueType
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLAppletElement

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)alt
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlt:(NSString *)alt
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)archive
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setArchive:(NSString *)archive
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)code
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCode:(NSString *)code
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)codeBase
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCodeBase:(NSString *)codeBase
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)codeType
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCodeType:(NSString *)codeType
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)height
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHeight:(NSString *)height
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)hspace
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHspace:(long)hspace
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setName:(NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)object
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setObject:(NSString *)object
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)vspace
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setVspace:(long)vspace
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setWidth:(NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLMapElement

- (DOMHTMLCollection *)areas
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setName:(NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLAreaElement

- (NSString *)accessKey
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAccessKey:(NSString *)accessKey
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)alt
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlt:(NSString *)alt
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)coords
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCoords:(NSString *)coords
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)href
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHref:(NSString *)href
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)noHref
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setNoHref:(BOOL)noHref
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)shape
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setShape:(NSString *)shape
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setTabIndex:(long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)target
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setTarget:(NSString *)target
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLScriptElement

- (NSString *)text
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setText:(NSString *)text
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)htmlFor
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHtmlFor:(NSString *)htmlFor
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)event
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setEvent:(NSString *)event
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)charset
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCharset:(NSString *)charset
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)defer
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setDefer:(BOOL)defer
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)src
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setSrc:(NSString *)src
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setType:(NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLTableCaptionElement

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLTableSectionElement

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)ch
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCh:(NSString *)ch
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)chOff
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setChOff:(NSString *)chOff
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)vAlign
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setVAlign:(NSString *)vAlign
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMHTMLCollection *)rows
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (DOMHTMLElement *)insertRow:(long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)deleteRow:(long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLTableElement

- (DOMHTMLTableCaptionElement *)caption
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCaption:(DOMHTMLTableCaptionElement *)caption
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMHTMLTableSectionElement *)tHead
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setTHead:(DOMHTMLTableSectionElement *)tHead
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMHTMLTableSectionElement *)tFoot
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setTFoot:(DOMHTMLTableSectionElement *)tFoot
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMHTMLCollection *)rows
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (DOMHTMLCollection *)tBodies
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)bgColor
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setBgColor:(NSString *)bgColor
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)border
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setBorder:(NSString *)border
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)cellPadding
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCellPadding:(NSString *)cellPadding
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)cellSpacing
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCellSpacing:(NSString *)cellSpacing
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)frame
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setFrame:(NSString *)frame
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)rules
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setRules:(NSString *)rules
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)summary
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setSummary:(NSString *)summary
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setWidth:(NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMHTMLElement *)createTHead
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)deleteTHead
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMHTMLElement *)createTFoot
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)deleteTFoot
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMHTMLElement *)createCaption
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)deleteCaption
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMHTMLElement *)insertRow:(long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");    return nil;
}

- (void)deleteRow:(long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLTableColElement

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)ch
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCh:(NSString *)ch
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)chOff
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setChOff:(NSString *)chOff
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)span
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setSpan:(long)span
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)vAlign
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setVAlign:(NSString *)vAlign
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setWidth:(NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLTableRowElement

- (long)rowIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setRowIndex:(long)rowIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)sectionRowIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setSectionRowIndex:(long)sectionRowIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMHTMLCollection *)cells
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCells:(DOMHTMLCollection *)cells // Is cells really read/write?
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)bgColor
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setBgColor:(NSString *)bgColor
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)ch
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCh:(NSString *)ch
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)chOff
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setChOff:(NSString *)chOff
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)vAlign
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setVAlign:(NSString *)vAlign
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMHTMLElement *)insertCell:(long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)deleteCell:(long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLTableCellElement

- (long)cellIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setCellIndex:(long)cellIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)abbr
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAbbr:(NSString *)abbr
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)axis
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAxis:(NSString *)axis
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)bgColor
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setBgColor:(NSString *)bgColor
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)ch
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCh:(NSString *)ch
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)chOff
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setChOff:(NSString *)chOff
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)colSpan
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setColSpan:(long)colSpan
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)headers
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHeaders:(NSString *)headers
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)height
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHeight:(NSString *)height
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)noWrap
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setNoWrap:(BOOL)noWrap
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (long)rowSpan
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)setRowSpan:(long)rowSpan
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)scope
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setScope:(NSString *)scope
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)vAlign
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setVAlign:(NSString *)vAlign
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setWidth:(NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLFrameSetElement

- (NSString *)cols
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCols:(NSString *)cols
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)rows
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setRows:(NSString *)rows
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation DOMHTMLFrameElement

- (NSString *)frameBorder
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setFrameBorder:(NSString *)frameBorder
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)longDesc
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setLongDesc:(NSString *)longDesc
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)marginHeight
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setMarginHeight:(NSString *)marginHeight
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)marginWidth
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setMarginWidth:(NSString *)marginWidth
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setName:(NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (BOOL)noResize
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setNoResize:(BOOL)noResize
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)scrolling
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setScrolling:(NSString *)scrolling
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)src
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setSrc:(NSString *)src
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMDocument *)contentDocument
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

@end

@implementation DOMHTMLIFrameElement

- (NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setAlign:(NSString *)align
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)frameBorder
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setFrameBorder:(NSString *)frameBorder
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)height
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHeight:(NSString *)height
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)longDesc
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setLongDesc:(NSString *)longDesc
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)marginHeight
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setMarginHeight:(NSString *)marginHeight
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)marginWidth
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setMarginWidth:(NSString *)marginWidth
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setName:(NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)scrolling
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setScrolling:(NSString *)scrolling
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)src
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setSrc:(NSString *)src
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setWidth:(NSString *)width
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMDocument *)contentDocument
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

@end
