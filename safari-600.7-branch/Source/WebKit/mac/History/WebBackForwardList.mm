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

#import "WebFrameInternal.h"
#import "WebHistoryItemInternal.h"
#import "WebHistoryItemPrivate.h"
#import "WebKitLogging.h"
#import "WebKitVersionChecks.h"
#import "WebNSObjectExtras.h"
#import "WebPreferencesPrivate.h"
#import "WebTypesInternal.h"
#import "WebViewPrivate.h"
#import <WebCore/BackForwardList.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/Page.h>
#import <WebCore/PageCache.h>
#import <WebCore/Settings.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <runtime/InitializeThreading.h>
#import <wtf/Assertions.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/StdLibExtras.h>

using namespace WebCore;

typedef HashMap<BackForwardList*, WebBackForwardList*> BackForwardListMap;

// FIXME: Instead of this we could just create a class derived from BackForwardList
// with a pointer to a WebBackForwardList in it.
static BackForwardListMap& backForwardLists()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(BackForwardListMap, staticBackForwardLists, ());
    return staticBackForwardLists;
}

@implementation WebBackForwardList (WebBackForwardListInternal)

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

    return [[[WebBackForwardList alloc] initWithBackForwardList:backForwardList] autorelease];
}

- (id)initWithBackForwardList:(PassRefPtr<BackForwardList>)backForwardList
{   
    WebCoreThreadViolationCheckRoundOne();
    self = [super init];
    if (!self)
        return nil;

    _private = reinterpret_cast<WebBackForwardListPrivate*>(backForwardList.leakRef());
    backForwardLists().set(core(self), self);
    return self;
}

@end

@implementation WebBackForwardList

+ (void)initialize
{
#if !PLATFORM(IOS)
    JSC::initializeThreading();
    WTF::initializeMainThreadToProcessMainThread();
    RunLoop::initializeMainRunLoop();
#endif
    WebCoreObjCFinalizeOnMainThread(self);
}

