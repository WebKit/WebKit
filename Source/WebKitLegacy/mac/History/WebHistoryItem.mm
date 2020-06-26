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
#import "WebNSDictionaryExtras.h"
#import "WebNSObjectExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebNSViewExtras.h"
#import "WebPluginController.h"
#import "WebTypesInternal.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/BackForwardCache.h>
#import <WebCore/HistoryItem.h>
#import <WebCore/Image.h>
#import <WebCore/ThreadCheck.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/Assertions.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>
#import <wtf/StdLibExtras.h>
#import <wtf/URL.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import <WebCore/WebCoreThreadMessage.h>

NSString *WebViewportInitialScaleKey = @"initial-scale";
NSString *WebViewportMinimumScaleKey = @"minimum-scale";
NSString *WebViewportMaximumScaleKey = @"maximum-scale";
NSString *WebViewportUserScalableKey = @"user-scalable";
NSString *WebViewportShrinkToFitKey  = @"shrink-to-fit";
NSString *WebViewportFitKey          = @"viewport-fit";
NSString *WebViewportWidthKey        = @"width";
NSString *WebViewportHeightKey       = @"height";

NSString *WebViewportFitAutoValue    = @"auto";
NSString *WebViewportFitContainValue = @"contain";
NSString *WebViewportFitCoverValue   = @"cover";

static NSString *scaleKey = @"scale";
static NSString *scaleIsInitialKey = @"scaleIsInitial";
static NSString *scrollPointXKey = @"scrollPointX";
static NSString *scrollPointYKey = @"scrollPointY";
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
    static NeverDestroyed<HistoryItemMap> historyItemWrappers;
    return historyItemWrappers;
}

void WKNotifyHistoryItemChanged(HistoryItem&)
{
#if !PLATFORM(IOS_FAMILY)
    [[NSNotificationCenter defaultCenter]
        postNotificationName:WebHistoryItemChangedNotification object:nil userInfo:nil];
#else
    WebThreadPostNotification(WebHistoryItemChangedNotification, nil, nil);
#endif
}

@implementation WebHistoryItem

+ (void)initialize
{
#if !PLATFORM(IOS_FAMILY)
    JSC::initializeThreading();
    WTF::initializeMainThread();
#endif
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
    return nsStringNilIfEmpty(core(_private)->urlString());
}

// The first URL we loaded to get to where this history item points.  Includes both client
// and server redirects.
- (NSString *)originalURLString
{
    return nsStringNilIfEmpty(core(_private)->originalURLString());
}

- (NSString *)title
{
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

#if !PLATFORM(IOS_FAMILY)
- (NSImage *)icon
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [[WebIconDatabase sharedIconDatabase] iconForURL:[self URLString] withSize:WebIconSmallSize];
    ALLOW_DEPRECATED_DECLARATIONS_END
}
#endif

- (NSTimeInterval)lastVisitedTimeInterval
{
    return _private->_lastVisitedTime;
}

- (NSUInteger)hash
{
    return [(NSString*)core(_private)->urlString() hash];
}

- (BOOL)isEqual:(id)anObject
{
    if (![anObject isMemberOfClass:[WebHistoryItem class]])
        return NO;

    return core(_private)->urlString() == core(((WebHistoryItem*)anObject)->_private)->urlString();
}

- (NSString *)description
{
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
        const auto& children = coreItem->children();
        int currPos = [result length];
        unsigned size = children.size();        
        for (unsigned i = 0; i < size; ++i) {
            WebHistoryItem *child = kit(const_cast<HistoryItem*>(children[i].ptr()));
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
        return nullptr;
    ASSERT(historyItemWrappers().get(core(item->_private)) == item);
    return core(item->_private);
}

WebHistoryItem *kit(HistoryItem* item)
{
    if (!item)
        return nil;
    if (auto wrapper = historyItemWrappers().get(item))
        return [[wrapper retain] autorelease];
    return [[[WebHistoryItem alloc] initWithWebCoreHistoryItem:*item] autorelease];
}

+ (WebHistoryItem *)entryWithURL:(NSURL *)URL
{
    return [[[self alloc] initWithURL:URL title:nil] autorelease];
}

- (id)initWithURLString:(NSString *)URLString title:(NSString *)title displayTitle:(NSString *)displayTitle lastVisitedTimeInterval:(NSTimeInterval)time
{
    auto item = [self initWithWebCoreHistoryItem:HistoryItem::create(URLString, title, displayTitle)];
    if (!item)
        return nil;
    item->_private->_lastVisitedTime = time;
    return item;
}

- (id)initWithWebCoreHistoryItem:(Ref<HistoryItem>&&)item
{   
    WebCoreThreadViolationCheckRoundOne();

    // Need to tell WebCore what function to call for the 
    // "History Item has Changed" notification - no harm in doing this
    // everytime a WebHistoryItem is created
    // Note: We also do this in [WebFrameView initWithFrame:] where we do
    // other "init before WebKit is used" type things
    // FIXME: This means that if we mix legacy WebKit and modern WebKit in the same process, we won't get both notifications.
    WebCore::notifyHistoryItemChanged = WKNotifyHistoryItemChanged;

    if (!(self = [super init]))
        return nil;

    _private = [[WebHistoryItemPrivate alloc] init];
    _private->_historyItem = WTFMove(item);

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
        NSURL *tempURL = [NSURL _webkit_URLWithUserTypedString:URLString];
        ASSERT(tempURL);
        NSString *newURLString = [tempURL _web_originalDataAsString];
        core(_private)->setURLString(newURLString);
        core(_private)->setOriginalURLString(newURLString);
    } 

    if ([dict _webkit_boolForKey:lastVisitWasFailureKey])
        core(_private)->setLastVisitWasFailure(true);
    
    if (NSArray *redirectURLs = [dict _webkit_arrayForKey:redirectURLsKey])
        _private->_redirectURLs = makeUnique<Vector<String>>(makeVector<String>(redirectURLs));

    NSArray *childDicts = [dict objectForKey:childrenKey];
    if (childDicts) {
        for (int i = [childDicts count] - 1; i >= 0; i--) {
            WebHistoryItem *child = [[WebHistoryItem alloc] initFromDictionaryRepresentation:[childDicts objectAtIndex:i]];
            core(_private)->addChildItem(*core(child->_private));
            [child release];
        }
    }

#if PLATFORM(IOS_FAMILY)
    NSNumber *scaleValue = [dict objectForKey:scaleKey];
    NSNumber *scaleIsInitialValue = [dict objectForKey:scaleIsInitialKey];
    if (scaleValue && scaleIsInitialValue)
        core(_private)->setScale([scaleValue floatValue], [scaleIsInitialValue boolValue]);

    if (id viewportArguments = [dict objectForKey:@"WebViewportArguments"])
        [self _setViewportArguments:viewportArguments];

    NSNumber *scrollPointXValue = [dict objectForKey:scrollPointXKey];
    NSNumber *scrollPointYValue = [dict objectForKey:scrollPointYKey];
    if (scrollPointXValue && scrollPointYValue)
        core(_private)->setScrollPosition(IntPoint([scrollPointXValue intValue], [scrollPointYValue intValue]));
#endif

    return self;
}

