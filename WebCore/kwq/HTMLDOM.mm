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

#import <WebCore/HTMLDOM.h>
#import <WebCore/KWQAssertions.h>

@implementation HTMLCollection

- (unsigned long)length
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (DOMNode *)item:(unsigned long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (DOMNode *)namedItem:(NSString *)name
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

@end

@implementation HTMLElement

- (NSString *)idName
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setIdName:(NSString *)idName
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)title
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setTitle:(NSString *)title
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)lang
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setLang:(NSString *)lang
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)dir
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setDir:(NSString *)dir
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)className
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setClassName:(NSString *)className
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation HTMLDocument

- (NSString *)title
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setTitle:(NSString *)title
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)referrer
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (NSString *)domain
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (NSString *)URL
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (HTMLElement *)body
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (HTMLCollection *)images
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (HTMLCollection *)applets
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (HTMLCollection *)links
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (HTMLCollection *)forms
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (HTMLCollection *)anchors
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setBody:(HTMLElement *)body
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)cookie
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCookie:(NSString *)cookie
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)open
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)close
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)write:(NSString *)text
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (void)writeln:(NSString *)text
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (DOMElement *)getElementById:(NSString *)elementId
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (DOMNodeList *)getElementsByName:(NSString *)elementName
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

@end

@implementation HTMLHtmlElement

- (NSString *)version
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setVersion:(NSString *)version
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation HTMLHeadElement

- (NSString *)profile
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setProfile:(NSString *)profile
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation HTMLLinkElement

- (BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");   
    return NO;
}

- (void)setDisabled:(BOOL)disabled
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

- (NSString *)media
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setMedia:(NSString *)media
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)rel
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setRrevel:(NSString *)rel
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

@end

@implementation HTMLTitleElement

- (NSString *)text
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setText:(NSString *)text
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation HTMLMetaElement

- (NSString *)content
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setContent:(NSString *)content
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)httpEquiv
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHttpEquiv:(NSString *)httpEquiv
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

- (NSString *)scheme
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setScheme:(NSString *)scheme
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation HTMLBaseElement

- (NSString *)href
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHref:(NSString *)href
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

@implementation HTMLStyleElement

- (BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (void)setDisabled:(BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)media
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setMedia:(NSString *)media
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

@implementation HTMLBodyElement

- (NSString *)aLink
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setALink:(NSString *)aLink
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)background
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setBackground:(NSString *)background
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

- (NSString *)link
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setLink:(NSString *)link
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)text
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setText:(NSString *)text
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (NSString *)vLink
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setVLink:(NSString *)vLink
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation HTMLFormElement

- (HTMLCollection *)elements
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

@implementation HTMLIsIndexElement

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

@implementation HTMLSelectElement

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

- (HTMLFormElement *)form
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (HTMLCollection *)options
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (BOOL)disabled
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
}

- (BOOL)multiple
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return NO;
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

- (long)tabIndex
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return 0;
}

- (void)add:(HTMLElement *)element :(HTMLElement *)before
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

@implementation HTMLOptGroupElement

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

@implementation HTMLOptionElement

- (HTMLFormElement *)form
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

@implementation HTMLInputElement

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

- (HTMLFormElement *)form
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

@implementation HTMLTextAreaElement

- (NSString *)defaultValue
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setDefaultValue:(NSString *)defaultValue
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (HTMLFormElement *)form
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

@implementation HTMLButtonElement

- (HTMLFormElement *)form
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

@implementation HTMLLabelElement

- (HTMLFormElement *)form
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

@implementation HTMLFieldSetElement

- (HTMLFormElement *)form
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

@end

@implementation HTMLLegendElement

- (HTMLFormElement *)form
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

@implementation HTMLUListElement

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

@implementation HTMLOListElement

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

@implementation HTMLDListElement

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

@implementation HTMLDirectoryElement

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

@implementation HTMLMenuElement

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

@implementation HTMLLIElement

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

@implementation HTMLBlockquoteElement

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

@implementation HTMLDivElement

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

@implementation HTMLParagraphElement

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

@implementation HTMLHeadingElement

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

@implementation HTMLQuoteElement

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

@implementation HTMLPreElement

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

@implementation HTMLBRElement

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

@implementation HTMLBaseFontElement

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

@implementation HTMLFontElement

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

@implementation HTMLHRElement

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

@implementation HTMLModElement

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

@implementation HTMLAnchorElement

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

@implementation HTMLImageElement

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

@implementation HTMLObjectElement

- (HTMLFormElement *)form
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

- (NSString *)hspace
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHspace:(NSString *)hspace
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

@implementation HTMLParamElement

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

@implementation HTMLAppletElement

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

- (NSString *)hspace
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setHspace:(NSString *)hspace
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

@implementation HTMLMapElement

- (HTMLCollection *)areas
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

@implementation HTMLAreaElement

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

@implementation HTMLScriptElement

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

@implementation HTMLTableCaptionElement

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

@implementation HTMLTableSectionElement

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

- (HTMLCollection *)rows
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (HTMLElement *)insertRow:(long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)deleteRow:(long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation HTMLTableElement

- (HTMLTableCaptionElement *)caption
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (HTMLTableSectionElement *)tHead
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (HTMLTableSectionElement *)tFoot
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (HTMLCollection *)rows
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (HTMLCollection *)tBodies
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

- (HTMLElement *)createTHead
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)deleteTHead
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (HTMLElement *)createTFoot
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)deleteTFoot
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (HTMLElement *)createCaption
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)deleteCaption
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

- (HTMLElement *)insertRow:(long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");    return nil;
}

- (void)deleteRow:(long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation HTMLTableColElement

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

@implementation HTMLTableRowElement

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

- (HTMLCollection *)cells
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)setCells:(HTMLCollection *)cells // Is cells really read/write?
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

- (HTMLElement *)insertCell:(long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
    return nil;
}

- (void)deleteCell:(long)index
{
    ASSERT_WITH_MESSAGE(0, "not implemented");
}

@end

@implementation HTMLTableCellElement

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

@implementation HTMLFrameSetElement

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

@implementation HTMLFrameElement

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

@end

@implementation HTMLIFrameElement

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

@end
