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

#import "WebHistoryItemInternal.h"
#import "WebHistoryItemPrivate.h"

#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebHTMLViewInternal.h"
#import "WebIconDatabase.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebNSArrayExtras.h"
#import "WebNSDictionaryExtras.h"
#import "WebNSObjectExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebNSViewExtras.h"
#import "WebPluginController.h"
#import "WebTypesInternal.h"
#import <WebCore/HistoryItem.h>
#import <WebCore/Image.h>
#import <WebCore/URL.h>
#import <WebCore/PageCache.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <runtime/InitializeThreading.h>
#import <wtf/Assertions.h>
#import <wtf/MainThread.h>
#import <wtf/RunLoop.h>
#import <wtf/StdLibExtras.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS)
#import <WebCore/WebCoreThreadMessage.h>

NSString *WebViewportInitialScaleKey = @"initial-scale";
NSString *WebViewportMinimumScaleKey = @"minimum-scale";
NSString *WebViewportMaximumScaleKey = @"maximum-scale";
NSString *WebViewportUserScalableKey = @"user-scalable";
NSString *WebViewportWidthKey        = @"width";
NSString *WebViewportHeightKey       = @"height";
NSString *WebViewportMinimalUIKey    = @"minimal-ui";

static NSString *scaleKey = @"scale";
static NSString *scaleIsInitialKey = @"scaleIsInitial";
static NSString *scrollPointXKey = @"scrollPointX";
static NSString *scrollPointYKey = @"scrollPointY";

static NSString * const bookmarkIDKey = @"bookmarkID";
static NSString * const sharedLinkUniqueIdentifierKey = @"sharedLinkUniqueIdentifier";
#endif

// Private keys used in the WebHistoryItem's dictionary representation.
// see 3245793 for explanation of "lastVisitedDate"
static NSString *lastVisitedTimeIntervalKey = @"lastVisitedDate";
static NSString *titleKey = @"title";
static NSString *childrenKey = @"children";
static NSString *displayTitleKey = @"displayTitle";
static NSString *lastVisitWasFailureKey = @"lastVisitWasFailure";
static NSString *redirectURLsKey = @"redirectURLs";

// Notification strings.
NSString *WebHistoryItemChangedNotification = @"WebHistoryItemChangedNotification";

using namespace WebCore;

@implementation WebHistoryItemPrivate

@end

typedef HashMap<HistoryItem*, WebHistoryItem*> HistoryItemMap;

static inline WebCoreHistoryItem* core(WebHistoryItemPrivate* itemPrivate)
{
    return itemPrivate->_historyItem.get();
}

static HistoryItemMap& historyItemWrappers()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(HistoryItemMap, historyItemWrappers, ());
    return historyItemWrappers;
}

void WKNotifyHistoryItemChanged(HistoryItem*)
{
#if !PLATFORM(IOS)
    [[NSNotificationCenter defaultCenter]
        postNotificationName:WebHistoryItemChangedNotification object:nil userInfo:nil];
#else
    WebThreadPostNotification(WebHistoryItemChangedNotification, nil, nil);
#endif
}

@implementation WebHistoryItem

+ (void)initialize
{
#if !PLATFORM(IOS)
    JSC::initializeThreading();
    WTF::initializeMainThreadToProcessMainThread();
    RunLoop::initializeMainRunLoop();
#endif
    WebCoreObjCFinalizeOnMainThread(self);
}

- (instancetype)init
{
    return [self initWithWebCoreHistoryItem:HistoryItem::create()];
}

- (instancetype)initWithURLString:(NSString *)URLString title:(NSString *)title lastVisitedTimeInterval:(NSTimeInterval)time
{
    WebCoreThreadViolationCheckRoundOne();

    WebHistoryItem *item = [self initWithWebCoreHistoryItem:HistoryItem::create(URLString, title)];
    item->_private->_lastVisitedTime = time;

    return item;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebHistoryItem class], self))
        return;

    historyItemWrappers().remove(_private->_historyItem.get());
    [_private release];

    [super dealloc];
}

