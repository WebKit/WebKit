/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "WebElementDictionary.h"

#import "WebDOMOperations.h"
#import "WebFrame.h"
#import "WebFrameBridge.h"
#import "WebView.h"
#import "WebViewPrivate.h"

#import <WebKit/DOMCore.h>
#import <WebKit/DOMPrivate.h>

typedef enum {
    WebElementSelf,
    WebElementInnerNode,
    WebElementInnerNonSharedNode,
    WebElementURLElement
} WebElementTargetObject;

typedef struct WebElementMethod {
    WebElementTargetObject target;
    SEL selector;
} WebElementMethod;

static CFMutableDictionaryRef lookupTable = NULL;

static void addLookupKey(NSString *key, SEL selector, WebElementTargetObject target)
{
    WebElementMethod *elementMethod = malloc(sizeof(WebElementMethod));
    elementMethod->target = target;
    elementMethod->selector = selector;
    CFDictionaryAddValue(lookupTable, key, elementMethod);
}

static void cacheValueForKey(const void *key, const void *value, void *self)
{
    // calling objectForKey will cache the value in our _cache dictionary
    [(WebElementDictionary *)self objectForKey:(NSString *)key];
}

@implementation WebElementDictionary
+ (void)initializeLookupTable
{
    if (lookupTable)
        return;

    lookupTable = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFCopyStringDictionaryKeyCallBacks, NULL);

    addLookupKey(WebElementDOMNodeKey, @selector(_domNode), WebElementSelf);
    addLookupKey(WebElementFrameKey, @selector(_webFrame), WebElementSelf);
    addLookupKey(WebElementImageAltStringKey, @selector(altDisplayString), WebElementInnerNonSharedNode);
    addLookupKey(WebElementImageKey, @selector(image), WebElementInnerNonSharedNode);
    addLookupKey(WebElementImageRectKey, @selector(_imageRect), WebElementSelf);
    addLookupKey(WebElementImageURLKey, @selector(absoluteImageURL), WebElementInnerNonSharedNode);
    addLookupKey(WebElementIsSelectedKey, @selector(_isSelected), WebElementSelf);
    addLookupKey(WebElementTitleKey, @selector(_title), WebElementSelf);
    addLookupKey(WebElementLinkURLKey, @selector(absoluteLinkURL), WebElementURLElement);
    addLookupKey(WebElementLinkTargetFrameKey, @selector(_targetWebFrame), WebElementSelf);
    addLookupKey(WebElementLinkTitleKey, @selector(titleDisplayString), WebElementURLElement);
    addLookupKey(WebElementLinkLabelKey, @selector(textContent), WebElementURLElement);
}

- (id)initWithInnerNonSharedNode:(DOMNode *)innerNonSharedNode innerNode:(DOMNode *)innerNode URLElement:(DOMElement *)URLElement andPoint:(NSPoint)point
{
    [[self class] initializeLookupTable];
    [super init];
    _point = point;
    _innerNode = [innerNode retain];
    _innerNonSharedNode = [innerNonSharedNode retain];
    _URLElement = [URLElement retain];
    return self;
}

- (void)dealloc
{
    [_innerNode release];
    [_innerNonSharedNode release];
    [_URLElement release];
    [_cache release];
    [_nilValues release];
    [super dealloc];
}

- (void)_fillCache
{
    CFDictionaryApplyFunction(lookupTable, cacheValueForKey, self);
    _cacheComplete = YES;
}

- (unsigned)count
{
    if (!_cacheComplete)
        [self _fillCache];
    return [_cache count];
}

- (NSEnumerator *)keyEnumerator
{
    if (!_cacheComplete)
        [self _fillCache];
    return [_cache keyEnumerator];
}

- (id)objectForKey:(id)key
{
    id value = [_cache objectForKey:key];
    if (value || _cacheComplete || [_nilValues containsObject:key])
        return value;

    WebElementMethod *elementMethod = (WebElementMethod *)CFDictionaryGetValue(lookupTable, key);
    if (!elementMethod)
        return nil;

    id target = nil;
    switch (elementMethod->target) {
        case WebElementSelf:
            target = self;
            break;
        case WebElementInnerNonSharedNode:
            target = _innerNonSharedNode;
            break;
        case WebElementInnerNode:
            target = _innerNode;
            break;
        case WebElementURLElement:
            target = _URLElement;
            break;
    }

    if (target && [target respondsToSelector:elementMethod->selector])
        value = [target performSelector:elementMethod->selector];

    unsigned lookupTableCount = CFDictionaryGetCount(lookupTable);

    if (value) {
        if (!_cache)
            _cache = [[NSMutableDictionary alloc] initWithCapacity:lookupTableCount];
        [_cache setObject:value forKey:key];
    } else {
        if (!_nilValues)
            _nilValues = [[NSMutableSet alloc] initWithCapacity:lookupTableCount];
        [_nilValues addObject:key];
    }

    _cacheComplete = ([_cache count] + [_nilValues count]) == lookupTableCount;

    return value;
}

- (DOMNode *)_domNode
{
    return [[_innerNonSharedNode retain] autorelease];
}

- (WebFrame *)_webFrame
{
    return [[_innerNonSharedNode ownerDocument] webFrame];
}

- (WebFrame *)_targetWebFrame
{
    if (!_URLElement || ![_URLElement respondsToSelector:@selector(target)])
        return nil;
    WebFrame *webFrame = [[_innerNonSharedNode ownerDocument] webFrame];
    NSString *targetName = [_URLElement performSelector:@selector(target)];
    if ([targetName length])
        return [webFrame findFrameNamed:targetName];
    return webFrame;
}

- (NSString *)_title
{
    // Find the title in the nearest enclosing DOM node.
    // For <area> tags in image maps, walk the tree for the <area>, not the <img> using it.
    for (DOMNode *titleNode = _innerNode; titleNode; titleNode = [titleNode parentNode]) {
        if ([titleNode isKindOfClass:[DOMHTMLElement class]]) {
            NSString *title = [(DOMHTMLElement *)titleNode titleDisplayString];
            if ([title length])
                return title;
        }
    }
    return nil;
}

- (NSValue *)_imageRect
{
    if ([self objectForKey:WebElementImageURLKey])
        return [NSValue valueWithRect:[_innerNonSharedNode boundingBox]];
    return nil;
}

- (NSNumber *)_isSelected
{
    return [NSNumber numberWithBool:[[[[_innerNonSharedNode ownerDocument] webFrame] _bridge] isPointInsideSelection:_point]];
}
@end