- (NSPoint)scrollPoint
{
    return core(_private)->scrollPosition();
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
#if PLATFORM(IOS_FAMILY)
- (NSDictionary *)dictionaryRepresentation
{
    return [self dictionaryRepresentationIncludingChildren:YES];
}

- (NSDictionary *)dictionaryRepresentationIncludingChildren:(BOOL)includesChildren
#else
- (NSDictionary *)dictionaryRepresentation
#endif
{
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
        [dict setObject:@YES forKey:lastVisitWasFailureKey];
    if (auto redirectURLs = _private->_redirectURLs.get())
        [dict setObject:createNSArray(*redirectURLs).get() forKey:redirectURLsKey];

#if PLATFORM(IOS_FAMILY)
    if (includesChildren && coreItem->children().size()) {
#else
    if (coreItem->children().size()) {
#endif
        const auto& children = coreItem->children();
        NSMutableArray *childDicts = [NSMutableArray arrayWithCapacity:children.size()];
        
        for (int i = children.size() - 1; i >= 0; i--)
            [childDicts addObject:[kit(const_cast<HistoryItem*>(children[i].ptr())) dictionaryRepresentation]];
        [dict setObject: childDicts forKey:childrenKey];
    }

#if PLATFORM(IOS_FAMILY)
    [dict setObject:[NSNumber numberWithFloat:core(_private)->scale()] forKey:scaleKey];
    [dict setObject:[NSNumber numberWithBool:core(_private)->scaleIsInitial()] forKey:scaleIsInitialKey];

    NSDictionary *viewportArguments = [self _viewportArguments];
    if (viewportArguments)
        [dict setObject:viewportArguments forKey:@"WebViewportArguments"];

    IntPoint scrollPosition = core(_private)->scrollPosition();
    [dict setObject:@(scrollPosition.x()) forKey:scrollPointXKey];
    [dict setObject:@(scrollPosition.y()) forKey:scrollPointYKey];
#endif

    return dict;
}

- (NSString *)target
{
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
    auto& children = core(_private)->children();
    if (children.isEmpty())
        return nil;

    return createNSArray(children, [] (auto& item) {
        return kit(const_cast<HistoryItem*>(item.ptr()));
    }).autorelease();
}

- (NSURL *)URL
{
    const URL& url = core(_private)->url();
    if (url.isEmpty())
        return nil;
    return url;
}

#if !PLATFORM(IOS_FAMILY)
+ (void)_releaseAllPendingPageCaches
{
}
#endif

- (BOOL)lastVisitWasFailure
{
    return core(_private)->lastVisitWasFailure();
}

- (NSArray *)_redirectURLs
{
    auto& redirectURLs = _private->_redirectURLs;
    if (!redirectURLs)
        return nil;

    return createNSArray(*redirectURLs).autorelease();
}

#if PLATFORM(IOS_FAMILY)
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
    [argumentsDictionary setObject:[NSNumber numberWithFloat:viewportArguments.shrinkToFit] forKey:WebViewportShrinkToFitKey];
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
    viewportArguments.shrinkToFit = [[arguments objectForKey:WebViewportShrinkToFitKey] floatValue];
    core(_private)->setViewportArguments(viewportArguments);
}

- (CGPoint)_scrollPoint
{
    return core(_private)->scrollPosition();
}

- (void)_setScrollPoint:(CGPoint)scrollPoint
{
    core(_private)->setScrollPosition(IntPoint(scrollPoint));
}

#endif // PLATFORM(IOS_FAMILY)

- (BOOL)_isInBackForwardCache
{
    return core(_private)->isInBackForwardCache();
}

- (BOOL)_hasCachedPageExpired
{
    return core(_private)->hasCachedPageExpired();
}

@end
