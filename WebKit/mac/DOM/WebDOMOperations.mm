/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebDOMOperationsPrivate.h"

#import "DOMNodeInternal.h"
#import "WebArchiveInternal.h"
#import "WebArchiver.h"
#import "WebDataSourcePrivate.h"
#import "WebFrameInternal.h"
#import "WebFramePrivate.h"
#import "WebKitNSStringExtras.h"
#import <JavaScriptCore/Assertions.h>
#import <WebCore/CSSHelper.h>
#import <WebCore/Document.h>
#import <WebCore/LegacyWebArchive.h>
#import <WebKit/DOMExtensions.h>
#import <WebKit/DOMHTML.h>

#if ENABLE(SVG)
#import <WebKit/DOMSVG.h>
#endif

using namespace WebCore;

@implementation DOMNode (WebDOMNodeOperations)

- (WebArchive *)webArchive
{
    WebArchive *archive = [[[WebArchive alloc] _initWithCoreLegacyWebArchive:LegacyWebArchive::create([self _node])] autorelease];
    return archive;
}

- (NSString *)markupString
{
    return [[[self ownerDocument] webFrame] _markupStringFromNode:self nodes:nil];
}

@end

@implementation DOMNode (WebDOMNodeOperationsPrivate)

- (NSArray *)_subresourceURLs
{
    Vector<KURL> urls;
    [self _node]->getSubresourceURLs(urls);
    if (!urls.size())
        return nil;

    NSMutableArray *array = [NSMutableArray arrayWithCapacity:urls.size()];
    for (unsigned i = 0; i < urls.size(); ++i)
        [array addObject:(NSURL *)urls[i]];
        
    return array;
}

@end

@implementation DOMDocument (WebDOMDocumentOperations)

- (WebFrame *)webFrame
{
    Document* document = core(self);
    Frame* frame = document->frame();
    if (!frame)
        return nil;
    return kit(frame);
}

- (NSURL *)URLWithAttributeString:(NSString *)string
{
    // FIXME: Is parseURL appropriate here?
    return core(self)->completeURL(parseURL(string));
}

@end

@implementation DOMDocument (WebDOMDocumentOperationsPrivate)

- (DOMRange *)_createRangeWithNode:(DOMNode *)node
{
    DOMRange *range = [self createRange];
    [range selectNode:node];
    return range;
}

- (DOMRange *)_documentRange
{
    return [self _createRangeWithNode:[self documentElement]];
}

@end

@implementation DOMRange (WebDOMRangeOperations)

- (WebArchive *)webArchive
{
    return [WebArchiver archiveRange:self];
}

- (NSString *)markupString
{
    return [[[[self startContainer] ownerDocument] webFrame] _markupStringFromRange:self nodes:nil];
}

@end

@implementation DOMHTMLFrameElement (WebDOMHTMLFrameElementOperations)

- (WebFrame *)contentFrame
{
    return [[self contentDocument] webFrame];
}

@end

@implementation DOMHTMLIFrameElement (WebDOMHTMLIFrameElementOperations)

- (WebFrame *)contentFrame
{
    return [[self contentDocument] webFrame];
}

@end

@implementation DOMHTMLObjectElement (WebDOMHTMLObjectElementOperations)

- (WebFrame *)contentFrame
{
    return [[self contentDocument] webFrame];
}

@end
