/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#import "DOMNodeInternal.h"
#import "WebDOMOperations.h"
#import "WebFrame.h"
#import "WebFrameInternal.h"
#import "WebKitLogging.h"
#import "WebView.h"
#import "WebViewPrivate.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/DragController.h>
#import <WebCore/Frame.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/Image.h>
#import <WebCore/WebCoreJITOperations.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebKitLegacy/DOMCore.h>
#import <WebKitLegacy/DOMExtensions.h>
#import <wtf/MainThread.h>
#import <wtf/RunLoop.h>

using namespace WebCore;

static RetainPtr<CFMutableDictionaryRef>& lookupTable()
{
    static NeverDestroyed<RetainPtr<CFMutableDictionaryRef>> lookupTable;
    return lookupTable;
}

static void addLookupKey(NSString *key, SEL selector)
{
    CFDictionaryAddValue(lookupTable().get(), (__bridge CFStringRef)key, selector);
}

static void cacheValueForKey(const void *key, const void *value, void *self)
{
    // calling objectForKey will cache the value in our _cache dictionary
    [(__bridge WebElementDictionary *)self objectForKey:(__bridge NSString *)key];
}

@implementation WebElementDictionary

+ (void)initialize
{
#if !PLATFORM(IOS_FAMILY)
    JSC::initialize();
    WTF::initializeMainThread();
    WebCore::populateJITOperations();
#endif
}

+ (void)initializeLookupTable
{
    if (lookupTable())
        return;

    lookupTable() = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFCopyStringDictionaryKeyCallBacks, NULL));

    addLookupKey(WebElementDOMNodeKey, @selector(_domNode));
    addLookupKey(WebElementFrameKey, @selector(_webFrame));
    addLookupKey(WebElementImageAltStringKey, @selector(_altDisplayString));
#if !PLATFORM(IOS_FAMILY)
    addLookupKey(WebElementImageKey, @selector(_image));
    addLookupKey(WebElementImageRectKey, @selector(_imageRect));
#endif
    addLookupKey(WebElementImageURLKey, @selector(_absoluteImageURL));
    addLookupKey(WebElementIsSelectedKey, @selector(_isSelected));
    addLookupKey(WebElementMediaURLKey, @selector(_absoluteMediaURL));
    addLookupKey(WebElementSpellingToolTipKey, @selector(_spellingToolTip));
    addLookupKey(WebElementTitleKey, @selector(_title));
    addLookupKey(WebElementLinkURLKey, @selector(_absoluteLinkURL));
    addLookupKey(WebElementLinkTargetFrameKey, @selector(_targetWebFrame));
    addLookupKey(WebElementLinkTitleKey, @selector(_titleDisplayString));
    addLookupKey(WebElementLinkLabelKey, @selector(_textContent));
    addLookupKey(WebElementLinkIsLiveKey, @selector(_isLiveLink));
    addLookupKey(WebElementIsContentEditableKey, @selector(_isContentEditable));
    addLookupKey(WebElementIsInScrollBarKey, @selector(_isInScrollBar));
}

- (id)initWithHitTestResult:(const HitTestResult&)result
{
    [[self class] initializeLookupTable];
    self = [super init];
    if (!self)
        return nil;
    _result = new HitTestResult(result);
    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebElementDictionary class], self))
        return;

    delete _result;
    [_cache release];
    [_nilValues release];
    [super dealloc];
}

- (void)_fillCache
{
    CFDictionaryApplyFunction(lookupTable().get(), cacheValueForKey, (__bridge void*)self);
    _cacheComplete = YES;
}

- (NSUInteger)count
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

    SEL selector = static_cast<SEL>(const_cast<void*>(CFDictionaryGetValue(lookupTable().get(), (__bridge CFTypeRef)key)));
    if (!selector)
        return nil;
    value = [self performSelector:selector];

    NSUInteger lookupTableCount = CFDictionaryGetCount(lookupTable().get());
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
    return kit(_result->innerNonSharedNode());
}

- (WebFrame *)_webFrame
{
    return [[[self _domNode] ownerDocument] webFrame];
}

// String's NSString* operator converts null Strings to empty NSStrings for compatibility
// with AppKit. We need to work around that here.
static NSString* NSStringOrNil(String coreString)
{
    if (coreString.isNull())
        return nil;
    return coreString;
}

- (NSString *)_altDisplayString
{
    return NSStringOrNil(_result->altDisplayString());
}

- (NSString *)_spellingToolTip
{
    TextDirection dir;
    return NSStringOrNil(_result->spellingToolTip(dir));
}

#if !PLATFORM(IOS_FAMILY)

- (NSImage *)_image
{
    Image* image = _result->image();
    return image ? image->nsImage() : nil;
}

- (NSValue *)_imageRect
{
    IntRect rect = _result->imageRect();
    return rect.isEmpty() ? nil : [NSValue valueWithRect:rect];
}

#endif

- (NSURL *)_absoluteImageURL
{
    return _result->absoluteImageURL();
}

- (NSURL *)_absoluteMediaURL
{
    return _result->absoluteMediaURL();
}

- (NSNumber *)_isSelected
{
    return [NSNumber numberWithBool:_result->isSelected()];
}

- (NSString *)_title
{
    TextDirection dir;
    return NSStringOrNil(_result->title(dir));
}

- (NSURL *)_absoluteLinkURL
{
    return _result->absoluteLinkURL();
}

- (WebFrame *)_targetWebFrame
{
    return kit(_result->targetFrame());
}

- (NSString *)_titleDisplayString
{
    return NSStringOrNil(_result->titleDisplayString());
}

- (NSString *)_textContent
{
    return NSStringOrNil(_result->textContent());
}

- (NSNumber *)_isLiveLink
{
    Element* urlElement = _result->URLElement();
    return [NSNumber numberWithBool:(urlElement && isDraggableLink(*urlElement))];
}

- (NSNumber *)_isContentEditable
{
    return [NSNumber numberWithBool:_result->isContentEditable()];
}

- (NSNumber *)_isInScrollBar
{
    return [NSNumber numberWithBool:_result->scrollbar() != 0];
}

@end
