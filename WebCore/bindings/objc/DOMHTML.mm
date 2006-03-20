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

#import "PlatformString.h"
#import "html_baseimpl.h"
#import "html_blockimpl.h"
#import "HTMLDocument.h"
#import "HTMLElement.h"
#import "HTMLOptionsCollection.h"
#import "HTMLFormElement.h"
#import "HTMLInputElement.h"
#import "HTMLIsIndexElement.h"
#import "HTMLSelectElement.h"
#import "HTMLOptionElement.h"
#import "HTMLTextAreaElement.h"
#import "HTMLButtonElement.h"
#import "HTMLLabelElement.h"
#import "HTMLLegendElement.h"
#import "HTMLFieldSetElement.h"
#import "HTMLOptGroupElement.h"
#import "html_headimpl.h"
#import "html_imageimpl.h"
#import "html_inlineimpl.h"
#import "html_listimpl.h"
#import "HTMLFormCollection.h"
#import "HTMLBaseFontElement.h"
#import "html_objectimpl.h"
#import "html_tableimpl.h"
#import "dom_elementimpl.h"
#import "ContainerNode.h"
#import "NameNodeList.h"
#import "markup.h"

#import "DOMExtensions.h"
#import "DOMInternal.h"
#import "DOMPrivate.h"
#import "DOMHTMLInternal.h"
#import <kxmlcore/Assertions.h>
#import "FoundationExtras.h"

using namespace WebCore;
using namespace WebCore::HTMLNames;

// FIXME: This code should be using the impl methods instead of doing so many get/setAttribute calls.
// FIXME: This code should be generated.

@interface DOMHTMLCollection (WebCoreInternal)
+ (DOMHTMLCollection *)_collectionWith:(HTMLCollection *)impl;
@end

@interface DOMHTMLElement (WebCoreInternal)
+ (DOMHTMLElement *)_elementWith:(HTMLElement *)impl;
- (HTMLElement *)_HTMLElement;
@end

@interface DOMHTMLFormElement (WebCoreInternal)
+ (DOMHTMLFormElement *)_formElementWith:(HTMLFormElement *)impl;
@end

@interface DOMHTMLTableCaptionElement (WebCoreInternal)
+ (DOMHTMLTableCaptionElement *)_tableCaptionElementWith:(HTMLTableCaptionElement *)impl;
- (HTMLTableCaptionElement *)_tableCaptionElement;
@end

@interface DOMHTMLTableSectionElement (WebCoreInternal)
+ (DOMHTMLTableSectionElement *)_tableSectionElementWith:(HTMLTableSectionElement *)impl;
- (HTMLTableSectionElement *)_tableSectionElement;
@end

@interface DOMHTMLTableElement (WebCoreInternal)
+ (DOMHTMLTableElement *)_tableElementWith:(HTMLTableElement *)impl;
- (HTMLTableElement *)_tableElement;
@end

@interface DOMHTMLTableCellElement (WebCoreInternal)
+ (DOMHTMLTableCellElement *)_tableCellElementWith:(HTMLTableCellElement *)impl;
- (HTMLTableCellElement *)_tableCellElement;
@end

//------------------------------------------------------------------------------------------

@implementation DOMHTMLCollection

- (void)dealloc
{
    if (_internal) {
        DOM_cast<HTMLCollection *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<HTMLCollection *>(_internal)->deref();
    }
    [super finalize];
}

- (HTMLCollection *)_collection
{
    return DOM_cast<HTMLCollection *>(_internal);
}

- (unsigned)length
{
    return [self _collection]->length();
}

- (DOMNode *)item:(unsigned)index
{
    return [DOMNode _nodeWith:[self _collection]->item(index)];
}

- (DOMNode *)namedItem:(NSString *)name
{
    return [DOMNode _nodeWith:[self _collection]->namedItem(name)];
}

@end

@implementation DOMHTMLCollection (WebCoreInternal)

- (id)_initWithCollection:(HTMLCollection *)impl
{
    ASSERT(impl);
    
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMHTMLCollection *)_collectionWith:(HTMLCollection *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithCollection:impl] autorelease];
}

@end

@implementation DOMHTMLOptionsCollection

- (void)dealloc
{
    if (_internal) {
        DOM_cast<HTMLOptionsCollection *>(_internal)->deref();
    }
    [super dealloc];
}

- (void)finalize
{
    if (_internal) {
        DOM_cast<HTMLOptionsCollection *>(_internal)->deref();
    }
    [super finalize];
}

- (id)_initWithOptionsCollection:(HTMLOptionsCollection *)impl
{
    ASSERT(impl);
    
    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMHTMLOptionsCollection *)_optionsCollectionWith:(HTMLOptionsCollection *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[self alloc] _initWithOptionsCollection:impl] autorelease];
}

- (HTMLOptionsCollection *)_optionsCollection
{
    return DOM_cast<HTMLOptionsCollection *>(_internal);
}

- (unsigned)length
{
    return [self _optionsCollection]->length();
}

- (void)setLength:(unsigned)length
{
    ExceptionCode ec;
    [self _optionsCollection]->setLength(length, ec);
    raiseOnDOMError(ec);
}

- (DOMNode *)item:(unsigned)index
{
    return [DOMNode _nodeWith:[self _optionsCollection]->item(index)];
}

- (DOMNode *)namedItem:(NSString *)name
{
    return [DOMNode _nodeWith:[self _optionsCollection]->namedItem(name)];
}

@end

@implementation DOMHTMLElement

- (NSString *)idName
{
    return [self _HTMLElement]->getAttribute(idAttr);
}

- (void)setIdName:(NSString *)idName
{
    [self _HTMLElement]->setAttribute(idAttr, idName);
}

- (NSString *)title
{
    return [self _HTMLElement]->title();
}

- (NSString *)titleDisplayString
{
    return [self _HTMLElement]->title().replace('\\', [self _element]->getDocument()->backslashAsCurrencySymbol());
}

- (void)setTitle:(NSString *)title
{
    [self _HTMLElement]->setTitle(title);
}

- (NSString *)lang
{
    return [self _HTMLElement]->lang();
}

- (void)setLang:(NSString *)lang
{
    [self _HTMLElement]->setLang(lang);
}

- (NSString *)dir
{
    return [self _HTMLElement]->dir();
}

- (void)setDir:(NSString *)dir
{
    [self _HTMLElement]->setDir(dir);
}

- (NSString *)className
{
    return [self _HTMLElement]->className();
}

- (void)setClassName:(NSString *)className
{
    [self _HTMLElement]->setClassName(className);
}

@end

@implementation DOMHTMLElement (WebCoreInternal)

+ (DOMHTMLElement *)_elementWith:(HTMLElement *)impl
{
    return static_cast<DOMHTMLElement *>([DOMNode _nodeWith:impl]);
}

- (HTMLElement *)_HTMLElement
{
    return static_cast<HTMLElement *>(DOM_cast<Node *>(_internal));
}

@end

@implementation DOMHTMLElement (DOMHTMLElementExtensions)

- (NSString *)innerHTML
{
    return [self _HTMLElement]->innerHTML();
}

- (void)setInnerHTML:(NSString *)innerHTML
{
    int exception = 0;
    [self _HTMLElement]->setInnerHTML(innerHTML, exception);
    raiseOnDOMError(exception);
}

- (NSString *)outerHTML
{
    return [self _HTMLElement]->outerHTML();
}

- (void)setOuterHTML:(NSString *)outerHTML
{
    int exception = 0;
    [self _HTMLElement]->setOuterHTML(outerHTML, exception);
    raiseOnDOMError(exception);
}

- (NSString *)innerText
{
    return [self _HTMLElement]->innerText();
}

- (void)setInnerText:(NSString *)innerText
{
    int exception = 0;
    [self _HTMLElement]->setInnerText(innerText, exception);
    raiseOnDOMError(exception);
}

- (NSString *)outerText
{
    return [self _HTMLElement]->outerText();
}

- (void)setOuterText:(NSString *)outerText
{
    int exception = 0;
    [self _HTMLElement]->setOuterText(outerText, exception);
    raiseOnDOMError(exception);
}

- (DOMHTMLCollection *)children
{
    HTMLCollection *collection = new HTMLCollection([self _HTMLElement], HTMLCollection::NODE_CHILDREN);
    return [DOMHTMLCollection _collectionWith:collection];
}

- (NSString *)contentEditable
{
    return [self _HTMLElement]->contentEditable();
}

- (void)setContentEditable:(NSString *)contentEditable
{
    [self _HTMLElement]->setContentEditable(contentEditable);
}

- (BOOL)isContentEditable
{
    return [self _HTMLElement]->isContentEditable();
}

@end

@implementation DOMHTMLDocument

- (HTMLDocument *)_HTMLDocument
{
    return static_cast<HTMLDocument *>(DOM_cast<Node *>(_internal));
}

- (NSString *)title
{
    return [self _HTMLDocument]->title();
}

- (void)setTitle:(NSString *)title
{
    [self _HTMLDocument]->setTitle(title);
}

- (NSString *)referrer
{
     return [self _HTMLDocument]->referrer();
}

- (NSString *)domain
{
     return [self _HTMLDocument]->domain();
}

- (NSString *)URL
{
    return [self _HTMLDocument]->URL().getNSString();
}

- (DOMHTMLElement *)body
{
    return [DOMHTMLElement _elementWith:[self _HTMLDocument]->body()];
}

- (DOMHTMLCollection *)images
{
    HTMLCollection *collection = new HTMLCollection([self _HTMLDocument], HTMLCollection::DOC_IMAGES);
    return [DOMHTMLCollection _collectionWith:collection];
}

- (DOMHTMLCollection *)applets
{
    HTMLCollection *collection = new HTMLCollection([self _HTMLDocument], HTMLCollection::DOC_APPLETS);
    return [DOMHTMLCollection _collectionWith:collection];
}

- (DOMHTMLCollection *)links
{
    HTMLCollection *collection = new HTMLCollection([self _HTMLDocument], HTMLCollection::DOC_LINKS);
    return [DOMHTMLCollection _collectionWith:collection];
}

- (DOMHTMLCollection *)forms
{
    HTMLCollection *collection = new HTMLCollection([self _HTMLDocument], HTMLCollection::DOC_FORMS);
    return [DOMHTMLCollection _collectionWith:collection];
}

