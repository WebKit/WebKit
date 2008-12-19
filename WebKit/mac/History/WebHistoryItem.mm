/*
 * Copyright (C) 2005, 2007, 2008 Apple Inc. All rights reserved.
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

#import "WebHistoryItemInternal.h"
#import "WebHistoryItemPrivate.h"

#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebHTMLViewInternal.h"
#import "WebIconDatabase.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebNSDictionaryExtras.h"
#import "WebNSObjectExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebNSViewExtras.h"
#import "WebPluginController.h"
#import "WebTypesInternal.h"
#import <WebCore/CachedPage.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/Image.h>
#import <WebCore/KURL.h>
#import <WebCore/PageCache.h>
#import <WebCore/PlatformString.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <runtime/InitializeThreading.h>
#import <wtf/Assertions.h>
#import <wtf/StdLibExtras.h>

// Private keys used in the WebHistoryItem's dictionary representation.
// see 3245793 for explanation of "lastVisitedDate"
static NSString *WebLastVisitedTimeIntervalKey = @"lastVisitedDate";
static NSString *WebVisitCountKey = @"visitCount";
static NSString *WebTitleKey = @"title";
static NSString *WebChildrenKey = @"children";
static NSString *WebDisplayTitleKey = @"displayTitle";
static NSString *lastVisitWasFailureKey = @"lastVisitWasFailure";

// Notification strings.
NSString *WebHistoryItemChangedNotification = @"WebHistoryItemChangedNotification";

using namespace WebCore;

typedef HashMap<HistoryItem*, WebHistoryItem*> HistoryItemMap;

static inline WebHistoryItemPrivate* kitPrivate(WebCoreHistoryItem* list) { return (WebHistoryItemPrivate*)list; }
static inline WebCoreHistoryItem* core(WebHistoryItemPrivate* list) { return (WebCoreHistoryItem*)list; }

HistoryItemMap& historyItemWrappers()
{
    DEFINE_STATIC_LOCAL(HistoryItemMap, historyItemWrappers, ());
    return historyItemWrappers;
}

void WKNotifyHistoryItemChanged()
{
    [[NSNotificationCenter defaultCenter]
        postNotificationName:WebHistoryItemChangedNotification object:nil userInfo:nil];
}

@implementation WebHistoryItem

+ (void)initialize
{
    JSC::initializeThreading();
#ifndef BUILDING_ON_TIGER
    WebCoreObjCFinalizeOnMainThread(self);
#endif
}

- (id)init
{
    return [self initWithWebCoreHistoryItem:HistoryItem::create()];
}

- (id)initWithURLString:(NSString *)URLString title:(NSString *)title lastVisitedTimeInterval:(NSTimeInterval)time
{
    WebCoreThreadViolationCheck();
    return [self initWithWebCoreHistoryItem:HistoryItem::create(URLString, title, time)];
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebHistoryItem class], self))
        return;

    if (_private) {
        HistoryItem* coreItem = core(_private);
        coreItem->deref();
        historyItemWrappers().remove(coreItem);
    }
    [super dealloc];
}

- (void)finalize
{
    WebCoreThreadViolationCheck();
    // FIXME: ~HistoryItem is what releases the history item's icon from the icon database
    // It's probably not good to release icons from the database only when the object is garbage-collected. 
    // Need to change design so this happens at a predictable time.
    if (_private) {
        HistoryItem* coreItem = core(_private);
        coreItem->deref();
        historyItemWrappers().remove(coreItem);
    }
    [super finalize];
}

- (id)copyWithZone:(NSZone *)zone
{
    WebCoreThreadViolationCheck();
    WebHistoryItem *copy = (WebHistoryItem *)NSCopyObject(self, 0, zone);
    RefPtr<HistoryItem> item = core(_private)->copy();
    copy->_private = kitPrivate(item.get());
    historyItemWrappers().set(item.release().releaseRef(), copy);
    
    return copy;
}

// FIXME: Need to decide if this class ever returns URLs and decide on the name of this method
- (NSString *)URLString
{
    ASSERT_MAIN_THREAD();
    return nsStringNilIfEmpty(core(_private)->urlString());
}

// The first URL we loaded to get to where this history item points.  Includes both client
// and server redirects.
- (NSString *)originalURLString
{
    ASSERT_MAIN_THREAD();
    return nsStringNilIfEmpty(core(_private)->originalURLString());
}

- (NSString *)title
{
    ASSERT_MAIN_THREAD();
    return nsStringNilIfEmpty(core(_private)->title());
}

- (void)setAlternateTitle:(NSString *)alternateTitle
{
    core(_private)->setAlternateTitle(alternateTitle);
}

- (NSString *)alternateTitle;
{
    return nsStringNilIfEmpty(core(_private)->alternateTitle());
}

- (NSImage *)icon
{
    return [[WebIconDatabase sharedIconDatabase] iconForURL:[self URLString] withSize:WebIconSmallSize];
    
    // FIXME: Ideally, this code should simply be the following -
    // return core(_private)->icon()->getNSImage();
    // Once radar -
    // <rdar://problem/4906567> - NSImage returned from WebCore::Image may be incorrect size
    // is resolved
}

- (NSTimeInterval)lastVisitedTimeInterval
{
    ASSERT_MAIN_THREAD();
    return core(_private)->lastVisitedTime();
}

- (NSUInteger)hash
{
    return [(NSString*)core(_private)->urlString() hash];
}

- (BOOL)isEqual:(id)anObject
{
    ASSERT_MAIN_THREAD();
    if (![anObject isMemberOfClass:[WebHistoryItem class]]) {
        return NO;
    }
    
    return core(_private)->urlString() == core(((WebHistoryItem*)anObject)->_private)->urlString();
}

- (NSString *)description
{
    ASSERT_MAIN_THREAD();
    HistoryItem* coreItem = core(_private);
    NSMutableString *result = [NSMutableString stringWithFormat:@"%@ %@", [super description], (NSString*)coreItem->urlString()];
    if (coreItem->target()) {
        [result appendFormat:@" in \"%@\"", (NSString*)coreItem->target()];
    }
    if (coreItem->isTargetItem()) {
        [result appendString:@" *target*"];
    }
    if (coreItem->formData()) {
        [result appendString:@" *POST*"];
    }
    
    if (coreItem->children().size()) {
        const HistoryItemVector& children = coreItem->children();
        int currPos = [result length];
        unsigned size = children.size();        
        for (unsigned i = 0; i < size; ++i) {
            WebHistoryItem *child = kit(children[i].get());
            [result appendString:@"\n"];
            [result appendString:[child description]];
        }
        // shift all the contents over.  A bit slow, but hey, this is for debugging.
        NSRange replRange = {currPos, [result length]-currPos};
        [result replaceOccurrencesOfString:@"\n" withString:@"\n    " options:0 range:replRange];
    }
    
    return result;
}

@end

@interface WebWindowWatcher : NSObject
@end


@implementation WebHistoryItem (WebInternal)

HistoryItem* core(WebHistoryItem *item)
{
    if (!item)
        return 0;
    return core(item->_private);
}

WebHistoryItem *kit(HistoryItem* item)
{
    if (!item)
        return nil;
        
    WebHistoryItem *kitItem = historyItemWrappers().get(item);
    if (kitItem)
        return kitItem;
    
    return [[[WebHistoryItem alloc] initWithWebCoreHistoryItem:item] autorelease];
}

+ (WebHistoryItem *)entryWithURL:(NSURL *)URL
{
    return [[[self alloc] initWithURL:URL title:nil] autorelease];
}

static WebWindowWatcher *_windowWatcher = nil;

+ (void)initWindowWatcherIfNecessary
{
    if (_windowWatcher)
        return;
    _windowWatcher = [[WebWindowWatcher alloc] init];
    [[NSNotificationCenter defaultCenter] addObserver:_windowWatcher selector:@selector(windowWillClose:)
        name:NSWindowWillCloseNotification object:nil];
}

- (id)initWithURL:(NSURL *)URL target:(NSString *)target parent:(NSString *)parent title:(NSString *)title
{
    return [self initWithWebCoreHistoryItem:HistoryItem::create(URL, target, parent, title)];
}

- (id)initWithURLString:(NSString *)URLString title:(NSString *)title displayTitle:(NSString *)displayTitle lastVisitedTimeInterval:(NSTimeInterval)time
{
    return [self initWithWebCoreHistoryItem:HistoryItem::create(URLString, title, displayTitle, time)];
}

- (id)initWithWebCoreHistoryItem:(PassRefPtr<HistoryItem>)item
{   
    WebCoreThreadViolationCheck();
    // Need to tell WebCore what function to call for the 
    // "History Item has Changed" notification - no harm in doing this
    // everytime a WebHistoryItem is created
    // Note: We also do this in [WebFrameView initWithFrame:] where we do
    // other "init before WebKit is used" type things
    WebCore::notifyHistoryItemChanged = WKNotifyHistoryItemChanged;
    
    self = [super init];
    
    _private = kitPrivate(item.releaseRef());
    ASSERT(!historyItemWrappers().get(core(_private)));
    historyItemWrappers().set(core(_private), self);
    return self;
}

- (void)setTitle:(NSString *)title
{
    core(_private)->setTitle(title);
}

- (void)setVisitCount:(int)count
{
    core(_private)->setVisitCount(count);
}

- (void)setViewState:(id)statePList;
{
    core(_private)->setViewState(statePList);
}

- (void)_mergeAutoCompleteHints:(WebHistoryItem *)otherItem
{
    ASSERT_ARG(otherItem, otherItem);
    core(_private)->mergeAutoCompleteHints(core(otherItem->_private));
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict
{
    ASSERT_MAIN_THREAD();
    NSString *URLString = [dict _webkit_stringForKey:@""];
    NSString *title = [dict _webkit_stringForKey:WebTitleKey];

    // Do an existence check to avoid calling doubleValue on a nil string. Leave
    // time interval at 0 if there's no value in dict.
    NSString *timeIntervalString = [dict _webkit_stringForKey:WebLastVisitedTimeIntervalKey];
    NSTimeInterval lastVisited = timeIntervalString == nil ? 0 : [timeIntervalString doubleValue];

    self = [self initWithURLString:URLString title:title displayTitle:[dict _webkit_stringForKey:WebDisplayTitleKey] lastVisitedTimeInterval:lastVisited];
    
    // Check if we've read a broken URL from the file that has non-Latin1 chars.  If so, try to convert
    // as if it was from user typing.
    if (![URLString canBeConvertedToEncoding:NSISOLatin1StringEncoding]) {
        NSURL *tempURL = [NSURL _web_URLWithUserTypedString:URLString];
        ASSERT(tempURL);
        NSString *newURLString = [tempURL _web_originalDataAsString];
        core(_private)->setURLString(newURLString);
        core(_private)->setOriginalURLString(newURLString);
    } 

    core(_private)->setVisitCount([dict _webkit_intForKey:WebVisitCountKey]);

    if ([dict _webkit_boolForKey:lastVisitWasFailureKey])
        core(_private)->setLastVisitWasFailure(true);

    NSArray *childDicts = [dict objectForKey:WebChildrenKey];
    if (childDicts) {
        for (int i = [childDicts count] - 1; i >= 0; i--) {
            WebHistoryItem *child = [[WebHistoryItem alloc] initFromDictionaryRepresentation:[childDicts objectAtIndex:i]];
            core(_private)->addChildItem(core(child->_private));
            [child release];
        }
    }

    return self;
}

- (NSPoint)scrollPoint
{
    ASSERT_MAIN_THREAD();
    return core(_private)->scrollPoint();
}

- (void)_visitedWithTitle:(NSString *)title
{
    core(_private)->visited(title, [NSDate timeIntervalSinceReferenceDate]);
}

- (void)_setVisitCount:(int)count
{
    core(_private)->setVisitCount(count);
}

@end

@implementation WebHistoryItem (WebPrivate)

- (id)initWithURL:(NSURL *)URL title:(NSString *)title
{
    return [self initWithURLString:[URL _web_originalDataAsString] title:title lastVisitedTimeInterval:0];
}

- (NSDictionary *)dictionaryRepresentation
{
    ASSERT_MAIN_THREAD();
    NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity:6];

    HistoryItem* coreItem = core(_private);
    
    if (!coreItem->urlString().isEmpty()) {
        [dict setObject:(NSString*)coreItem->urlString() forKey:@""];
    }
    if (!coreItem->title().isEmpty()) {
        [dict setObject:(NSString*)coreItem->title() forKey:WebTitleKey];
    }
    if (!coreItem->alternateTitle().isEmpty()) {
        [dict setObject:(NSString*)coreItem->alternateTitle() forKey:WebDisplayTitleKey];
    }
    if (coreItem->lastVisitedTime() != 0.0) {
        // store as a string to maintain backward compatibility (see 3245793)
        [dict setObject:[NSString stringWithFormat:@"%.1lf", coreItem->lastVisitedTime()]
                 forKey:WebLastVisitedTimeIntervalKey];
    }
    if (coreItem->visitCount())
        [dict setObject:[NSNumber numberWithInt:coreItem->visitCount()] forKey:WebVisitCountKey];
    if (coreItem->lastVisitWasFailure())
        [dict setObject:[NSNumber numberWithBool:YES] forKey:lastVisitWasFailureKey];
    if (coreItem->children().size()) {
        const HistoryItemVector& children = coreItem->children();
        NSMutableArray *childDicts = [NSMutableArray arrayWithCapacity:children.size()];
        
        for (int i = children.size() - 1; i >= 0; i--)
            [childDicts addObject:[kit(children[i].get()) dictionaryRepresentation]];
        [dict setObject: childDicts forKey:WebChildrenKey];
    }

    return dict;
}

- (NSString *)target
{
    ASSERT_MAIN_THREAD();
    return nsStringNilIfEmpty(core(_private)->target());
}

- (BOOL)isTargetItem
{
    return core(_private)->isTargetItem();
}

- (int)visitCount
{
    ASSERT_MAIN_THREAD();
    return core(_private)->visitCount();
}

- (NSString *)RSSFeedReferrer
{
    return nsStringNilIfEmpty(core(_private)->rssFeedReferrer());
}

- (void)setRSSFeedReferrer:(NSString *)referrer
{
    core(_private)->setRSSFeedReferrer(referrer);
}

- (NSArray *)children
{
    ASSERT_MAIN_THREAD();
    const HistoryItemVector& children = core(_private)->children();
    if (!children.size())
        return nil;

    unsigned size = children.size();
    NSMutableArray *result = [[[NSMutableArray alloc] initWithCapacity:size] autorelease];
    
    for (unsigned i = 0; i < size; ++i)
        [result addObject:kit(children[i].get())];
    
    return result;
}

- (void)setAlwaysAttemptToUsePageCache:(BOOL)flag
{
    // Safari 2.0 uses this for SnapBack, so we stub it out to avoid a crash.
}

- (NSURL *)URL
{
    ASSERT_MAIN_THREAD();
    const KURL& url = core(_private)->url();
    if (url.isEmpty())
        return nil;
    return url;
}

// This should not be called directly for WebHistoryItems that are already included
// in WebHistory. Use -[WebHistory setLastVisitedTimeInterval:forItem:] instead.
- (void)_setLastVisitedTimeInterval:(NSTimeInterval)time
{
    core(_private)->setLastVisitedTime(time);
}

// FIXME: <rdar://problem/4880065> - Push Global History into WebCore
// Once that task is complete, this accessor can go away
- (NSCalendarDate *)_lastVisitedDate
{
    ASSERT_MAIN_THREAD();
    return [[[NSCalendarDate alloc] initWithTimeIntervalSinceReferenceDate:core(_private)->lastVisitedTime()] autorelease];
}

- (WebHistoryItem *)targetItem
{    
    ASSERT_MAIN_THREAD();
    HistoryItem* coreItem = core(_private);
    if (coreItem->isTargetItem() || !coreItem->hasChildren())
        return self;
    return kit(coreItem->recurseToFindTargetItem());
}

+ (void)_releaseAllPendingPageCaches
{
    pageCache()->releaseAutoreleasedPagesNow();
}

- (id)_transientPropertyForKey:(NSString *)key
{
    return core(_private)->getTransientProperty(key);
}

- (void)_setTransientProperty:(id)property forKey:(NSString *)key
{
    core(_private)->setTransientProperty(key, property);
}

- (BOOL)lastVisitWasFailure
{
    return core(_private)->lastVisitWasFailure();
}

@end


// FIXME: <rdar://problem/4886761>
// This is a bizarre policy - we flush the page caches ANY time ANY window is closed?  
@implementation WebWindowWatcher
-(void)windowWillClose:(NSNotification *)notification
{
    pageCache()->releaseAutoreleasedPagesNow();
}
@end
