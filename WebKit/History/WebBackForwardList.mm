/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
#import <JavaScriptCore/Assertions.h>
#import "WebBackForwardList.h"
#import "WebBackForwardListInternal.h"

#import "WebHistoryItemInternal.h"
#import "WebHistoryItemPrivate.h"
#import "WebKitLogging.h"
#import "WebNSObjectExtras.h"
#import "WebPreferencesPrivate.h"
#import "WebKitSystemBits.h"

#import "WebTypesInternal.h"
#import <WebCore/BackForwardList.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/RetainPtr.h>

#define COMPUTE_DEFAULT_PAGE_CACHE_SIZE UINT_MAX

using WebCore::BackForwardList;
using WebCore::HistoryItem;
using WebCore::HistoryItemVector;
using WebCore::RetainPtr;

static inline WebBackForwardListPrivate* kitPrivate(BackForwardList* list) { return (WebBackForwardListPrivate*)list; }
static inline BackForwardList* core(WebBackForwardListPrivate* list) { return (BackForwardList*)list; }

HashMap<BackForwardList*, WebBackForwardList*>& backForwardListWrappers()
{
    static HashMap<BackForwardList*, WebBackForwardList*> backForwardListWrappers;
    return backForwardListWrappers;
}

@implementation WebBackForwardList (WebBackForwardListInternal)

BackForwardList* core(WebBackForwardList *list)
{
    if (!list)
        return 0;
    return core(list->_private);
}

WebBackForwardList *kit(BackForwardList* list)
{
    if (!list)
        return nil;
        
    WebBackForwardList *kitList = backForwardListWrappers().get(list);
    if (kitList)
        return kitList;
    
    return [[[WebBackForwardList alloc] initWithWebCoreBackForwardList:list] autorelease];
}

+ (void)setDefaultPageCacheSizeIfNecessary
{
    static bool initialized = false;
    if (initialized)
        return;
        
    vm_size_t memSize = WebSystemMainMemory();
    unsigned s = [[WebPreferences standardPreferences] _pageCacheSize];
    if (memSize >= 1024 * 1024 * 1024)
        BackForwardList::setDefaultPageCacheSize(s);
    else if (memSize >= 512 * 1024 * 1024)
        BackForwardList::setDefaultPageCacheSize(s - 1);
    else 
        BackForwardList::setDefaultPageCacheSize(s - 2);
        
    initialized = true;
}

- (id)initWithWebCoreBackForwardList:(PassRefPtr<BackForwardList>)list
{   
    self = [super init];
    if (!self)
        return nil;
    
    _private = kitPrivate(list.releaseRef());
    backForwardListWrappers().set(core(_private), self);
    return self;
}

@end

@implementation WebBackForwardList

- (id)init
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    BackForwardList* coreList = new BackForwardList;
    _private = kitPrivate(coreList);
    coreList->ref();
    
    backForwardListWrappers().set(coreList, self);
    
    return self;
}

- (void)dealloc
{
    BackForwardList* coreList = core(_private);
    ASSERT(coreList->closed());
    backForwardListWrappers().remove(coreList);
    coreList->deref();
        
    [super dealloc];
}

- (void)finalize
{
    BackForwardList* coreList = core(_private);
    ASSERT(coreList->closed());
    backForwardListWrappers().remove(coreList);
    coreList->deref();
        
    [super finalize];
}

- (void)_close
{
    core(_private)->close();
}

- (void)addItem:(WebHistoryItem *)entry;
{
    core(_private)->addItem(core(entry));
    
    // Since the assumed contract with WebBackForwardList is that it retains its WebHistoryItems,
    // the following line prevents a whole class of problems where a history item will be created in
    // a function, added to the BFlist, then used in the rest of that function.
    [[entry retain] autorelease];
}

- (void)removeItem:(WebHistoryItem *)item
{
    core(_private)->removeItem(core(item));
}

- (BOOL)containsItem:(WebHistoryItem *)item
{
    return core(_private)->containsItem(core(item));
}

- (void)goBack
{
    core(_private)->goBack();
}

- (void)goForward
{
    core(_private)->goForward();
}

- (void)goToItem:(WebHistoryItem *)item
{
    core(_private)->goToItem(core(item));
}

- (WebHistoryItem *)backItem
{
    return kit(core(_private)->backItem());
}

- (WebHistoryItem *)currentItem
{
    return kit(core(_private)->currentItem());
}

- (WebHistoryItem *)forwardItem
{
    return kit(core(_private)->forwardItem());
}

static NSArray* vectorToNSArray(HistoryItemVector& list)
{
    unsigned size = list.size();
    NSMutableArray *result = [[[NSMutableArray alloc] initWithCapacity:size] autorelease];
    for (unsigned i = 0; i < size; ++i)
        [result addObject:kit(list[i].get())];

    return result;
}

- (NSArray *)backListWithLimit:(int)limit;
{
    HistoryItemVector list;
    core(_private)->backListWithLimit(limit, list);
    return vectorToNSArray(list);
}

- (NSArray *)forwardListWithLimit:(int)limit;
{
    HistoryItemVector list;
    core(_private)->forwardListWithLimit(limit, list);
    return vectorToNSArray(list);
}

- (int)capacity
{
    return core(_private)->capacity();
}

- (void)setCapacity:(int)size
{
    core(_private)->setCapacity(size);
}


-(NSString *)description
{
    NSMutableString *result;
    
    result = [NSMutableString stringWithCapacity:512];
    
    [result appendString:@"\n--------------------------------------------\n"];    
    [result appendString:@"WebBackForwardList:\n"];
    
    BackForwardList* coreList = core(_private);
    HistoryItemVector& entries = coreList->entries();
    
    unsigned size = entries.size();
    for (unsigned i = 0; i < size; ++i) {
        if (entries[i] == coreList->currentItem()) {
            [result appendString:@" >>>"]; 
        } else {
            [result appendString:@"    "]; 
        }   
        [result appendFormat:@"%2d) ", i];
        int currPos = [result length];
        [result appendString:[kit(entries[i].get()) description]];

        // shift all the contents over.  a bit slow, but this is for debugging
        NSRange replRange = {currPos, [result length]-currPos};
        [result replaceOccurrencesOfString:@"\n" withString:@"\n        " options:0 range:replRange];
        
        [result appendString:@"\n"];
    }

    [result appendString:@"\n--------------------------------------------\n"];    

    return result;
}

- (void)_clearPageCache
{
    core(_private)->clearPageCache();
}

- (void)setPageCacheSize: (unsigned)size
{
    core(_private)->setPageCacheSize(size);
}

- (unsigned)pageCacheSize
{
    return core(_private)->pageCacheSize();
}

- (BOOL)_usesPageCache
{
    return core(_private)->usesPageCache();
}

- (int)backListCount
{
    return core(_private)->backListCount();
}

- (int)forwardListCount
{
    return core(_private)->forwardListCount();
}

- (WebHistoryItem *)itemAtIndex:(int)index
{
    return kit(core(_private)->itemAtIndex(index));
}

@end