- (DOMHTMLCollection *)anchors
{
    HTMLCollection *collection = new HTMLCollection([self _HTMLDocument], HTMLCollection::DOC_ANCHORS);
    return [DOMHTMLCollection _collectionWith:collection];
}

- (void)setBody:(DOMHTMLElement *)body
{
    ExceptionCode ec = 0;
    [self _HTMLDocument]->setBody([body _HTMLElement], ec);
    raiseOnDOMError(ec);
}

- (NSString *)cookie
{
    return [self _HTMLDocument]->cookie();
}

- (void)setCookie:(NSString *)cookie
{
    [self _HTMLDocument]->setCookie(cookie);
}

- (void)open
{
    [self _HTMLDocument]->open();
}

- (void)close
{
    [self _HTMLDocument]->close();
}

- (void)write:(NSString *)text
{
    [self _HTMLDocument]->write(text);
}

- (void)writeln:(NSString *)text
{
    [self _HTMLDocument]->writeln(text);
}

- (DOMElement *)getElementById:(NSString *)elementId
{
    return [DOMElement _elementWith:[self _HTMLDocument]->getElementById(elementId)];
}

- (DOMNodeList *)getElementsByName:(NSString *)elementName
{
    NameNodeList *nodeList = new NameNodeList([self _HTMLDocument], elementName);
    return [DOMNodeList _nodeListWith:nodeList];
}

@end

@implementation DOMHTMLDocument (WebPrivate)

- (DOMDocumentFragment *)_createDocumentFragmentWithMarkupString:(NSString *)markupString baseURLString:(NSString *)baseURLString
{
    RefPtr<DocumentFragment> fragment = createFragmentFromMarkup([self _document], DeprecatedString::fromNSString(markupString), DeprecatedString::fromNSString(baseURLString));
    return [DOMDocumentFragment _documentFragmentWith:fragment.get()];
}

- (DOMDocumentFragment *)_createDocumentFragmentWithText:(NSString *)text
{
    return [DOMDocumentFragment _documentFragmentWith:createFragmentFromText([self _document], DeprecatedString::fromNSString(text)).get()];
}

@end

@implementation DOMHTMLHtmlElement

- (HTMLHtmlElement *)_HTMLHtmlElement
{
    return static_cast<HTMLHtmlElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)version
{
    return [self _HTMLHtmlElement]->version();
}

- (void)setVersion:(NSString *)version
{
    [self _HTMLHtmlElement]->setVersion(version);
}

@end

@implementation DOMHTMLHeadElement

- (HTMLHeadElement *)_headElement
{
    return static_cast<HTMLHeadElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)profile
{
    return [self _headElement]->profile();
}

- (void)setProfile:(NSString *)profile
{
    [self _headElement]->setProfile(profile);
}

@end

@implementation DOMHTMLLinkElement

- (HTMLLinkElement *)_linkElement
{
    return static_cast<HTMLLinkElement *>(DOM_cast<Node *>(_internal));
}

- (BOOL)disabled
{
    return ![self _linkElement]->disabled();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _linkElement]->setDisabled(disabled);
}

- (NSString *)charset
{
    return [self _linkElement]->charset();
}

- (void)setCharset:(NSString *)charset
{
    [self _linkElement]->setCharset(charset);
}

- (NSURL *)absoluteLinkURL
{
    return [self _getURLAttribute:@"href"];
}

- (NSString *)href
{
    return [self _linkElement]->href();
}

- (void)setHref:(NSString *)href
{
    [self _linkElement]->href();
}

- (NSString *)hreflang
{
    return [self _linkElement]->hreflang();
}

- (void)setHreflang:(NSString *)hreflang
{
    [self _linkElement]->setHreflang(hreflang);
}

- (NSString *)media
{
    return [self _linkElement]->getAttribute(mediaAttr);
}

- (void)setMedia:(NSString *)media
{
    [self _linkElement]->setAttribute(mediaAttr, media);
}

- (NSString *)rel
{
    return [self _linkElement]->getAttribute(relAttr);
}

- (void)setRel:(NSString *)rel
{
    [self _linkElement]->setAttribute(relAttr, rel);
}

- (NSString *)rev
{
    return [self _linkElement]->getAttribute(revAttr);
}

- (void)setRev:(NSString *)rev
{
    [self _linkElement]->setAttribute(revAttr, rev);
}

- (NSString *)target
{
    return [self _linkElement]->getAttribute(targetAttr);
}

- (void)setTarget:(NSString *)target
{
    [self _linkElement]->setAttribute(targetAttr, target);
}

- (NSString *)type
{
    return [self _linkElement]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _linkElement]->setAttribute(typeAttr, type);
}

@end

@implementation DOMHTMLTitleElement

- (HTMLTitleElement *)_titleElement
{
    return static_cast<HTMLTitleElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)text
{
    return [self _titleElement]->getAttribute(textAttr);
}

- (void)setText:(NSString *)text
{
    [self _titleElement]->setAttribute(textAttr, text);
}

@end

@implementation DOMHTMLMetaElement

- (HTMLMetaElement *)_metaElement
{
    return static_cast<HTMLMetaElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)content
{
    return [self _metaElement]->getAttribute(contentAttr);
}

- (void)setContent:(NSString *)content
{
    [self _metaElement]->setAttribute(contentAttr, content);
}

- (NSString *)httpEquiv
{
    return [self _metaElement]->getAttribute(http_equivAttr);
}

- (void)setHttpEquiv:(NSString *)httpEquiv
{
    [self _metaElement]->setAttribute(http_equivAttr, httpEquiv);
}

- (NSString *)name
{
    return [self _metaElement]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _metaElement]->setAttribute(nameAttr, name);
}

- (NSString *)scheme
{
    return [self _metaElement]->getAttribute(schemeAttr);
}

- (void)setScheme:(NSString *)scheme
{
    [self _metaElement]->setAttribute(schemeAttr, scheme);
}

@end

@implementation DOMHTMLBaseElement

- (HTMLBaseElement *)_baseElement
{
    return static_cast<HTMLBaseElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)href
{
    return [self _baseElement]->href();
}

- (void)setHref:(NSString *)href
{
    [self _baseElement]->setAttribute(hrefAttr, href);
}

- (NSString *)target
{
    return [self _baseElement]->getAttribute(targetAttr);
}

- (void)setTarget:(NSString *)target
{
    [self _baseElement]->setAttribute(schemeAttr, target);
}

@end

@implementation DOMHTMLStyleElement

- (HTMLStyleElement *)_styleElement
{
    return static_cast<HTMLStyleElement *>(DOM_cast<Node *>(_internal));
}

- (BOOL)disabled
{
    return ![self _styleElement]->getAttribute(disabledAttr).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _styleElement]->setAttribute(disabledAttr, disabled ? "" : 0);
}

- (NSString *)media
{
    return [self _styleElement]->getAttribute(mediaAttr);
}

- (void)setMedia:(NSString *)media
{
    [self _styleElement]->setAttribute(mediaAttr, media);
}

- (NSString *)type
{
    return [self _styleElement]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _styleElement]->setAttribute(typeAttr, type);
}

@end

@implementation DOMHTMLBodyElement

- (HTMLBodyElement *)_bodyElement
{
    return static_cast<HTMLBodyElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)aLink
{
    return [self _bodyElement]->getAttribute(alinkAttr);
}

- (void)setALink:(NSString *)aLink
{
    [self _bodyElement]->setAttribute(alinkAttr, aLink);
}

- (NSString *)background
{
    return [self _bodyElement]->getAttribute(backgroundAttr);
}

- (void)setBackground:(NSString *)background
{
    [self _bodyElement]->setAttribute(backgroundAttr, background);
}

- (NSString *)bgColor
{
    return [self _bodyElement]->getAttribute(bgcolorAttr);
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _bodyElement]->setAttribute(bgcolorAttr, bgColor);
}

- (NSString *)link
{
    return [self _bodyElement]->getAttribute(linkAttr);
}

- (void)setLink:(NSString *)link
{
    [self _bodyElement]->setAttribute(linkAttr, link);
}

- (NSString *)text
{
    return [self _bodyElement]->getAttribute(textAttr);
}

- (void)setText:(NSString *)text
{
    [self _bodyElement]->setAttribute(textAttr, text);
}

- (NSString *)vLink
{
    return [self _bodyElement]->getAttribute(vlinkAttr);
}

- (void)setVLink:(NSString *)vLink
{
    [self _bodyElement]->setAttribute(vlinkAttr, vLink);
}

@end

@implementation DOMHTMLFormElement

- (HTMLFormElement *)_formElement
{
    return static_cast<HTMLFormElement *>(DOM_cast<Node *>(_internal));
}

- (DOMHTMLCollection *)elements
{
    HTMLCollection *collection = new HTMLFormCollection([self _formElement]);
    return [DOMHTMLCollection _collectionWith:collection];
}

- (int)length
{
    return [self _formElement]->length();
}

- (NSString *)name
{
    return [self _formElement]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _formElement]->setAttribute(nameAttr, name);
}

- (NSString *)acceptCharset
{
    return [self _formElement]->getAttribute(accept_charsetAttr);
}

- (void)setAcceptCharset:(NSString *)acceptCharset
{
    [self _formElement]->setAttribute(accept_charsetAttr, acceptCharset);
}

- (NSString *)action
{
    return [self _formElement]->getAttribute(actionAttr);
}

- (void)setAction:(NSString *)action
{
    [self _formElement]->setAttribute(actionAttr, action);
}

- (NSString *)enctype
{
    return [self _formElement]->getAttribute(enctypeAttr);
}

- (void)setEnctype:(NSString *)enctype
{
    [self _formElement]->setAttribute(enctypeAttr, enctype);
}

- (NSString *)method
{
    return [self _formElement]->getAttribute(methodAttr);
}

- (void)setMethod:(NSString *)method
{
    [self _formElement]->setAttribute(methodAttr, method);
}

- (NSString *)target
{
    return [self _formElement]->getAttribute(targetAttr);
}

- (void)setTarget:(NSString *)target
{
    [self _formElement]->setAttribute(targetAttr, target);
}

- (void)submit
{
    [self _formElement]->submit(false);
}

