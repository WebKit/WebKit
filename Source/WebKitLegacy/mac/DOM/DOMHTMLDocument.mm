/*
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "DOMHTMLDocumentInternal.h"

#import "DOMHTMLCollectionInternal.h"
#import "DOMNodeInternal.h"
#import "ExceptionHandlers.h"
#import <WebCore/HTMLCollection.h>
#import <WebCore/HTMLDocument.h>
#import <WebCore/JSExecState.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebScriptObjectPrivate.h>
#import <wtf/GetPtr.h>
#import <wtf/URL.h>

#define IMPL protect(downcast<WebCore::HTMLDocument>(reinterpret_cast<WebCore::Node*>(_internal)))

@implementation DOMHTMLDocument

- (DOMHTMLCollection *)embeds
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->embeds()));
}

- (DOMHTMLCollection *)plugins
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->embeds()));
}

- (DOMHTMLCollection *)scripts
{
    WebCore::JSMainThreadNullState state;
    return kit(WTF::getPtr(IMPL->scripts()));
}

- (int)width
{
    return 0;
}

- (int)height
{
    return 0;
}

- (NSString *)dir
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->dir()).autorelease();
}

- (void)setDir:(NSString *)newDir
{
    WebCore::JSMainThreadNullState state;
    IMPL->setDir(AtomStringImpl::add((__bridge CFStringRef)newDir).get());
}

- (NSString *)designMode
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->designMode()).autorelease();
}

- (void)setDesignMode:(NSString *)newDesignMode
{
    WebCore::JSMainThreadNullState state;
    IMPL->setDesignMode(StringImpl::create(newDesignMode).get());
}

- (NSString *)compatMode
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->compatMode()).autorelease();
}

- (NSString *)bgColor
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->bgColor()).autorelease();
}

- (void)setBgColor:(NSString *)newBgColor
{
    WebCore::JSMainThreadNullState state;
    IMPL->setBgColor(AtomStringImpl::add((__bridge CFStringRef)newBgColor).get());
}

- (NSString *)fgColor
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->fgColor()).autorelease();
}

- (void)setFgColor:(NSString *)newFgColor
{
    WebCore::JSMainThreadNullState state;
    IMPL->setFgColor(AtomStringImpl::add((__bridge CFStringRef)newFgColor).get());
}

- (NSString *)alinkColor
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->alinkColor()).autorelease();
}

- (void)setAlinkColor:(NSString *)newAlinkColor
{
    WebCore::JSMainThreadNullState state;
    IMPL->setAlinkColor(AtomStringImpl::add((__bridge CFStringRef)newAlinkColor).get());
}

- (NSString *)linkColor
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->linkColorForBindings()).autorelease();
}

- (void)setLinkColor:(NSString *)newLinkColor
{
    WebCore::JSMainThreadNullState state;
    IMPL->setLinkColorForBindings(AtomStringImpl::add((__bridge CFStringRef)newLinkColor).get());
}

- (NSString *)vlinkColor
{
    WebCore::JSMainThreadNullState state;
    return createNSString(IMPL->vlinkColor()).autorelease();
}

- (void)setVlinkColor:(NSString *)newVlinkColor
{
    WebCore::JSMainThreadNullState state;
    IMPL->setVlinkColor(AtomStringImpl::add((__bridge CFStringRef)newVlinkColor).get());
}

- (void)open
{
    WebCore::JSMainThreadNullState state;
    IMPL->open();
}

- (void)close
{
    WebCore::JSMainThreadNullState state;
    IMPL->close();
}

- (void)write:(NSString *)text
{
    WebCore::JSMainThreadNullState state;
    IMPL->write(nullptr, FixedVector<RefPtr<StringImpl>> { StringImpl::create(text) });
}

- (void)writeln:(NSString *)text
{
    WebCore::JSMainThreadNullState state;
    IMPL->writeln(nullptr, FixedVector<RefPtr<StringImpl>> { StringImpl::create(text) });
}

- (void)clear
{
    WebCore::JSMainThreadNullState state;
    IMPL->clear();
}

- (void)captureEvents
{
    WebCore::JSMainThreadNullState state;
    IMPL->captureEvents();
}

- (void)releaseEvents
{
    WebCore::JSMainThreadNullState state;
    IMPL->releaseEvents();
}

@end

WebCore::HTMLDocument* core(DOMHTMLDocument *wrapper)
{
    return wrapper ? reinterpret_cast<WebCore::HTMLDocument*>(wrapper->_internal) : 0;
}

SUPPRESS_NODELETE DOMHTMLDocument *kit(WebCore::HTMLDocument* value)
{
    WebCoreThreadViolationCheckRoundOne();
    return static_cast<DOMHTMLDocument*>(kit(static_cast<WebCore::Node*>(value)));
}

#undef IMPL