- (void)finalize
{
    WebCoreThreadViolationCheckRoundOne();

    // FIXME: ~HistoryItem is what releases the history item's icon from the icon database
    // It's probably not good to release icons from the database only when the object is garbage-collected. 
    // Need to change design so this happens at a predictable time.
    historyItemWrappers().remove(_private->_historyItem.get());

    [super finalize];
}

- (id)copyWithZone:(NSZone *)zone
{
    WebCoreThreadViolationCheckRoundOne();
    WebHistoryItem *copy = [[[self class] alloc] initWithWebCoreHistoryItem:core(_private)->copy()];

    copy->_private->_lastVisitedTime = _private->_lastVisitedTime;

    historyItemWrappers().set(core(copy->_private), copy);

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

- (NSString *)alternateTitle
{
    return nsStringNilIfEmpty(core(_private)->alternateTitle());
}

#if !PLATFORM(IOS)
- (NSImage *)icon
{
    return [[WebIconDatabase sharedIconDatabase] iconForURL:[self URLString] withSize:WebIconSmallSize];
}
#endif

- (NSTimeInterval)lastVisitedTimeInterval
{
    ASSERT_MAIN_THREAD();
    return _private->_lastVisitedTime;
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
    if (!coreItem->target().isEmpty()) {
        NSString *target = coreItem->target();
        [result appendFormat:@" in \"%@\"", target];
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
        NSRange replRange = { static_cast<NSUInteger>(currPos), [result length] - currPos };
        [result replaceOccurrencesOfString:@"\n" withString:@"\n    " options:0 range:replRange];
    }
    
    return result;
}

HistoryItem* core(WebHistoryItem *item)
{
    if (!item)
        return 0;
    
    ASSERT(historyItemWrappers().get(core(item->_private)) == item);

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

- (id)initWithURL:(NSURL *)URL target:(NSString *)target parent:(NSString *)parent title:(NSString *)title
{
    return [self initWithWebCoreHistoryItem:HistoryItem::create(URL, target, parent, title)];
}

- (id)initWithURLString:(NSString *)URLString title:(NSString *)title displayTitle:(NSString *)displayTitle lastVisitedTimeInterval:(NSTimeInterval)time
{
    WebHistoryItem *item = [self initWithWebCoreHistoryItem:HistoryItem::create(URLString, title, displayTitle)];

    item->_private->_lastVisitedTime = time;

    return item;
}

- (id)initWithWebCoreHistoryItem:(PassRefPtr<HistoryItem>)item
{   
    WebCoreThreadViolationCheckRoundOne();
    // Need to tell WebCore what function to call for the 
    // "History Item has Changed" notification - no harm in doing this
    // everytime a WebHistoryItem is created
    // Note: We also do this in [WebFrameView initWithFrame:] where we do
    // other "init before WebKit is used" type things
    WebCore::notifyHistoryItemChanged = WKNotifyHistoryItemChanged;
    
    if (!(self = [super init]))
        return nil;

    _private = [[WebHistoryItemPrivate alloc] init];
    _private->_historyItem = item;

    ASSERT(!historyItemWrappers().get(core(_private)));
    historyItemWrappers().set(core(_private), self);
    return self;
}

- (void)setTitle:(NSString *)title
{
    core(_private)->setTitle(title);
}

- (void)setViewState:(id)statePList
{
    core(_private)->setViewState(statePList);
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict
{
    ASSERT_MAIN_THREAD();
    NSString *URLString = [dict _webkit_stringForKey:@""];
    NSString *title = [dict _webkit_stringForKey:titleKey];

    // Do an existence check to avoid calling doubleValue on a nil string. Leave
    // time interval at 0 if there's no value in dict.
    NSString *timeIntervalString = [dict _webkit_stringForKey:lastVisitedTimeIntervalKey];
    NSTimeInterval lastVisited = timeIntervalString == nil ? 0 : [timeIntervalString doubleValue];

    self = [self initWithURLString:URLString title:title displayTitle:[dict _webkit_stringForKey:displayTitleKey] lastVisitedTimeInterval:lastVisited];
    
    // Check if we've read a broken URL from the file that has non-Latin1 chars.  If so, try to convert
    // as if it was from user typing.
    if (![URLString canBeConvertedToEncoding:NSISOLatin1StringEncoding]) {
        NSURL *tempURL = [NSURL _web_URLWithUserTypedString:URLString];
        ASSERT(tempURL);
        NSString *newURLString = [tempURL _web_originalDataAsString];
        core(_private)->setURLString(newURLString);
        core(_private)->setOriginalURLString(newURLString);
    } 

    if ([dict _webkit_boolForKey:lastVisitWasFailureKey])
        core(_private)->setLastVisitWasFailure(true);
    
    if (NSArray *redirectURLs = [dict _webkit_arrayForKey:redirectURLsKey]) {
        NSUInteger size = [redirectURLs count];
        auto redirectURLsVector = std::make_unique<Vector<String>>(size);
        for (NSUInteger i = 0; i < size; ++i)
            (*redirectURLsVector)[i] = String([redirectURLs _webkit_stringAtIndex:i]);
        core(_private)->setRedirectURLs(WTF::move(redirectURLsVector));
    }

    NSArray *childDicts = [dict objectForKey:childrenKey];
    if (childDicts) {
        for (int i = [childDicts count] - 1; i >= 0; i--) {
            WebHistoryItem *child = [[WebHistoryItem alloc] initFromDictionaryRepresentation:[childDicts objectAtIndex:i]];
            core(_private)->addChildItem(core(child->_private));
            [child release];
        }
    }

#if PLATFORM(IOS)
    NSNumber *scaleValue = [dict objectForKey:scaleKey];
    NSNumber *scaleIsInitialValue = [dict objectForKey:scaleIsInitialKey];
    if (scaleValue && scaleIsInitialValue)
        core(_private)->setScale([scaleValue floatValue], [scaleIsInitialValue boolValue]);

    if (id viewportArguments = [dict objectForKey:@"WebViewportArguments"])
        [self _setViewportArguments:viewportArguments];

    NSNumber *scrollPointXValue = [dict objectForKey:scrollPointXKey];
    NSNumber *scrollPointYValue = [dict objectForKey:scrollPointYKey];
    if (scrollPointXValue && scrollPointYValue)
        core(_private)->setScrollPoint(IntPoint([scrollPointXValue intValue], [scrollPointYValue intValue]));

    uint32_t bookmarkIDValue = [[dict objectForKey:bookmarkIDKey] unsignedIntValue];
    if (bookmarkIDValue)
        core(_private)->setBookmarkID(bookmarkIDValue);

    NSString *sharedLinkUniqueIdentifierValue = [dict objectForKey:sharedLinkUniqueIdentifierKey];
    if (sharedLinkUniqueIdentifierValue)
        core(_private)->setSharedLinkUniqueIdentifier(sharedLinkUniqueIdentifierValue);
#endif

    return self;
}

- (NSPoint)scrollPoint
{
    ASSERT_MAIN_THREAD();
    return core(_private)->scrollPoint();
}

- (void)_visitedWithTitle:(NSString *)title
{
    core(_private)->setTitle(title);
    _private->_lastVisitedTime = [NSDate timeIntervalSinceReferenceDate];
}

@end

@implementation WebHistoryItem (WebPrivate)

- (id)initWithURL:(NSURL *)URL title:(NSString *)title
{
    return [self initWithURLString:[URL _web_originalDataAsString] title:title lastVisitedTimeInterval:0];
}

// FIXME: The only iOS difference here should be whether YES or NO is passed to dictionaryRepresentationIncludingChildren:
#if PLATFORM(IOS)
- (NSDictionary *)dictionaryRepresentation
{
    return [self dictionaryRepresentationIncludingChildren:YES];
}

- (NSDictionary *)dictionaryRepresentationIncludingChildren:(BOOL)includesChildren
#else
- (NSDictionary *)dictionaryRepresentation
#endif
{
    ASSERT_MAIN_THREAD();
    NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity:8];

    HistoryItem* coreItem = core(_private);
    
    if (!coreItem->urlString().isEmpty())
        [dict setObject:(NSString*)coreItem->urlString() forKey:@""];
    if (!coreItem->title().isEmpty())
        [dict setObject:(NSString*)coreItem->title() forKey:titleKey];
    if (!coreItem->alternateTitle().isEmpty())
        [dict setObject:(NSString*)coreItem->alternateTitle() forKey:displayTitleKey];
    if (_private->_lastVisitedTime) {
        // Store as a string to maintain backward compatibility. (See 3245793)
        [dict setObject:[NSString stringWithFormat:@"%.1lf", _private->_lastVisitedTime]
                 forKey:lastVisitedTimeIntervalKey];
    }
    if (coreItem->lastVisitWasFailure())
        [dict setObject:[NSNumber numberWithBool:YES] forKey:lastVisitWasFailureKey];
    if (Vector<String>* redirectURLs = coreItem->redirectURLs()) {
        size_t size = redirectURLs->size();
        ASSERT(size);
        NSMutableArray *result = [[NSMutableArray alloc] initWithCapacity:size];
        for (size_t i = 0; i < size; ++i)
            [result addObject:(NSString*)redirectURLs->at(i)];
        [dict setObject:result forKey:redirectURLsKey];
        [result release];
    }
    
#if PLATFORM(IOS)
    if (includesChildren && coreItem->children().size()) {
#else
    if (coreItem->children().size()) {
#endif
        const HistoryItemVector& children = coreItem->children();
        NSMutableArray *childDicts = [NSMutableArray arrayWithCapacity:children.size()];
        
        for (int i = children.size() - 1; i >= 0; i--)
            [childDicts addObject:[kit(children[i].get()) dictionaryRepresentation]];
        [dict setObject: childDicts forKey:childrenKey];
    }

#if PLATFORM(IOS)
    [dict setObject:[NSNumber numberWithFloat:core(_private)->scale()] forKey:scaleKey];
    [dict setObject:[NSNumber numberWithBool:core(_private)->scaleIsInitial()] forKey:scaleIsInitialKey];

    NSDictionary *viewportArguments = [self _viewportArguments];
    if (viewportArguments)
        [dict setObject:viewportArguments forKey:@"WebViewportArguments"];

    IntPoint scrollPoint = core(_private)->scrollPoint();
    [dict setObject:[NSNumber numberWithInt:scrollPoint.x()] forKey:scrollPointXKey];
    [dict setObject:[NSNumber numberWithInt:scrollPoint.y()] forKey:scrollPointYKey];

    uint32_t bookmarkID = core(_private)->bookmarkID();
    if (bookmarkID)
        [dict setObject:[NSNumber numberWithUnsignedInt:bookmarkID] forKey:bookmarkIDKey];

    NSString *sharedLinkUniqueIdentifier = [self _sharedLinkUniqueIdentifier];
    if (sharedLinkUniqueIdentifier)
        [dict setObject:sharedLinkUniqueIdentifier forKey:sharedLinkUniqueIdentifierKey];
#endif

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

- (NSString *)RSSFeedReferrer
{
    return nsStringNilIfEmpty(core(_private)->referrer());
}

- (void)setRSSFeedReferrer:(NSString *)referrer
{
    core(_private)->setReferrer(referrer);
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

- (NSURL *)URL
{
    ASSERT_MAIN_THREAD();
    const URL& url = core(_private)->url();
    if (url.isEmpty())
        return nil;
    return url;
}

- (WebHistoryItem *)targetItem
{    
    ASSERT_MAIN_THREAD();
    return kit(core(_private)->targetItem());
}

#if !PLATFORM(IOS)
+ (void)_releaseAllPendingPageCaches
{
}
#endif

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

- (NSArray *)_redirectURLs
{
    Vector<String>* redirectURLs = core(_private)->redirectURLs();
    if (!redirectURLs)
        return nil;

    size_t size = redirectURLs->size();
    ASSERT(size);
    NSMutableArray *result = [[NSMutableArray alloc] initWithCapacity:size];
    for (size_t i = 0; i < size; ++i)
        [result addObject:(NSString*)redirectURLs->at(i)];
    return [result autorelease];
}

#if PLATFORM(IOS)
- (void)_setScale:(float)scale isInitial:(BOOL)aFlag
{
    core(_private)->setScale(scale, aFlag);
}

- (float)_scale
{
    return core(_private)->scale();
}

- (BOOL)_scaleIsInitial
{
    return core(_private)->scaleIsInitial();
}

- (NSDictionary *)_viewportArguments
{
    const ViewportArguments& viewportArguments = core(_private)->viewportArguments();
    NSMutableDictionary *argumentsDictionary = [NSMutableDictionary dictionary];
    [argumentsDictionary setObject:[NSNumber numberWithFloat:viewportArguments.zoom] forKey:WebViewportInitialScaleKey];
    [argumentsDictionary setObject:[NSNumber numberWithFloat:viewportArguments.minZoom] forKey:WebViewportMinimumScaleKey];
    [argumentsDictionary setObject:[NSNumber numberWithFloat:viewportArguments.maxZoom] forKey:WebViewportMaximumScaleKey];
    [argumentsDictionary setObject:[NSNumber numberWithFloat:viewportArguments.width] forKey:WebViewportWidthKey];
    [argumentsDictionary setObject:[NSNumber numberWithFloat:viewportArguments.height] forKey:WebViewportHeightKey];
    [argumentsDictionary setObject:[NSNumber numberWithFloat:viewportArguments.userZoom] forKey:WebViewportUserScalableKey];
    [argumentsDictionary setObject:[NSNumber numberWithBool:viewportArguments.minimalUI] forKey:WebViewportMinimalUIKey];
    return argumentsDictionary;
}

- (void)_setViewportArguments:(NSDictionary *)arguments
{
    ViewportArguments viewportArguments;
    viewportArguments.zoom = [[arguments objectForKey:WebViewportInitialScaleKey] floatValue];
    viewportArguments.minZoom = [[arguments objectForKey:WebViewportMinimumScaleKey] floatValue];
    viewportArguments.maxZoom = [[arguments objectForKey:WebViewportMaximumScaleKey] floatValue];
    viewportArguments.width = [[arguments objectForKey:WebViewportWidthKey] floatValue];
    viewportArguments.height = [[arguments objectForKey:WebViewportHeightKey] floatValue];
    viewportArguments.userZoom = [[arguments objectForKey:WebViewportUserScalableKey] floatValue];
    viewportArguments.minimalUI = [[arguments objectForKey:WebViewportMinimalUIKey] boolValue];
    core(_private)->setViewportArguments(viewportArguments);
}

- (CGPoint)_scrollPoint
{
    return core(_private)->scrollPoint();
}

- (void)_setScrollPoint:(CGPoint)scrollPoint
{
    core(_private)->setScrollPoint(IntPoint(scrollPoint));
}

- (uint32_t)_bookmarkID
{
    return core(_private)->bookmarkID();
}

- (void)_setBookmarkID:(uint32_t)bookmarkID
{
    core(_private)->setBookmarkID(bookmarkID);
}

- (NSString *)_sharedLinkUniqueIdentifier
{
    return nsStringNilIfEmpty(core(_private)->sharedLinkUniqueIdentifier());
}

- (void)_setSharedLinkUniqueIdentifier:(NSString *)identifier
{
    core(_private)->setSharedLinkUniqueIdentifier(identifier);
}
#endif // PLATFORM(IOS)

- (BOOL)_isInPageCache
{
    return core(_private)->isInPageCache();
}

- (BOOL)_hasCachedPageExpired
{
    return core(_private)->hasCachedPageExpired();
}

@end