- (void)reset
{
    [self _formElement]->reset();
}

@end

@implementation DOMHTMLFormElement (WebCoreInternal)

+ (DOMHTMLFormElement *)_formElementWith:(HTMLFormElement *)impl
{
    return static_cast<DOMHTMLFormElement *>([DOMNode _nodeWith:impl]);
}

@end

@implementation DOMHTMLIsIndexElement

- (HTMLIsIndexElement *)_isIndexElement
{
    return static_cast<HTMLIsIndexElement *>(DOM_cast<Node *>(_internal));
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWith:[self _isIndexElement]->form()];
}

- (NSString *)prompt
{
    return [self _isIndexElement]->prompt();
}

- (void)setPrompt:(NSString *)prompt
{
    [self _isIndexElement]->setPrompt(prompt);
}

@end

@implementation DOMHTMLSelectElement

- (HTMLSelectElement *)_selectElement
{
    return static_cast<HTMLSelectElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)type
{
    return [self _selectElement]->type();
}

- (int)selectedIndex
{
    return [self _selectElement]->selectedIndex();
}

- (void)setSelectedIndex:(int)selectedIndex
{
    [self _selectElement]->setSelectedIndex(selectedIndex);
}

- (NSString *)value
{
    return [self _selectElement]->value();
}

- (void)setValue:(NSString *)value
{
    String s(value);
    [self _selectElement]->setValue(s.impl());
}

- (int)length
{
    return [self _selectElement]->length();
}

- (void)setLength:(int)length
{
    // FIXME: Not yet clear what to do about this one.
    // There's some JavaScript-specific hackery in the JavaScript bindings for this.
    //[self _selectElement]->setLength(length);
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWith:[self _selectElement]->form()];
}

- (DOMHTMLOptionsCollection *)options
{
    return [DOMHTMLOptionsCollection _optionsCollectionWith:[self _selectElement]->options().get()];
}

- (BOOL)disabled
{
    return ![self _selectElement]->disabled();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _selectElement]->setDisabled(disabled);
}

- (BOOL)multiple
{
    return ![self _selectElement]->multiple();
}

- (void)setMultiple:(BOOL)multiple
{
    [self _selectElement]->setMultiple(multiple);
}

- (NSString *)name
{
    return [self _selectElement]->name();
}

- (void)setName:(NSString *)name
{
    [self _selectElement]->setName(name);
}

- (int)size
{
    return [self _selectElement]->size();
}

- (void)setSize:(int)size
{
    [self _selectElement]->setSize(size);
}

- (int)tabIndex
{
    return [self _selectElement]->tabIndex();
}

- (void)setTabIndex:(int)tabIndex
{
    [self _selectElement]->setTabIndex(tabIndex);
}

- (void)add:(DOMHTMLElement *)element :(DOMHTMLElement *)before
{
    ExceptionCode ec = 0;
    [self _selectElement]->add([element _HTMLElement], [before _HTMLElement], ec);
    raiseOnDOMError(ec);
}

- (void)remove:(int)index
{
    [self _selectElement]->remove(index);
}

- (void)blur
{
    [self _selectElement]->blur();
}

- (void)focus
{
    [self _selectElement]->focus();
}

@end

@implementation DOMHTMLOptGroupElement

- (HTMLOptGroupElement *)_optGroupElement
{
    return static_cast<HTMLOptGroupElement *>(DOM_cast<Node *>(_internal));
}

- (BOOL)disabled
{
    return ![self _optGroupElement]->disabled();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _optGroupElement]->setDisabled(disabled);
}

- (NSString *)label
{
    return [self _optGroupElement]->label();
}

- (void)setLabel:(NSString *)label
{
    [self _optGroupElement]->setLabel(label);
}

@end

@implementation DOMHTMLOptionElement

- (HTMLOptionElement *)_optionElement
{
    return static_cast<HTMLOptionElement *>(DOM_cast<Node *>(_internal));
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWith:[self _optionElement]->form()];
}

- (BOOL)defaultSelected
{
    return ![self _optionElement]->defaultSelected();
}

- (void)setDefaultSelected:(BOOL)defaultSelected
{
    [self _optionElement]->setDefaultSelected(defaultSelected);
}

- (NSString *)text
{
    return [self _optionElement]->text();
}

- (int)index
{
    return [self _optionElement]->index();
}

- (BOOL)disabled
{
    return ![self _optionElement]->disabled();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _optionElement]->setDisabled(disabled);
}

- (NSString *)label
{
    return [self _optionElement]->label();
}

- (void)setLabel:(NSString *)label
{
    [self _optionElement]->setLabel(label);
}

- (BOOL)selected
{
    return [self _optionElement]->selected();
}

- (void)setSelected:(BOOL)selected
{
    [self _optionElement]->setSelected(selected);
}

- (NSString *)value
{
    return [self _optionElement]->value();
}

- (void)setValue:(NSString *)value
{
    String string = value;
    [self _optionElement]->setValue(string.impl());
}

@end

@implementation DOMHTMLInputElement

- (HTMLInputElement *)_inputElement
{
    return static_cast<HTMLInputElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)defaultValue
{
    return [self _inputElement]->defaultValue();
}

- (void)setDefaultValue:(NSString *)defaultValue
{
    [self _inputElement]->setDefaultValue(defaultValue);
}

- (BOOL)defaultChecked
{
    return [self _inputElement]->defaultChecked();
}

- (void)setDefaultChecked:(BOOL)defaultChecked
{
    [self _inputElement]->setDefaultChecked(defaultChecked);
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWith:[self _inputElement]->form()];
}

- (NSString *)accept
{
    return [self _inputElement]->accept();
}

- (void)setAccept:(NSString *)accept
{
    [self _inputElement]->setAccept(accept);
}

- (NSString *)accessKey
{
    return [self _inputElement]->accessKey();
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _inputElement]->setAccessKey(accessKey);
}

- (NSString *)align
{
    return [self _inputElement]->align();
}

- (void)setAlign:(NSString *)align
{
    [self _inputElement]->setAlign(align);
}

- (NSString *)alt
{
    return [self _inputElement]->alt();
}

- (NSString *)altDisplayString
{
    return [self _inputElement]->alt().replace('\\', [self _element]->getDocument()->backslashAsCurrencySymbol());
}

- (void)setAlt:(NSString *)alt
{
    [self _inputElement]->setAlt(alt);
}

- (BOOL)checked
{
    return [self _inputElement]->checked();
}

- (void)setChecked:(BOOL)checked
{
    return [self _inputElement]->setChecked(checked);
}

- (BOOL)disabled
{
    return [self _inputElement]->disabled();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _inputElement]->setDisabled(disabled);
}

- (int)maxLength
{
    return [self _inputElement]->maxLength();
}

- (void)setMaxLength:(int)maxLength
{
    [self _inputElement]->setMaxLength(maxLength);
}

- (NSString *)name
{
    return [self _inputElement]->name();
}

- (void)setName:(NSString *)name
{
    [self _inputElement]->setName(name);
}

- (BOOL)readOnly
{
    return [self _inputElement]->readOnly();
}

- (void)setReadOnly:(BOOL)readOnly
{
    [self _inputElement]->setReadOnly(readOnly);
}

- (unsigned)size
{
    return [self _inputElement]->size();
}

- (void)setSize:(unsigned)size
{
    [self _inputElement]->setSize(size);
}

- (NSURL *)absoluteImageURL
{
    if (![self _inputElement]->renderer() || ![self _inputElement]->renderer()->isImage())
        return nil;
    return [self _getURLAttribute:@"src"];
}

- (NSString *)src
{
    return [self _inputElement]->src();
}

- (void)setSrc:(NSString *)src
{
    [self _inputElement]->setSrc(src);
}

- (int)tabIndex
{
    return [self _inputElement]->tabIndex();
}

- (void)setTabIndex:(int)tabIndex
{
    [self _inputElement]->setTabIndex(tabIndex);
}

- (NSString *)type
{
    return [self _inputElement]->type();
}

- (void)setType:(NSString *)type
{
    [self _inputElement]->setType(type);
}

- (NSString *)useMap
{
    return [self _inputElement]->useMap();
}

- (void)setUseMap:(NSString *)useMap
{
    [self _inputElement]->setUseMap(useMap);
}

- (NSString *)value
{
    return [self _inputElement]->value();
}

- (void)setValue:(NSString *)value
{
    [self _inputElement]->setValue(value);
}

- (void)blur
{
    [self _inputElement]->blur();
}

- (void)focus
{
    [self _inputElement]->focus();
}

- (void)select
{
    [self _inputElement]->select();
}

- (void)click
{
    [self _inputElement]->click(false);
}

@end

@implementation DOMHTMLTextAreaElement

- (HTMLTextAreaElement *)_textAreaElement
{
    return static_cast<HTMLTextAreaElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)defaultValue
{
    return [self _textAreaElement]->defaultValue();
}

- (void)setDefaultValue:(NSString *)defaultValue
{
    [self _textAreaElement]->setDefaultValue(defaultValue);
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWith:[self _textAreaElement]->form()];
}

- (NSString *)accessKey
{
    return [self _textAreaElement]->getAttribute(accesskeyAttr);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _textAreaElement]->setAttribute(accesskeyAttr, accessKey);
}

- (int)cols
{
    return [self _textAreaElement]->getAttribute(colsAttr).toInt();
}

- (void)setCols:(int)cols
{
    String value(DeprecatedString::number(cols));
    [self _textAreaElement]->setAttribute(colsAttr, value);
}

- (BOOL)disabled
{
    return [self _textAreaElement]->getAttribute(disabledAttr).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _textAreaElement]->setAttribute(disabledAttr, disabled ? "" : 0);
}

- (NSString *)name
{
    return [self _textAreaElement]->name();
}

- (void)setName:(NSString *)name
{
    [self _textAreaElement]->setName(name);
}

- (BOOL)readOnly
{
    return [self _textAreaElement]->getAttribute(readonlyAttr).isNull();
}

- (void)setReadOnly:(BOOL)readOnly
{
    [self _textAreaElement]->setAttribute(readonlyAttr, readOnly ? "" : 0);
}

- (int)rows
{
    return [self _textAreaElement]->getAttribute(rowsAttr).toInt();
}

