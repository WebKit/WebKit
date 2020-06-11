/*
 * Copyright (C) 2005, 2007 Apple Inc. All rights reserved.
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

#import "WebBackForwardList.h"
#import "WebBackForwardListInternal.h"

#import "BackForwardList.h"
#import "WebFrameInternal.h"
#import "WebHistoryItemInternal.h"
#import "WebHistoryItemPrivate.h"
#import "WebKitLogging.h"
#import "WebKitVersionChecks.h"
#import "WebNSObjectExtras.h"
#import "WebPreferencesPrivate.h"
#import "WebTypesInternal.h"
#import "WebViewPrivate.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/BackForwardCache.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/Settings.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/Assertions.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/VectorCocoa.h>

typedef HashMap<BackForwardList*, WebBackForwardList*> BackForwardListMap;

// FIXME: Instead of this we could just create a class derived from BackForwardList
// with a pointer to a WebBackForwardList in it.
static BackForwardListMap& backForwardLists()
{
    static NeverDestroyed<BackForwardListMap> staticBackForwardLists;
    return staticBackForwardLists;
}

@implementation WebBackForwardList

BackForwardList* core(WebBackForwardList *webBackForwardList)
{
    if (!webBackForwardList)
        return 0;

    return reinterpret_cast<BackForwardList*>(webBackForwardList->_private);
}

WebBackForwardList *kit(BackForwardList* backForwardList)
{
    if (!backForwardList)
        return nil;

    if (WebBackForwardList *webBackForwardList = backForwardLists().get(backForwardList))
        return webBackForwardList;

    return [[[WebBackForwardList alloc] initWithBackForwardList:*backForwardList] autorelease];
}

- (id)initWithBackForwardList:(Ref<BackForwardList>&&)backForwardList
{   
    WebCoreThreadViolationCheckRoundOne();
    self = [super init];
    if (!self)
        return nil;

    _private = reinterpret_cast<WebBackForwardListPrivate*>(&backForwardList.leakRef());
    backForwardLists().set(core(self), self);
    return self;
}

+ (void)initialize
{
#if !PLATFORM(IOS_FAMILY)
    JSC::initializeThreading();
    RunLoop::initializeMain();
#endif
}

- (id)init
{
    return [self initWithBackForwardList:BackForwardList::create(nullptr)];
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebBackForwardList class], self))
        return;

    BackForwardList* backForwardList = core(self);
    ASSERT(backForwardList);
    if (backForwardList) {
        ASSERT(backForwardList->closed());
        backForwardLists().remove(backForwardList);
        backForwardList->deref();
    }

    [super dealloc];
}

- (void)_close
{
    core(self)->close();
}

- (void)addItem:(WebHistoryItem *)entry
{
    ASSERT(entry);
    core(self)->addItem(*core(entry));
    
    // Since the assumed contract with WebBackForwardList is that it retains its WebHistoryItems,
    // the following line prevents a whole class of problems where a history item will be created in
    // a function, added to the BFlist, then used in the rest of that function.
    [[entry retain] autorelease];
}

- (void)removeItem:(WebHistoryItem *)item
{
    if (!item)
        return;

    core(self)->removeItem(*core(item));
}

#if PLATFORM(IOS_FAMILY)

// FIXME: Move into WebCore the code that deals directly with WebCore::BackForwardList.

constexpr auto WebBackForwardListDictionaryEntriesKey = @"entries";
constexpr auto WebBackForwardListDictionaryCapacityKey = @"capacity";
constexpr auto WebBackForwardListDictionaryCurrentKey = @"current";

- (NSDictionary *)dictionaryRepresentation
{
    auto& list = *core(self);
    auto entries = createNSArray(list.entries(), [] (auto& item) {
        return [kit(const_cast<WebCore::HistoryItem*>(item.ptr())) dictionaryRepresentationIncludingChildren:NO];
    });
    return @{
        WebBackForwardListDictionaryEntriesKey: entries.get(),
        WebBackForwardListDictionaryCurrentKey: @(list.current()),
        WebBackForwardListDictionaryCapacityKey: @(list.capacity()),
    };
}

- (void)setToMatchDictionaryRepresentation:(NSDictionary *)dictionary
{
    auto& list = *core(self);

    list.setCapacity([[dictionary objectForKey:WebBackForwardListDictionaryCapacityKey] unsignedIntValue]);
    for (NSDictionary *itemDictionary in [dictionary objectForKey:WebBackForwardListDictionaryEntriesKey])
        list.addItem(*core(adoptNS([[WebHistoryItem alloc] initFromDictionaryRepresentation:itemDictionary]).get()));

    unsigned currentIndex = [[dictionary objectForKey:WebBackForwardListDictionaryCurrentKey] unsignedIntValue];
    size_t listSize = list.entries().size();
    if (currentIndex >= listSize)
        currentIndex = listSize - 1;
    list.setCurrent(currentIndex);
}

#endif // PLATFORM(IOS_FAMILY)

- (BOOL)containsItem:(WebHistoryItem *)item
{
    if (!item)
        return NO;

    return core(self)->containsItem(*core(item));
}

- (void)goBack
{
    core(self)->goBack();
}

- (void)goForward
{
    core(self)->goForward();
}

- (void)goToItem:(WebHistoryItem *)item
{
    if (item)
        core(self)->goToItem(*core(item));
}

- (WebHistoryItem *)backItem
{
    return [[kit(core(self)->backItem().get()) retain] autorelease];
}

- (WebHistoryItem *)currentItem
{
    return [[kit(core(self)->currentItem().get()) retain] autorelease];
}

- (WebHistoryItem *)forwardItem
{
    return [[kit(core(self)->forwardItem().get()) retain] autorelease];
}

static bool bumperCarBackForwardHackNeeded()
{
#if !PLATFORM(IOS_FAMILY)
    static bool hackNeeded = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.freeverse.bumpercar"]
        && !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITHOUT_BUMPERCAR_BACK_FORWARD_QUIRK);
    return hackNeeded;
#else
    return false;
#endif
}

- (NSArray *)backListWithLimit:(int)limit
{
    Vector<Ref<WebCore::HistoryItem>> list;
    core(self)->backListWithLimit(limit, list);
    auto result = createNSArray(list, [] (auto& item) {
        return kit(item.ptr());
    });
    if (bumperCarBackForwardHackNeeded()) {
        static NeverDestroyed<RetainPtr<NSArray>> lastBackListArray;
        lastBackListArray.get() = result;
    }
    return result.autorelease();
}

- (NSArray *)forwardListWithLimit:(int)limit
{
    Vector<Ref<WebCore::HistoryItem>> list;
    core(self)->forwardListWithLimit(limit, list);
    auto result = createNSArray(list, [] (auto& item) {
        return kit(item.ptr());
    });
    if (bumperCarBackForwardHackNeeded()) {
        static NeverDestroyed<RetainPtr<NSArray>> lastForwardListArray;
        lastForwardListArray.get() = result;
    }
    return result.autorelease();
}

- (int)capacity
{
    return core(self)->capacity();
}

- (void)setCapacity:(int)size
{
    core(self)->setCapacity(size);
}


-(NSString *)description
{
    NSMutableString *result;
    
    result = [NSMutableString stringWithCapacity:512];
    
    [result appendString:@"\n--------------------------------------------\n"];    
    [result appendString:@"WebBackForwardList:\n"];
    
    BackForwardList* backForwardList = core(self);
    auto& entries = backForwardList->entries();

    for (unsigned i = 0; i < entries.size(); ++i) {
        if (entries[i].ptr() == backForwardList->currentItem()) {
            [result appendString:@" >>>"]; 
        } else {
            [result appendString:@"    "]; 
        }   
        [result appendFormat:@"%2d) ", i];
        int currPos = [result length];
        [result appendString:[kit(const_cast<WebCore::HistoryItem*>(entries[i].ptr())) description]];

        // shift all the contents over.  a bit slow, but this is for debugging
        NSRange replRange = { static_cast<NSUInteger>(currPos), [result length] - currPos };
        [result replaceOccurrencesOfString:@"\n" withString:@"\n        " options:0 range:replRange];
        
        [result appendString:@"\n"];
    }

    [result appendString:@"\n--------------------------------------------\n"];    

    return result;
}

- (void)setPageCacheSize:(NSUInteger)size
{
    [core(self)->webView() setUsesPageCache:size != 0];
}

- (NSUInteger)pageCacheSize
{
    return [core(self)->webView() usesPageCache] ? WebCore::BackForwardCache::singleton().maxSize() : 0;
}

- (int)backListCount
{
    return core(self)->backListCount();
}

- (int)forwardListCount
{
    return core(self)->forwardListCount();
}

- (WebHistoryItem *)itemAtIndex:(int)index
{
    return [[kit(core(self)->itemAtIndex(index).get()) retain] autorelease];
}

@end
