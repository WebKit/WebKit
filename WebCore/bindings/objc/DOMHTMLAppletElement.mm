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

- (WebCore::HTMLAppletElement *)_appletElement
{
    return static_cast<WebCore::HTMLAppletElement *>(DOM_cast<WebCore::Node *>(_internal));
}

- (NSString *)align
{
    return [self _appletElement]->align();
}

- (void)setAlign:(NSString *)align
{
    [self _appletElement]->setAlign(align);
}

- (NSString *)alt
{
    return [self _appletElement]->alt();
}

- (void)setAlt:(NSString *)alt
{
    [self _appletElement]->setAlt(alt);
}

- (NSString *)archive
{
    return [self _appletElement]->archive();
}

- (void)setArchive:(NSString *)archive
{
    [self _appletElement]->setArchive(archive);
}

- (NSString *)code
{
    return [self _appletElement]->code();
}

- (void)setCode:(NSString *)code
{
    [self _appletElement]->setCode(code);
}

- (NSString *)codeBase
{
    return [self _appletElement]->codeBase();
}

- (void)setCodeBase:(NSString *)codeBase
{
    [self _appletElement]->setCodeBase(codeBase);
}

- (NSString *)height
{
    return [self _appletElement]->height();
}

- (void)setHeight:(NSString *)height
{
    [self _appletElement]->setHeight(height);
}

//FIXME: DOM spec says hspace should be a DOMString, not an int
- (int)hspace
{
    return [self _appletElement]->hspace().toInt();
}

- (void)setHspace:(int)hspace
{
    [self _appletElement]->setHspace(WebCore::String::number(hspace));
}

- (NSString *)name
{
    return [self _appletElement]->name();
}

- (void)setName:(NSString *)name
{
    [self _appletElement]->setName(name);
}

- (NSString *)object
{
    return [self _appletElement]->object();
}

- (void)setObject:(NSString *)object
{
    [self _appletElement]->setObject(object);
}

//FIXME: DOM spec says vspace should be a DOMString, not an int
- (int)vspace
{
    return [self _appletElement]->vspace().toInt();
}

- (void)setVspace:(int)vspace
{
    [self _appletElement]->setVspace(WebCore::String::number(vspace));
}

- (NSString *)width
{
    return [self _appletElement]->width();
}

- (void)setWidth:(NSString *)width
{
    [self _appletElement]->setWidth(width);
}

@end