- (void)setRows:(int)rows
{
    String value(DeprecatedString::number(rows));
    [self _textAreaElement]->setAttribute(rowsAttr, value);
}

- (int)tabIndex
{
    return [self _textAreaElement]->tabIndex();
}

- (void)setTabIndex:(int)tabIndex
{
    [self _textAreaElement]->setTabIndex(tabIndex);
}

- (NSString *)type
{
    return [self _textAreaElement]->type();
}

- (void)setType:(NSString *)type
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)value
{
    return [self _textAreaElement]->value();
}

- (void)setValue:(NSString *)value
{
    [self _textAreaElement]->setValue(value);
}

- (void)blur
{
    [self _textAreaElement]->blur();
}

- (void)focus
{
    [self _textAreaElement]->focus();
}

- (void)select
{
    [self _textAreaElement]->select();
}

@end

@implementation DOMHTMLButtonElement

- (HTMLButtonElement *)_buttonElement
{
    return static_cast<HTMLButtonElement *>(DOM_cast<Node *>(_internal));
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWith:[self _buttonElement]->form()];
}

- (NSString *)accessKey
{
    return [self _buttonElement]->getAttribute(accesskeyAttr);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _buttonElement]->setAttribute(accesskeyAttr, accessKey);
}

- (BOOL)disabled
{
    return [self _buttonElement]->getAttribute(disabledAttr).isNull();
}

- (void)setDisabled:(BOOL)disabled
{
    [self _buttonElement]->setAttribute(disabledAttr, disabled ? "" : 0);
}

- (NSString *)name
{
    return [self _buttonElement]->name();
}

- (void)setName:(NSString *)name
{
    [self _buttonElement]->setName(name);
}

- (int)tabIndex
{
    return [self _buttonElement]->tabIndex();
}

- (void)setTabIndex:(int)tabIndex
{
    [self _buttonElement]->setTabIndex(tabIndex);
}

- (NSString *)type
{
    return [self _buttonElement]->type();
}

- (NSString *)value
{
    return [self _buttonElement]->getAttribute(valueAttr);
}

- (void)setValue:(NSString *)value
{
    [self _buttonElement]->setAttribute(valueAttr, value);
}

@end

@implementation DOMHTMLLabelElement

- (HTMLLabelElement *)_labelElement
{
    return static_cast<HTMLLabelElement *>(DOM_cast<Node *>(_internal));
}

- (DOMHTMLFormElement *)form
{
    Element *formElement = [self _labelElement]->formElement();
    if (!formElement)
        return 0;
    return [DOMHTMLFormElement _formElementWith:static_cast<HTMLGenericFormElement *>(formElement)->form()];
}

- (NSString *)accessKey
{
    return [self _labelElement]->getAttribute(accesskeyAttr);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _labelElement]->setAttribute(accesskeyAttr, accessKey);
}

- (NSString *)htmlFor
{
    return [self _labelElement]->getAttribute(forAttr);
}

- (void)setHtmlFor:(NSString *)htmlFor
{
    [self _labelElement]->setAttribute(forAttr, htmlFor);
}

@end

@implementation DOMHTMLFieldSetElement

- (HTMLFieldSetElement *)_fieldSetElement
{
    return static_cast<HTMLFieldSetElement *>(DOM_cast<Node *>(_internal));
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWith:[self _fieldSetElement]->form()];
}

@end

@implementation DOMHTMLLegendElement

- (HTMLLegendElement *)_legendElement
{
    return static_cast<HTMLLegendElement *>(DOM_cast<Node *>(_internal));
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWith:[self _legendElement]->form()];
}

- (NSString *)accessKey
{
    return [self _legendElement]->getAttribute(accesskeyAttr);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _legendElement]->setAttribute(accesskeyAttr, accessKey);
}

- (NSString *)align
{
    return [self _legendElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _legendElement]->setAttribute(alignAttr, align);
}

@end

@implementation DOMHTMLUListElement

- (HTMLUListElement *)_uListElement
{
    return static_cast<HTMLUListElement *>(DOM_cast<Node *>(_internal));
}

- (BOOL)compact
{
    return [self _uListElement]->getAttribute(compactAttr).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _uListElement]->setAttribute(compactAttr, compact ? "" : 0);
}

- (NSString *)type
{
    return [self _uListElement]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _uListElement]->setAttribute(typeAttr, type);
}

@end

@implementation DOMHTMLOListElement

- (HTMLOListElement *)_oListElement
{
    return static_cast<HTMLOListElement *>(DOM_cast<Node *>(_internal));
}

- (BOOL)compact
{
    return [self _oListElement]->compact();
}

- (void)setCompact:(BOOL)compact
{
    [self _oListElement]->setCompact(compact);
}

- (int)start
{
    return [self _oListElement]->getAttribute(startAttr).toInt();
}

- (void)setStart:(int)start
{
    String value(DeprecatedString::number(start));
    [self _oListElement]->setAttribute(startAttr, value);
}

- (NSString *)type
{
    return [self _oListElement]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _oListElement]->setAttribute(typeAttr, type);
}

@end

@implementation DOMHTMLDListElement

- (HTMLDListElement *)_dListElement
{
    return static_cast<HTMLDListElement *>(DOM_cast<Node *>(_internal));
}

- (BOOL)compact
{
    return [self _dListElement]->getAttribute(compactAttr).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _dListElement]->setAttribute(compactAttr, compact ? "" : 0);
}

@end

@implementation DOMHTMLDirectoryElement

- (HTMLDirectoryElement *)_directoryListElement
{
    return static_cast<HTMLDirectoryElement *>(DOM_cast<Node *>(_internal));
}

- (BOOL)compact
{
    return [self _directoryListElement]->getAttribute(compactAttr).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _directoryListElement]->setAttribute(compactAttr, compact ? "" : 0);
}

@end

@implementation DOMHTMLMenuElement

- (HTMLMenuElement *)_menuListElement
{
    return static_cast<HTMLMenuElement *>(DOM_cast<Node *>(_internal));
}

- (BOOL)compact
{
    return [self _menuListElement]->getAttribute(compactAttr).isNull();
}

- (void)setCompact:(BOOL)compact
{
    [self _menuListElement]->setAttribute(compactAttr, compact ? "" : 0);
}

@end

@implementation DOMHTMLLIElement

- (HTMLLIElement *)_liElement
{
    return static_cast<HTMLLIElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)type
{
    return [self _liElement]->type();
}

- (void)setType:(NSString *)type
{
    [self _liElement]->setType(type);
}

- (int)value
{
    return [self _liElement]->value();
}

- (void)setValue:(int)value
{
    [self _liElement]->setValue(value);
}

@end

@implementation DOMHTMLQuoteElement

- (HTMLElement *)_quoteElement
{
    return static_cast<HTMLElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)cite
{
    return [self _quoteElement]->getAttribute(citeAttr);
}

- (void)setCite:(NSString *)cite
{
    [self _quoteElement]->setAttribute(citeAttr, cite);
}

@end

@implementation DOMHTMLDivElement

- (HTMLDivElement *)_divElement
{
    return static_cast<HTMLDivElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)align
{
    return [self _divElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _divElement]->setAttribute(alignAttr, align);
}

@end

@implementation DOMHTMLParagraphElement

- (HTMLParagraphElement *)_paragraphElement
{
    return static_cast<HTMLParagraphElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)align
{
    return [self _paragraphElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _paragraphElement]->setAttribute(alignAttr, align);
}

@end

@implementation DOMHTMLHeadingElement

- (HTMLHeadingElement *)_headingElement
{
    return static_cast<HTMLHeadingElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)align
{
    return [self _headingElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _headingElement]->setAttribute(alignAttr, align);
}

@end

@implementation DOMHTMLPreElement

- (HTMLPreElement *)_preElement
{
    return static_cast<HTMLPreElement *>(DOM_cast<Node *>(_internal));
}

- (int)width
{
    return [self _preElement]->getAttribute(widthAttr).toInt();
}

- (void)setWidth:(int)width
{
    String string(DeprecatedString::number(width));
    [self _preElement]->setAttribute(widthAttr, string);
}

@end

@implementation DOMHTMLBRElement

- (HTMLBRElement *)_BRElement
{
    return static_cast<HTMLBRElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)clear
{
    return [self _BRElement]->getAttribute(clearAttr);
}

- (void)setClear:(NSString *)clear
{
    [self _BRElement]->setAttribute(clearAttr, clear);
}

@end

@implementation DOMHTMLBaseFontElement

- (HTMLBaseFontElement *)_baseFontElement
{
    return static_cast<HTMLBaseFontElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)color
{
    return [self _baseFontElement]->getAttribute(colorAttr);
}

- (void)setColor:(NSString *)color
{
    [self _baseFontElement]->setAttribute(colorAttr, color);
}

- (NSString *)face
{
    return [self _baseFontElement]->getAttribute(faceAttr);
}

- (void)setFace:(NSString *)face
{
    [self _baseFontElement]->setAttribute(faceAttr, face);
}

- (NSString *)size
{
    return [self _baseFontElement]->getAttribute(sizeAttr);
}

- (void)setSize:(NSString *)size
{
    [self _baseFontElement]->setAttribute(sizeAttr, size);
}

@end

@implementation DOMHTMLFontElement

- (HTMLFontElement *)_fontElement
{
    return static_cast<HTMLFontElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)color
{
    return [self _fontElement]->getAttribute(colorAttr);
}

- (void)setColor:(NSString *)color
{
    [self _fontElement]->setAttribute(colorAttr, color);
}

- (NSString *)face
{
    return [self _fontElement]->getAttribute(faceAttr);
}

- (void)setFace:(NSString *)face
{
    [self _fontElement]->setAttribute(faceAttr, face);
}

- (NSString *)size
{
    return [self _fontElement]->getAttribute(sizeAttr);
}

- (void)setSize:(NSString *)size
{
    [self _fontElement]->setAttribute(sizeAttr, size);
}

@end

@implementation DOMHTMLHRElement

- (HTMLHRElement *)_HRElement
{
    return static_cast<HTMLHRElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)align
{
    return [self _HRElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _HRElement]->setAttribute(alignAttr, align);
}

- (BOOL)noShade
{
    return [self _HRElement]->getAttribute(noshadeAttr).isNull();
}

