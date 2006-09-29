/*
 * Copyright (C) 2004-2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#import "config.h"
#import "DOMHTMLAppletElement.h"

#import "DOMInternal.h"
#import "HTMLAppletElement.h"
#import "PlatformString.h"

@implementation DOMHTMLAppletElement

#define IMPL static_cast<WebCore::HTMLAppletElement*>(reinterpret_cast<WebCore::Node*>(_internal))

- (NSString *)align
{
    return IMPL->align();
}

- (void)setAlign:(NSString *)newAlign
{
    IMPL->setAlign(newAlign);
}

- (NSString *)alt
{
    return IMPL->alt();
}

- (void)setAlt:(NSString *)newAlt
{
    IMPL->setAlt(newAlt);
}

- (NSString *)archive
{
    return IMPL->archive();
}

- (void)setArchive:(NSString *)newArchive
{
    IMPL->setArchive(newArchive);
}

- (NSString *)code
{
    return IMPL->code();
}

- (void)setCode:(NSString *)newCode
{
    IMPL->setCode(newCode);
}

- (NSString *)codeBase
{
    return IMPL->codeBase();
}

- (void)setCodeBase:(NSString *)newCodeBase
{
    IMPL->setCodeBase(newCodeBase);
}

- (NSString *)height
{
    return IMPL->height();
}

- (void)setHeight:(NSString *)newHeight
{
    IMPL->setHeight(newHeight);
}

//FIXME: DOM spec says hspace should be a DOMString, not an int
- (int)hspace
{
    return IMPL->hspace().toInt();
}

- (void)setHspace:(int)newHspace
{
    IMPL->setHspace(WebCore::String::number(newHspace));
}

- (NSString *)name
{
    return IMPL->name();
}

- (void)setName:(NSString *)newName
{
    IMPL->setName(newName);
}

- (NSString *)object
{
    return IMPL->object();
}

- (void)setObject:(NSString *)newObject
{
    IMPL->setObject(newObject);
}

//FIXME: DOM spec says vspace should be a DOMString, not an int
- (int)vspace
{
    return IMPL->vspace().toInt();
}

- (void)setVspace:(int)newVspace
{
    IMPL->setVspace(WebCore::String::number(newVspace));
}

- (NSString *)width
{
    return IMPL->width();
}

- (void)setWidth:(NSString *)newWidth
{
    IMPL->setWidth(newWidth);
}

@end

@implementation DOMHTMLAppletElement (WebCoreInternal)

- (WebCore::HTMLAppletElement *)_HTMLAppletElement
{
    return IMPL;
}

+ (DOMHTMLAppletElement *)_HTMLOptionElementWith:(WebCore::HTMLAppletElement *)impl
{
    return static_cast<DOMHTMLAppletElement*>([DOMNode _nodeWith:impl]);
}

@end
