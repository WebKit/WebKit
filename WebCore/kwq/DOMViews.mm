/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source exceptionCode must retain the above copyright
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

#import "DOMViews.h"

#import "DOMInternal.h"
#import "DOMViewsInternal.h"
#import <kxmlcore/Assertions.h>

#import "dom_docimpl.h"
#import "dom2_viewsimpl.h"

using DOM::AbstractViewImpl;

ALLOW_DOM_CAST(AbstractViewImpl)

@implementation DOMAbstractView

- (DOMDocument *)document
{
    return [DOMDocument _documentWithImpl:[self _abstractViewImpl]->document()];
}

@end

@implementation DOMAbstractView (WebCoreInternal)

- (AbstractViewImpl *)_abstractViewImpl
{
    return DOM_cast<AbstractViewImpl *>(_internal);
}

- (id)_initWithAbstractViewImpl:(AbstractViewImpl *)impl
{
    ASSERT(impl);

    [super _init];
    _internal = DOM_cast<DOMObjectInternal *>(impl);
    impl->ref();
    addDOMWrapper(self, impl);
    return self;
}

+ (DOMAbstractView *)_abstractViewWithImpl:(AbstractViewImpl *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[DOMAbstractView alloc] _initWithAbstractViewImpl:impl] autorelease];
}

@end

@implementation DOMDocument (DOMDocumentView)

- (DOMAbstractView *)defaultView
{
    return [DOMAbstractView _abstractViewWithImpl:[self _documentImpl]->defaultView()];
}

@end