- (void)setNoShade:(BOOL)noShade
{
    [self _HRElement]->setAttribute(noshadeAttr, noShade ? "" : 0);
}

- (NSString *)size
{
    return [self _HRElement]->getAttribute(sizeAttr);
}

- (void)setSize:(NSString *)size
{
    [self _HRElement]->setAttribute(sizeAttr, size);
}

- (NSString *)width
{
    return [self _HRElement]->getAttribute(widthAttr);
}

- (void)setWidth:(NSString *)width
{
    [self _HRElement]->setAttribute(widthAttr, width);
}

@end

@implementation DOMHTMLModElement

- (HTMLElement *)_modElement
{
    return static_cast<HTMLElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)cite
{
    return [self _modElement]->getAttribute(citeAttr);
}

- (void)setCite:(NSString *)cite
{
    [self _modElement]->setAttribute(citeAttr, cite);
}

- (NSString *)dateTime
{
    return [self _modElement]->getAttribute(datetimeAttr);
}

- (void)setDateTime:(NSString *)dateTime
{
    [self _modElement]->setAttribute(datetimeAttr, dateTime);
}

@end

@implementation DOMHTMLAnchorElement

- (HTMLAnchorElement *)_anchorElement
{
    return static_cast<HTMLAnchorElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)accessKey
{
    return [self _anchorElement]->getAttribute(accesskeyAttr);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _anchorElement]->setAttribute(accesskeyAttr, accessKey);
}

- (NSString *)charset
{
    return [self _anchorElement]->getAttribute(charsetAttr);
}

- (void)setCharset:(NSString *)charset
{
    [self _anchorElement]->setAttribute(charsetAttr, charset);
}

- (NSString *)coords
{
    return [self _anchorElement]->getAttribute(coordsAttr);
}

- (void)setCoords:(NSString *)coords
{
    [self _anchorElement]->setAttribute(coordsAttr, coords);
}

- (NSURL *)absoluteLinkURL
{
    return [self _getURLAttribute:@"href"];
}

- (NSString *)href
{
    return [self _anchorElement]->href();
}

- (void)setHref:(NSString *)href
{
    [self _anchorElement]->setAttribute(hrefAttr, href);
}

- (NSString *)hreflang
{
    return [self _anchorElement]->hreflang();
}

- (void)setHreflang:(NSString *)hreflang
{
    [self _anchorElement]->setHreflang(hreflang);
}

- (NSString *)name
{
    return [self _anchorElement]->name();
}

- (void)setName:(NSString *)name
{
    [self _anchorElement]->setName(name);
}

- (NSString *)rel
{
    return [self _anchorElement]->rel();
}

- (void)setRel:(NSString *)rel
{
    [self _anchorElement]->setRel(rel);
}

- (NSString *)rev
{
    return [self _anchorElement]->rev();
}

- (void)setRev:(NSString *)rev
{
    [self _anchorElement]->setRev(rev);
}

- (NSString *)shape
{
    return [self _anchorElement]->shape();
}

- (void)setShape:(NSString *)shape
{
    [self _anchorElement]->setShape(shape);
}

- (int)tabIndex
{
    return [self _anchorElement]->tabIndex();
}

- (void)setTabIndex:(int)tabIndex
{
    [self _anchorElement]->setTabIndex(tabIndex);
}

- (NSString *)target
{
    return [self _anchorElement]->getAttribute(targetAttr);
}

- (void)setTarget:(NSString *)target
{
    [self _anchorElement]->setAttribute(targetAttr, target);
}

- (NSString *)type
{
    return [self _anchorElement]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _anchorElement]->setAttribute(typeAttr, type);
}

- (void)blur
{
    HTMLAnchorElement *impl = [self _anchorElement];
    if (impl->getDocument()->focusNode() == impl)
        impl->getDocument()->setFocusNode(0);
}

- (void)focus
{
    HTMLAnchorElement *impl = [self _anchorElement];
    impl->getDocument()->setFocusNode(static_cast<Element*>(impl));
}

@end

@implementation DOMHTMLImageElement

- (HTMLImageElement *)_imageElement
{
    return static_cast<HTMLImageElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)name
{
    return [self _imageElement]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _imageElement]->setAttribute(nameAttr, name);
}

- (NSString *)align
{
    return [self _imageElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _imageElement]->setAttribute(alignAttr, align);
}

- (NSString *)alt
{
    return [self _imageElement]->getAttribute(altAttr);
}

- (NSString *)altDisplayString
{
    String alt = [self _imageElement]->getAttribute(altAttr);
    return alt.replace('\\', [self _element]->getDocument()->backslashAsCurrencySymbol());
}

- (void)setAlt:(NSString *)alt
{
    [self _imageElement]->setAttribute(altAttr, alt);
}

- (NSString *)border
{
    return [self _imageElement]->getAttribute(borderAttr);
}

- (void)setBorder:(NSString *)border
{
    [self _imageElement]->setAttribute(borderAttr, border);
}

- (int)height
{
    return [self _imageElement]->getAttribute(heightAttr).toInt();
}

- (void)setHeight:(int)height
{
    String string(DeprecatedString::number(height));
    [self _imageElement]->setAttribute(heightAttr, string);
}

- (int)hspace
{
    return [self _imageElement]->getAttribute(hspaceAttr).toInt();
}

- (void)setHspace:(int)hspace
{
    String string(DeprecatedString::number(hspace));
    [self _imageElement]->setAttribute(hspaceAttr, string);
}

- (BOOL)isMap
{
    return [self _imageElement]->getAttribute(ismapAttr).isNull();
}

- (void)setIsMap:(BOOL)isMap
{
    [self _imageElement]->setAttribute(ismapAttr, isMap ? "" : 0);
}

- (NSString *)longDesc
{
    return [self _imageElement]->getAttribute(longdescAttr);
}

- (void)setLongDesc:(NSString *)longDesc
{
    [self _imageElement]->setAttribute(longdescAttr, longDesc);
}

- (NSURL *)absoluteImageURL
{
    return [self _getURLAttribute:@"src"];
}

- (NSString *)src
{
    return [self _imageElement]->src();
}

- (void)setSrc:(NSString *)src
{
    [self _imageElement]->setAttribute(srcAttr, src);
}

- (NSString *)useMap
{
    return [self _imageElement]->getAttribute(usemapAttr);
}

- (void)setUseMap:(NSString *)useMap
{
    [self _imageElement]->setAttribute(usemapAttr, useMap);
}

- (int)vspace
{
    return [self _imageElement]->getAttribute(vspaceAttr).toInt();
}

- (void)setVspace:(int)vspace
{
    String string(DeprecatedString::number(vspace));
    [self _imageElement]->setAttribute(vspaceAttr, string);
}

- (int)width
{
    return [self _imageElement]->getAttribute(widthAttr).toInt();
}

- (void)setWidth:(int)width
{
    String string(DeprecatedString::number(width));
    [self _imageElement]->setAttribute(widthAttr, string);
}

@end

@implementation DOMHTMLObjectElement

- (HTMLObjectElement *)_objectElement
{
    return static_cast<HTMLObjectElement *>(DOM_cast<Node *>(_internal));
}

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _formElementWith:[self _objectElement]->form()];
}

- (NSString *)code
{
    return [self _objectElement]->getAttribute(codeAttr);
}

- (void)setCode:(NSString *)code
{
    [self _objectElement]->setAttribute(codeAttr, code);
}

- (NSString *)align
{
    return [self _objectElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _objectElement]->setAttribute(alignAttr, align);
}

- (NSString *)archive
{
    return [self _objectElement]->getAttribute(archiveAttr);
}

- (void)setArchive:(NSString *)archive
{
    [self _objectElement]->setAttribute(archiveAttr, archive);
}

- (NSString *)border
{
    return [self _objectElement]->getAttribute(borderAttr);
}

- (void)setBorder:(NSString *)border
{
    [self _objectElement]->setAttribute(borderAttr, border);
}

- (NSString *)codeBase
{
    return [self _objectElement]->getAttribute(codebaseAttr);
}

- (void)setCodeBase:(NSString *)codeBase
{
    [self _objectElement]->setAttribute(codebaseAttr, codeBase);
}

- (NSString *)codeType
{
    return [self _objectElement]->getAttribute(codetypeAttr);
}

- (void)setCodeType:(NSString *)codeType
{
    [self _objectElement]->setAttribute(codetypeAttr, codeType);
}

- (NSURL *)absoluteImageURL
{
    if (![self _objectElement]->renderer() || ![self _objectElement]->renderer()->isImage())
        return nil;
    return [self _getURLAttribute:@"data"];
}

- (NSString *)data
{
    return [self _objectElement]->getAttribute(dataAttr);
}

- (void)setData:(NSString *)data
{
    [self _objectElement]->setAttribute(dataAttr, data);
}

- (BOOL)declare
{
    return [self _objectElement]->getAttribute(declareAttr).isNull();
}

- (void)setDeclare:(BOOL)declare
{
    [self _objectElement]->setAttribute(declareAttr, declare ? "" : 0);
}

- (NSString *)height
{
    return [self _objectElement]->getAttribute(heightAttr);
}

- (void)setHeight:(NSString *)height
{
    [self _objectElement]->setAttribute(heightAttr, height);
}

- (int)hspace
{
    return [self _objectElement]->getAttribute(hspaceAttr).toInt();
}

- (void)setHspace:(int)hspace
{
    String string(DeprecatedString::number(hspace));
    [self _objectElement]->setAttribute(hspaceAttr, string);
}

- (NSString *)name
{
    return [self _objectElement]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _objectElement]->setAttribute(nameAttr, name);
}

- (NSString *)standby
{
    return [self _objectElement]->getAttribute(standbyAttr);
}

- (void)setStandby:(NSString *)standby
{
    [self _objectElement]->setAttribute(standbyAttr, standby);
}

- (int)tabIndex
{
    return [self _objectElement]->getAttribute(tabindexAttr).toInt();
}

- (void)setTabIndex:(int)tabIndex
{
    String string(DeprecatedString::number(tabIndex));
    [self _objectElement]->setAttribute(tabindexAttr, string);
}

- (NSString *)type
{
    return [self _objectElement]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _objectElement]->setAttribute(typeAttr, type);
}