- (id)init
{
    return [self initWithBackForwardList:BackForwardList::create(0)];
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

- (void)finalize
{
    WebCoreThreadViolationCheckRoundOne();
    BackForwardList* backForwardList = core(self);
    ASSERT(backForwardList);
    if (backForwardList) {
        ASSERT(backForwardList->closed());
        backForwardLists().remove(backForwardList);
        backForwardList->deref();
    }
        
    [super finalize];
}

- (void)_close
{
    core(self)->close();
}

- (void)addItem:(WebHistoryItem *)entry
{
    core(self)->addItem(core(entry));
    
    // Since the assumed contract with WebBackForwardList is that it retains its WebHistoryItems,
    // the following line prevents a whole class of problems where a history item will be created in
    // a function, added to the BFlist, then used in the rest of that function.
    [[entry retain] autorelease];
}

- (void)removeItem:(WebHistoryItem *)item
{
    core(self)->removeItem(core(item));
}

#if PLATFORM(IOS)

// FIXME: Move into WebCore the code that deals directly with WebCore::BackForwardList.

#define WebBackForwardListDictionaryEntriesKey @"entries"
#define WebBackForwardListDictionaryCapacityKey @"capacity"
#define WebBackForwardListDictionaryCurrentKey @"current"

- (NSDictionary *)dictionaryRepresentation
{
    BackForwardList *coreBFList = core(self);
    
    HistoryItemVector historyItems = coreBFList->entries();
    unsigned size = historyItems.size();
    NSMutableArray *entriesArray = [[NSMutableArray alloc] initWithCapacity:size];
    for (unsigned i = 0; i < size; ++i)
        [entriesArray addObject:[kit(historyItems[i].get()) dictionaryRepresentationIncludingChildren:NO]];
    
    NSDictionary *dictionary = [NSDictionary dictionaryWithObjectsAndKeys:
        entriesArray, WebBackForwardListDictionaryEntriesKey,
        [NSNumber numberWithUnsignedInt:coreBFList->current()], WebBackForwardListDictionaryCurrentKey,
        [NSNumber numberWithInt:coreBFList->capacity()], WebBackForwardListDictionaryCapacityKey,
        nil];
        
    [entriesArray release];
    
    return dictionary;
}

- (void)setToMatchDictionaryRepresentation:(NSDictionary *)dictionary
{
    BackForwardList *coreBFList = core(self);
    
    coreBFList->setCapacity([[dictionary objectForKey:WebBackForwardListDictionaryCapacityKey] intValue]);
    
    for (NSDictionary *itemDictionary in [dictionary objectForKey:WebBackForwardListDictionaryEntriesKey]) {
        WebHistoryItem *item = [[WebHistoryItem alloc] initFromDictionaryRepresentation:itemDictionary];
        coreBFList->addItem(core(item));
        [item release];
    }

    unsigned currentIndex = [[dictionary objectForKey:WebBackForwardListDictionaryCurrentKey] unsignedIntValue];
    size_t listSize = coreBFList->entries().size();
    if (currentIndex >= listSize)
        currentIndex = listSize - 1;
    coreBFList->setCurrent(currentIndex);
}
#endif // PLATFORM(IOS)

- (BOOL)containsItem:(WebHistoryItem *)item
{
    return core(self)->containsItem(core(item));
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
    core(self)->goToItem(core(item));
}

- (WebHistoryItem *)backItem
{
    return [[kit(core(self)->backItem()) retain] autorelease];
}

- (WebHistoryItem *)currentItem
{
    return [[kit(core(self)->currentItem()) retain] autorelease];
}

- (WebHistoryItem *)forwardItem
{
    return [[kit(core(self)->forwardItem()) retain] autorelease];
}

static NSArray* vectorToNSArray(HistoryItemVector& list)
{
    unsigned size = list.size();
    NSMutableArray *result = [[[NSMutableArray alloc] initWithCapacity:size] autorelease];
    for (unsigned i = 0; i < size; ++i)
        [result addObject:kit(list[i].get())];

    return result;
}

static bool bumperCarBackForwardHackNeeded() 
{
#if !PLATFORM(IOS)
    static bool hackNeeded = [[[NSBundle mainBundle] bundleIdentifier] isEqualToString:@"com.freeverse.bumpercar"] && 
        !WebKitLinkedOnOrAfter(WEBKIT_FIRST_VERSION_WITHOUT_BUMPERCAR_BACK_FORWARD_QUIRK);

    return hackNeeded;
#else
    return false;
#endif
}

- (NSArray *)backListWithLimit:(int)limit
{
    HistoryItemVector list;
    core(self)->backListWithLimit(limit, list);
    NSArray *result = vectorToNSArray(list);
    
    if (bumperCarBackForwardHackNeeded()) {
        static NSArray *lastBackListArray = nil;
        [lastBackListArray release];
        lastBackListArray = [result retain];
    }
    
    return result;
}

- (NSArray *)forwardListWithLimit:(int)limit
{
    HistoryItemVector list;
    core(self)->forwardListWithLimit(limit, list);
    NSArray *result = vectorToNSArray(list);
    
    if (bumperCarBackForwardHackNeeded()) {
        static NSArray *lastForwardListArray = nil;
        [lastForwardListArray release];
        lastForwardListArray = [result retain];
    }
    
    return result;
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
    HistoryItemVector& entries = backForwardList->entries();
    
    unsigned size = entries.size();
    for (unsigned i = 0; i < size; ++i) {
        if (entries[i] == backForwardList->currentItem()) {
            [result appendString:@" >>>"]; 
        } else {
            [result appendString:@"    "]; 
        }   
        [result appendFormat:@"%2d) ", i];
        int currPos = [result length];
        [result appendString:[kit(entries[i].get()) description]];

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
    [kit(core(self)->page()) setUsesPageCache:size != 0];
}

- (NSUInteger)pageCacheSize
{
    return [kit(core(self)->page()) usesPageCache] ? pageCache()->capacity() : 0;
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
    return [[kit(core(self)->itemAtIndex(index)) retain] autorelease];
}

@end