- (NSString *)useMap
{
    return [self _objectElement]->getAttribute(usemapAttr);
}

- (void)setUseMap:(NSString *)useMap
{
    [self _objectElement]->setAttribute(usemapAttr, useMap);
}

- (int)vspace
{
    return [self _objectElement]->getAttribute(vspaceAttr).toInt();
}

- (void)setVspace:(int)vspace
{
    String string(DeprecatedString::number(vspace));
    [self _objectElement]->setAttribute(vspaceAttr, string);
}

- (NSString *)width
{
    return [self _objectElement]->getAttribute(widthAttr);
}

- (void)setWidth:(NSString *)width
{
    [self _objectElement]->setAttribute(widthAttr, width);
}

- (DOMDocument *)contentDocument
{
    return [DOMDocument _documentWith:[self _objectElement]->contentDocument()];
}

@end

@implementation DOMHTMLParamElement

- (HTMLParamElement *)_paramElement
{
    return static_cast<HTMLParamElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)name
{
    return [self _paramElement]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _paramElement]->setAttribute(nameAttr, name);
}

- (NSString *)type
{
    return [self _paramElement]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _paramElement]->setAttribute(typeAttr, type);
}

- (NSString *)value
{
    return [self _paramElement]->getAttribute(valueAttr);
}

- (void)setValue:(NSString *)value
{
    [self _paramElement]->setAttribute(valueAttr, value);
}

- (NSString *)valueType
{
    return [self _paramElement]->getAttribute(valuetypeAttr);
}

- (void)setValueType:(NSString *)valueType
{
    [self _paramElement]->setAttribute(valuetypeAttr, valueType);
}

@end

@implementation DOMHTMLAppletElement

- (HTMLAppletElement *)_appletElement
{
    return static_cast<HTMLAppletElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)align
{
    return [self _appletElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _appletElement]->setAttribute(alignAttr, align);
}

- (NSString *)alt
{
    return [self _appletElement]->getAttribute(altAttr);
}

- (NSString *)altDisplayString
{
    String alt = [self _appletElement]->getAttribute(altAttr);
    return alt.replace('\\', [self _element]->getDocument()->backslashAsCurrencySymbol());
}

- (void)setAlt:(NSString *)alt
{
    [self _appletElement]->setAttribute(altAttr, alt);
}

- (NSString *)archive
{
    return [self _appletElement]->getAttribute(archiveAttr);
}

- (void)setArchive:(NSString *)archive
{
    [self _appletElement]->setAttribute(archiveAttr, archive);
}

- (NSString *)code
{
    return [self _appletElement]->getAttribute(codeAttr);
}

- (void)setCode:(NSString *)code
{
    [self _appletElement]->setAttribute(codeAttr, code);
}

- (NSString *)codeBase
{
    return [self _appletElement]->getAttribute(codebaseAttr);
}

- (void)setCodeBase:(NSString *)codeBase
{
    [self _appletElement]->setAttribute(codebaseAttr, codeBase);
}

- (NSString *)height
{
    return [self _appletElement]->getAttribute(heightAttr);
}

- (void)setHeight:(NSString *)height
{
    [self _appletElement]->setAttribute(heightAttr, height);
}

- (int)hspace
{
    return [self _appletElement]->getAttribute(hspaceAttr).toInt();
}

- (void)setHspace:(int)hspace
{
    String string(DeprecatedString::number(hspace));
    [self _appletElement]->setAttribute(hspaceAttr, string);
}

- (NSString *)name
{
    return [self _appletElement]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _appletElement]->setAttribute(nameAttr, name);
}

- (NSString *)object
{
    return [self _appletElement]->getAttribute(objectAttr);
}

- (void)setObject:(NSString *)object
{
    [self _appletElement]->setAttribute(objectAttr, object);
}

- (int)vspace
{
    return [self _appletElement]->getAttribute(vspaceAttr).toInt();
}

- (void)setVspace:(int)vspace
{
    String string(DeprecatedString::number(vspace));
    [self _appletElement]->setAttribute(vspaceAttr, string);
}

- (NSString *)width
{
    return [self _appletElement]->getAttribute(widthAttr);
}

- (void)setWidth:(NSString *)width
{
    [self _appletElement]->setAttribute(widthAttr, width);
}

@end

@implementation DOMHTMLMapElement

- (HTMLMapElement *)_mapElement
{
    return static_cast<HTMLMapElement *>(DOM_cast<Node *>(_internal));
}

- (DOMHTMLCollection *)areas
{
    HTMLCollection *collection = new HTMLCollection([self _mapElement], HTMLCollection::MAP_AREAS);
    return [DOMHTMLCollection _collectionWith:collection];
}

- (NSString *)name
{
    return [self _mapElement]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _mapElement]->setAttribute(nameAttr, name);
}

@end

@implementation DOMHTMLAreaElement

- (HTMLAreaElement *)_areaElement
{
    return static_cast<HTMLAreaElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)accessKey
{
    return [self _areaElement]->getAttribute(accesskeyAttr);
}

- (void)setAccessKey:(NSString *)accessKey
{
    [self _areaElement]->setAttribute(accesskeyAttr, accessKey);
}

- (NSString *)alt
{
    return [self _areaElement]->getAttribute(altAttr);
}

- (NSString *)altDisplayString
{
    String alt = [self _areaElement]->getAttribute(altAttr);
    return alt.replace('\\', [self _element]->getDocument()->backslashAsCurrencySymbol());
}

- (void)setAlt:(NSString *)alt
{
    [self _areaElement]->setAttribute(altAttr, alt);
}

- (NSString *)coords
{
    return [self _areaElement]->getAttribute(coordsAttr);
}

- (void)setCoords:(NSString *)coords
{
    [self _areaElement]->setAttribute(coordsAttr, coords);
}

- (NSURL *)absoluteLinkURL
{
    return [self _getURLAttribute:@"href"];
}

- (NSString *)href
{
    return [self _areaElement]->href();
}

- (void)setHref:(NSString *)href
{
    [self _areaElement]->setAttribute(hrefAttr, href);
}

- (BOOL)noHref
{
    return [self _areaElement]->getAttribute(nohrefAttr).isNull();
}

- (void)setNoHref:(BOOL)noHref
{
    [self _areaElement]->setAttribute(nohrefAttr, noHref ? "" : 0);
}

- (NSString *)shape
{
    return [self _areaElement]->getAttribute(shapeAttr);
}

- (void)setShape:(NSString *)shape
{
    [self _areaElement]->setAttribute(shapeAttr, shape);
}

- (int)tabIndex
{
    return [self _areaElement]->getAttribute(tabindexAttr).toInt();
}

- (void)setTabIndex:(int)tabIndex
{
    String string(DeprecatedString::number(tabIndex));
    [self _areaElement]->setAttribute(tabindexAttr, string);
}

- (NSString *)target
{
    return [self _areaElement]->getAttribute(targetAttr);
}

- (void)setTarget:(NSString *)target
{
    [self _areaElement]->setAttribute(targetAttr, target);
}

@end

@implementation DOMHTMLScriptElement

- (HTMLScriptElement *)_scriptElement
{
    return static_cast<HTMLScriptElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)text
{
    return [self _scriptElement]->getAttribute(textAttr);
}

- (void)setText:(NSString *)text
{
    [self _scriptElement]->setAttribute(textAttr, text);
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
    return [self _scriptElement]->getAttribute(charsetAttr);
}

- (void)setCharset:(NSString *)charset
{
    [self _scriptElement]->setAttribute(charsetAttr, charset);
}

- (BOOL)defer
{
    return [self _scriptElement]->getAttribute(deferAttr).isNull();
}

- (void)setDefer:(BOOL)defer
{
    [self _scriptElement]->setAttribute(deferAttr, defer ? "" : 0);
}

- (NSString *)src
{
    return [self _scriptElement]->getAttribute(srcAttr);
}

- (void)setSrc:(NSString *)src
{
    [self _scriptElement]->setAttribute(srcAttr, src);
}

- (NSString *)type
{
    return [self _scriptElement]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _scriptElement]->setAttribute(typeAttr, type);
}

@end

@implementation DOMHTMLTableCaptionElement

- (NSString *)align
{
    return [self _tableCaptionElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _tableCaptionElement]->setAttribute(alignAttr, align);
}

@end

@implementation DOMHTMLTableCaptionElement (WebCoreInternal)

+ (DOMHTMLTableCaptionElement *)_tableCaptionElementWith:(HTMLTableCaptionElement *)impl
{
    return static_cast<DOMHTMLTableCaptionElement *>([DOMNode _nodeWith:impl]);
}

- (HTMLTableCaptionElement *)_tableCaptionElement
{
    return static_cast<HTMLTableCaptionElement *>(DOM_cast<Node *>(_internal));
}

@end

@implementation DOMHTMLTableSectionElement

- (NSString *)align
{
    return [self _tableSectionElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _tableSectionElement]->setAttribute(alignAttr, align);
}

- (NSString *)ch
{
    return [self _tableSectionElement]->getAttribute(charoffAttr);
}

- (void)setCh:(NSString *)ch
{
    [self _tableSectionElement]->setAttribute(charoffAttr, ch);
}

- (NSString *)chOff
{
    return [self _tableSectionElement]->getAttribute(charoffAttr);
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableSectionElement]->setAttribute(charoffAttr, chOff);
}

- (NSString *)vAlign
{
    return [self _tableSectionElement]->getAttribute(valignAttr);
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableSectionElement]->setAttribute(valignAttr, vAlign);
}

- (DOMHTMLCollection *)rows
{
    HTMLCollection *collection = new HTMLCollection([self _tableSectionElement], HTMLCollection::TABLE_ROWS);
    return [DOMHTMLCollection _collectionWith:collection];
}

- (DOMHTMLElement *)insertRow:(int)index
{
    ExceptionCode ec = 0;
    HTMLElement *impl = [self _tableSectionElement]->insertRow(index, ec);
    raiseOnDOMError(ec);
    return [DOMHTMLElement _elementWith:impl];
}

- (void)deleteRow:(int)index
{
    ExceptionCode ec = 0;
    [self _tableSectionElement]->deleteRow(index, ec);
    raiseOnDOMError(ec);
}

@end

@implementation DOMHTMLTableSectionElement (WebCoreInternal)

+ (DOMHTMLTableSectionElement *)_tableSectionElementWith:(HTMLTableSectionElement *)impl
{
    return static_cast<DOMHTMLTableSectionElement *>([DOMNode _nodeWith:impl]);
}

- (HTMLTableSectionElement *)_tableSectionElement
{
    return static_cast<HTMLTableSectionElement *>(DOM_cast<Node *>(_internal));
}

@end

@implementation DOMHTMLTableElement

- (DOMHTMLTableCaptionElement *)caption
{
    return [DOMHTMLTableCaptionElement _tableCaptionElementWith:[self _tableElement]->caption()];
}

- (void)setCaption:(DOMHTMLTableCaptionElement *)caption
{
    [self _tableElement]->setCaption([caption _tableCaptionElement]);
}

- (DOMHTMLTableSectionElement *)tHead
{
    return [DOMHTMLTableSectionElement _tableSectionElementWith:[self _tableElement]->tHead()];
}

- (void)setTHead:(DOMHTMLTableSectionElement *)tHead
{
    [self _tableElement]->setTHead([tHead _tableSectionElement]);
}

- (DOMHTMLTableSectionElement *)tFoot
{
    return [DOMHTMLTableSectionElement _tableSectionElementWith:[self _tableElement]->tFoot()];
}

- (void)setTFoot:(DOMHTMLTableSectionElement *)tFoot
{
    [self _tableElement]->setTFoot([tFoot _tableSectionElement]);
}

- (DOMHTMLCollection *)rows
{
    HTMLCollection *collection = new HTMLCollection([self _tableElement], HTMLCollection::TABLE_ROWS);
    return [DOMHTMLCollection _collectionWith:collection];
}

- (DOMHTMLCollection *)tBodies
{
    HTMLCollection *collection = new HTMLCollection([self _tableElement], HTMLCollection::TABLE_TBODIES);
    return [DOMHTMLCollection _collectionWith:collection];
}

- (NSString *)align
{
    return [self _tableElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _tableElement]->setAttribute(alignAttr, align);
}

- (NSString *)bgColor
{
    return [self _tableElement]->getAttribute(bgcolorAttr);
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _tableElement]->setAttribute(bgcolorAttr, bgColor);
}

- (NSString *)border
{
    return [self _tableElement]->getAttribute(borderAttr);
}

- (void)setBorder:(NSString *)border
{
    [self _tableElement]->setAttribute(borderAttr, border);
}

- (NSString *)cellPadding
{
    return [self _tableElement]->getAttribute(cellpaddingAttr);
}

- (void)setCellPadding:(NSString *)cellPadding
{
    [self _tableElement]->setAttribute(cellpaddingAttr, cellPadding);
}

- (NSString *)cellSpacing
{
    return [self _tableElement]->getAttribute(cellspacingAttr);
}

- (void)setCellSpacing:(NSString *)cellSpacing
{
    [self _tableElement]->setAttribute(cellspacingAttr, cellSpacing);
}

- (NSString *)frameBorders
{
    return [self _tableElement]->getAttribute(frameAttr);
}

- (void)setFrameBorders:(NSString *)frameBorders
{
    [self _tableElement]->setAttribute(frameAttr, frameBorders);
}

- (NSString *)rules
{
    return [self _tableElement]->getAttribute(rulesAttr);
}

- (void)setRules:(NSString *)rules
{
    [self _tableElement]->setAttribute(rulesAttr, rules);
}

- (NSString *)summary
{
    return [self _tableElement]->getAttribute(summaryAttr);
}

- (void)setSummary:(NSString *)summary
{
    [self _tableElement]->setAttribute(summaryAttr, summary);
}

- (NSString *)width
{
    return [self _tableElement]->getAttribute(widthAttr);
}

- (void)setWidth:(NSString *)width
{
    [self _tableElement]->setAttribute(widthAttr, width);
}

- (DOMHTMLElement *)createTHead
{
    HTMLTableSectionElement *impl = static_cast<HTMLTableSectionElement *>([self _tableElement]->createTHead());
    return [DOMHTMLTableSectionElement _tableSectionElementWith:impl];
}

- (void)deleteTHead
{
    [self _tableElement]->deleteTHead();
}

- (DOMHTMLElement *)createTFoot
{
    HTMLTableSectionElement *impl = static_cast<HTMLTableSectionElement *>([self _tableElement]->createTFoot());
    return [DOMHTMLTableSectionElement _tableSectionElementWith:impl];
}

- (void)deleteTFoot
{
    [self _tableElement]->deleteTFoot();
}

- (DOMHTMLElement *)createCaption
{
    HTMLTableCaptionElement *impl = static_cast<HTMLTableCaptionElement *>([self _tableElement]->createCaption());
    return [DOMHTMLTableCaptionElement _tableCaptionElementWith:impl];
}

- (void)deleteCaption
{
    [self _tableElement]->deleteCaption();
}

- (DOMHTMLElement *)insertRow:(int)index
{
    ExceptionCode ec = 0;
    HTMLTableElement *impl = static_cast<HTMLTableElement *>([self _tableElement]->insertRow(index, ec));
    raiseOnDOMError(ec);
    return [DOMHTMLTableElement _tableElementWith:impl];
}

- (void)deleteRow:(int)index
{
    ExceptionCode ec = 0;
    [self _tableElement]->deleteRow(index, ec);
    raiseOnDOMError(ec);
}

@end

@implementation DOMHTMLTableElement (WebCoreInternal)

+ (DOMHTMLTableElement *)_tableElementWith:(HTMLTableElement *)impl
{
    return static_cast<DOMHTMLTableElement *>([DOMNode _nodeWith:impl]);
}

- (HTMLTableElement *)_tableElement
{
    return static_cast<HTMLTableElement *>(DOM_cast<Node *>(_internal));
}

@end

@implementation DOMHTMLTableColElement

- (HTMLTableColElement *)_tableColElement
{
    return static_cast<HTMLTableColElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)align
{
    return [self _tableColElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _tableColElement]->setAttribute(alignAttr, align);
}

- (NSString *)ch
{
    return [self _tableColElement]->getAttribute(charoffAttr);
}

- (void)setCh:(NSString *)ch
{
    [self _tableColElement]->setAttribute(charoffAttr, ch);
}

- (NSString *)chOff
{
    return [self _tableColElement]->getAttribute(charoffAttr);
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableColElement]->setAttribute(charoffAttr, chOff);
}

- (int)span
{
    return [self _tableColElement]->getAttribute(spanAttr).toInt();
}

- (void)setSpan:(int)span
{
    String string(DeprecatedString::number(span));
    [self _tableColElement]->setAttribute(spanAttr, string);
}

- (NSString *)vAlign
{
    return [self _tableColElement]->getAttribute(valignAttr);
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableColElement]->setAttribute(valignAttr, vAlign);
}

- (NSString *)width
{
    return [self _tableColElement]->getAttribute(widthAttr);
}

- (void)setWidth:(NSString *)width
{
    [self _tableColElement]->setAttribute(widthAttr, width);
}

@end

@implementation DOMHTMLTableRowElement

- (HTMLTableRowElement *)_tableRowElement
{
    return static_cast<HTMLTableRowElement *>(DOM_cast<Node *>(_internal));
}

- (int)rowIndex
{
    return [self _tableRowElement]->rowIndex();
}

- (int)sectionRowIndex
{
    return [self _tableRowElement]->sectionRowIndex();
}

- (DOMHTMLCollection *)cells
{
    HTMLCollection *collection = new HTMLCollection([self _tableRowElement], HTMLCollection::TR_CELLS);
    return [DOMHTMLCollection _collectionWith:collection];
}

- (NSString *)align
{
    return [self _tableRowElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _tableRowElement]->setAttribute(alignAttr, align);
}

- (NSString *)bgColor
{
    return [self _tableRowElement]->getAttribute(bgcolorAttr);
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _tableRowElement]->setAttribute(bgcolorAttr, bgColor);
}

- (NSString *)ch
{
    return [self _tableRowElement]->getAttribute(charoffAttr);
}

- (void)setCh:(NSString *)ch
{
    [self _tableRowElement]->setAttribute(charoffAttr, ch);
}

- (NSString *)chOff
{
    return [self _tableRowElement]->getAttribute(charoffAttr);
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableRowElement]->setAttribute(charoffAttr, chOff);
}

- (NSString *)vAlign
{
    return [self _tableRowElement]->getAttribute(valignAttr);
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableRowElement]->setAttribute(valignAttr, vAlign);
}

- (DOMHTMLElement *)insertCell:(int)index
{
    ExceptionCode ec = 0;
    HTMLTableCellElement *impl = static_cast<HTMLTableCellElement *>([self _tableRowElement]->insertCell(index, ec));
    raiseOnDOMError(ec);
    return [DOMHTMLTableCellElement _tableCellElementWith:impl];
}

- (void)deleteCell:(int)index
{
    ExceptionCode ec = 0;
    [self _tableRowElement]->deleteCell(index, ec);
    raiseOnDOMError(ec);
}

@end

@implementation DOMHTMLTableCellElement

- (int)cellIndex
{
    return [self _tableCellElement]->cellIndex();
}

- (NSString *)abbr
{
    return [self _tableCellElement]->getAttribute(abbrAttr);
}

- (void)setAbbr:(NSString *)abbr
{
    [self _tableCellElement]->setAttribute(abbrAttr, abbr);
}

- (NSString *)align
{
    return [self _tableCellElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _tableCellElement]->setAttribute(alignAttr, align);
}

- (NSString *)axis
{
    return [self _tableCellElement]->getAttribute(axisAttr);
}

- (void)setAxis:(NSString *)axis
{
    [self _tableCellElement]->setAttribute(axisAttr, axis);
}

- (NSString *)bgColor
{
    return [self _tableCellElement]->getAttribute(bgcolorAttr);
}

- (void)setBgColor:(NSString *)bgColor
{
    [self _tableCellElement]->setAttribute(bgcolorAttr, bgColor);
}

- (NSString *)ch
{
    return [self _tableCellElement]->getAttribute(charoffAttr);
}

- (void)setCh:(NSString *)ch
{
    [self _tableCellElement]->setAttribute(charoffAttr, ch);
}

- (NSString *)chOff
{
    return [self _tableCellElement]->getAttribute(charoffAttr);
}

- (void)setChOff:(NSString *)chOff
{
    [self _tableCellElement]->setAttribute(charoffAttr, chOff);
}

- (int)colSpan
{
    return [self _tableCellElement]->getAttribute(colspanAttr).toInt();
}

- (void)setColSpan:(int)colSpan
{
    String string(DeprecatedString::number(colSpan));
    [self _tableCellElement]->setAttribute(colspanAttr, string);
}

- (NSString *)headers
{
    return [self _tableCellElement]->getAttribute(headersAttr);
}

- (void)setHeaders:(NSString *)headers
{
    [self _tableCellElement]->setAttribute(headersAttr, headers);
}

- (NSString *)height
{
    return [self _tableCellElement]->getAttribute(heightAttr);
}

- (void)setHeight:(NSString *)height
{
    [self _tableCellElement]->setAttribute(heightAttr, height);
}

- (BOOL)noWrap
{
    return [self _tableCellElement]->getAttribute(nowrapAttr).isNull();
}

- (void)setNoWrap:(BOOL)noWrap
{
    [self _tableCellElement]->setAttribute(nowrapAttr, noWrap ? "" : 0);
}

- (int)rowSpan
{
    return [self _tableCellElement]->getAttribute(rowspanAttr).toInt();
}

- (void)setRowSpan:(int)rowSpan
{
    String string(DeprecatedString::number(rowSpan));
    [self _tableCellElement]->setAttribute(rowspanAttr, string);
}

- (NSString *)scope
{
    return [self _tableCellElement]->getAttribute(scopeAttr);
}

- (void)setScope:(NSString *)scope
{
    [self _tableCellElement]->setAttribute(scopeAttr, scope);
}

- (NSString *)vAlign
{
    return [self _tableCellElement]->getAttribute(valignAttr);
}

- (void)setVAlign:(NSString *)vAlign
{
    [self _tableCellElement]->setAttribute(valignAttr, vAlign);
}

- (NSString *)width
{
    return [self _tableCellElement]->getAttribute(widthAttr);
}

- (void)setWidth:(NSString *)width
{
    [self _tableCellElement]->setAttribute(widthAttr, width);
}

@end

@implementation DOMHTMLTableCellElement (WebCoreInternal)

+ (DOMHTMLTableCellElement *)_tableCellElementWith:(HTMLTableCellElement *)impl
{
    return static_cast<DOMHTMLTableCellElement *>([DOMNode _nodeWith:impl]);
}

- (HTMLTableCellElement *)_tableCellElement
{
    return static_cast<HTMLTableCellElement *>(DOM_cast<Node *>(_internal));
}

@end

@implementation DOMHTMLFrameSetElement

- (HTMLFrameSetElement *)_frameSetElement
{
    return static_cast<HTMLFrameSetElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)rows
{
    return [self _frameSetElement]->getAttribute(rowsAttr);
}

- (void)setRows:(NSString *)rows
{
    [self _frameSetElement]->setAttribute(rowsAttr, rows);
}

- (NSString *)cols
{
    return [self _frameSetElement]->getAttribute(colsAttr);
}

- (void)setCols:(NSString *)cols
{
    [self _frameSetElement]->setAttribute(colsAttr, cols);
}

@end

@implementation DOMHTMLFrameElement

- (HTMLFrameElement *)_frameElement
{
    return static_cast<HTMLFrameElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)frameBorder
{
    return [self _frameElement]->getAttribute(frameborderAttr);
}

- (void)setFrameBorder:(NSString *)frameBorder
{
    [self _frameElement]->setAttribute(frameborderAttr, frameBorder);
}

- (NSString *)longDesc
{
    return [self _frameElement]->getAttribute(longdescAttr);
}

- (void)setLongDesc:(NSString *)longDesc
{
    [self _frameElement]->setAttribute(longdescAttr, longDesc);
}

- (NSString *)marginHeight
{
    return [self _frameElement]->getAttribute(marginheightAttr);
}

- (void)setMarginHeight:(NSString *)marginHeight
{
    [self _frameElement]->setAttribute(marginheightAttr, marginHeight);
}

- (NSString *)marginWidth
{
    return [self _frameElement]->getAttribute(marginwidthAttr);
}

- (void)setMarginWidth:(NSString *)marginWidth
{
    [self _frameElement]->setAttribute(marginwidthAttr, marginWidth);
}

- (NSString *)name
{
    return [self _frameElement]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _frameElement]->setAttribute(nameAttr, name);
}

- (BOOL)noResize
{
    return [self _frameElement]->getAttribute(noresizeAttr).isNull();
}

- (void)setNoResize:(BOOL)noResize
{
    [self _frameElement]->setAttribute(noresizeAttr, noResize ? "" : 0);
}

- (NSString *)scrolling
{
    return [self _frameElement]->getAttribute(scrollingAttr);
}

- (void)setScrolling:(NSString *)scrolling
{
    [self _frameElement]->setAttribute(scrollingAttr, scrolling);
}

- (NSString *)src
{
    return [self _frameElement]->getAttribute(srcAttr);
}

- (void)setSrc:(NSString *)src
{
    [self _frameElement]->setAttribute(srcAttr, src);
}

- (DOMDocument *)contentDocument
{
    return [DOMDocument _documentWith:[self _frameElement]->contentDocument()];
}

@end

@implementation DOMHTMLIFrameElement

- (HTMLIFrameElement *)_IFrameElement
{
    return static_cast<HTMLIFrameElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)align
{
    return [self _IFrameElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _IFrameElement]->setAttribute(alignAttr, align);
}

- (NSString *)frameBorder
{
    return [self _IFrameElement]->getAttribute(frameborderAttr);
}

- (void)setFrameBorder:(NSString *)frameBorder
{
    [self _IFrameElement]->setAttribute(frameborderAttr, frameBorder);
}

- (NSString *)height
{
    return [self _IFrameElement]->getAttribute(heightAttr);
}

- (void)setHeight:(NSString *)height
{
    [self _IFrameElement]->setAttribute(heightAttr, height);
}

- (NSString *)longDesc
{
    return [self _IFrameElement]->getAttribute(longdescAttr);
}

- (void)setLongDesc:(NSString *)longDesc
{
    [self _IFrameElement]->setAttribute(longdescAttr, longDesc);
}

- (NSString *)marginHeight
{
    return [self _IFrameElement]->getAttribute(marginheightAttr);
}

- (void)setMarginHeight:(NSString *)marginHeight
{
    [self _IFrameElement]->setAttribute(marginheightAttr, marginHeight);
}

- (NSString *)marginWidth
{
    return [self _IFrameElement]->getAttribute(marginwidthAttr);
}

- (void)setMarginWidth:(NSString *)marginWidth
{
    [self _IFrameElement]->setAttribute(marginwidthAttr, marginWidth);
}

- (NSString *)name
{
    return [self _IFrameElement]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _IFrameElement]->setAttribute(nameAttr, name);
}

- (BOOL)noResize
{
    return [self _IFrameElement]->getAttribute(noresizeAttr).isNull();
}

- (void)setNoResize:(BOOL)noResize
{
    [self _IFrameElement]->setAttribute(noresizeAttr, noResize ? "" : 0);
}

- (NSString *)scrolling
{
    return [self _IFrameElement]->getAttribute(scrollingAttr);
}

- (void)setScrolling:(NSString *)scrolling
{
    [self _IFrameElement]->setAttribute(scrollingAttr, scrolling);
}

- (NSString *)src
{
    return [self _IFrameElement]->getAttribute(srcAttr);
}

- (void)setSrc:(NSString *)src
{
    [self _IFrameElement]->setAttribute(srcAttr, src);
}

- (NSString *)width
{
    return [self _IFrameElement]->getAttribute(widthAttr);
}

- (void)setWidth:(NSString *)width
{
    [self _IFrameElement]->setAttribute(widthAttr, width);
}

- (DOMDocument *)contentDocument
{
    return [DOMDocument _documentWith:[self _IFrameElement]->contentDocument()];
}

@end

#pragma mark DOM EXTENSIONS

@implementation DOMHTMLEmbedElement

- (HTMLEmbedElement *)_embedElement
{
    return static_cast<HTMLEmbedElement *>(DOM_cast<Node *>(_internal));
}

- (NSString *)align
{
    return [self _embedElement]->getAttribute(alignAttr);
}

- (void)setAlign:(NSString *)align
{
    [self _embedElement]->setAttribute(alignAttr, align);
}

- (int)height
{
    return [self _embedElement]->getAttribute(heightAttr).toInt();
}

- (void)setHeight:(int)height
{
    String string(DeprecatedString::number(height));
    [self _embedElement]->setAttribute(heightAttr, string);
}

- (NSString *)name
{
    return [self _embedElement]->getAttribute(nameAttr);
}

- (void)setName:(NSString *)name
{
    [self _embedElement]->setAttribute(nameAttr, name);
}

- (NSString *)src
{
    return [self _embedElement]->getAttribute(srcAttr);
}

- (void)setSrc:(NSString *)src
{
    [self _embedElement]->setAttribute(srcAttr, src);
}

- (NSString *)type
{
    return [self _embedElement]->getAttribute(typeAttr);
}

- (void)setType:(NSString *)type
{
    [self _embedElement]->setAttribute(typeAttr, type);
}

- (int)width
{
    return [self _embedElement]->getAttribute(widthAttr).toInt();
}

- (void)setWidth:(int)width
{
    String string(DeprecatedString::number(width));
    [self _embedElement]->setAttribute(widthAttr, string);
}

@end

// These #imports and "usings" are used only by viewForElement and should be deleted 
// when that function goes away.
#import "RenderObject.h"
#import "render_replaced.h"
using WebCore::RenderObject;
using WebCore::RenderWidget;

// This function is used only by the two FormAutoFillTransition categories, and will go away
// as soon as possible.
static NSView *viewForElement(DOMElement *element)
{
    RenderObject *renderer = [element _element]->renderer();
    if (renderer && renderer->isWidget()) {
        Widget *widget = static_cast<const RenderWidget *>(renderer)->widget();
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

@end
