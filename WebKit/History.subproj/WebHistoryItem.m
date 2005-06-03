/*	
    WebHistoryItem.m
    Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebHistoryItemPrivate.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebIconDatabase.h>
#import <WebKit/WebIconLoader.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebKitNSStringExtras.h>
#import <WebKit/WebNSDictionaryExtras.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebNSURLRequestExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebPluginController.h>

#import <CoreGraphics/CoreGraphicsPrivate.h>

// Private keys used in the WebHistoryItem's dictionary representation.
// see 3245793 for explanation of "lastVisitedDate"
static NSString *WebLastVisitedTimeIntervalKey = @"lastVisitedDate";
static NSString *WebVisitCountKey = @"visitCount";
static NSString *WebTitleKey = @"title";
static NSString *WebChildrenKey = @"children";
static NSString *WebDisplayTitleKey = @"displayTitle";

// Notification strings.
NSString *WebHistoryItemChangedNotification = @"WebHistoryItemChangedNotification";

@interface WebHistoryItemPrivate : NSObject
{
@public
    NSString *URLString;
    NSString *originalURLString;
    NSString *target;
    NSString *parent;
    NSString *title;
    NSString *displayTitle;
    NSCalendarDate *lastVisitedDate;
    NSTimeInterval lastVisitedTimeInterval;
    NSPoint scrollPoint;
    NSArray *documentState;
    NSMutableArray *subItems;
    NSMutableDictionary *pageCache;
    BOOL isTargetItem;
    BOOL alwaysAttemptToUsePageCache;
    int visitCount;
    // info used to repost form data
    NSArray *formData;
    NSString *formContentType;
    NSString *formReferrer;
    // info used to support RSS feeds
    NSString *RSSFeedReferrer;
}
@end

@implementation WebHistoryItemPrivate
- (void)dealloc
{
    [URLString release];
    [originalURLString release];
    [target release];
    [parent release];
    [title release];
    [displayTitle release];
    [lastVisitedDate release];
    [documentState release];
    [subItems release];
    [pageCache release];
    [formData release];
    [formContentType release];
    [formReferrer release];
    [RSSFeedReferrer release];

    [super dealloc];
}
@end

@implementation WebHistoryItem

- (id)init
{
    return [self initWithURLString:nil title:nil lastVisitedTimeInterval:0];
}

- (id)initWithURLString:(NSString *)URLString title:(NSString *)title lastVisitedTimeInterval:(NSTimeInterval)time
{
    self = [super init];
    _private = [[WebHistoryItemPrivate alloc] init];
    _private->lastVisitedTimeInterval = time;
    _private->title = [title copy];
    _private->URLString = [URLString copy];
    _private->originalURLString = [_private->URLString retain];
    [self _retainIconInDatabase:YES];
    return self;
}

- (void)dealloc
{
    [self _retainIconInDatabase:NO];

    [_private release];
    
    [super dealloc];
}

- (void)finalize
{
    // FIXME: Probably not good to release icons from the database only
    // when the object is garbage-collected. Need to change design so
    // this happens at a predictable time.
    [self _retainIconInDatabase:NO];
    [super finalize];
}

- (id)copyWithZone:(NSZone *)zone
{
    WebHistoryItem *copy = NSCopyObject(self, 0, zone);
    copy->_private = [[WebHistoryItemPrivate alloc] init];
    copy->_private->URLString = [_private->URLString copy];
    [copy _retainIconInDatabase:YES];
    copy->_private->originalURLString = [_private->originalURLString copy];
    copy->_private->target = [_private->target copy];
    copy->_private->parent = [_private->parent copy];
    copy->_private->title = [_private->title copy];
    copy->_private->displayTitle = [_private->displayTitle copy];
    copy->_private->lastVisitedTimeInterval = _private->lastVisitedTimeInterval;
    copy->_private->lastVisitedDate = [_private->lastVisitedDate copy];
    copy->_private->scrollPoint = _private->scrollPoint;
    copy->_private->documentState = [_private->documentState copy];
    if (_private->subItems) {
        copy->_private->subItems = [[NSMutableArray alloc] initWithArray:_private->subItems copyItems:YES];
    }
    copy->_private->isTargetItem = _private->isTargetItem;
    copy->_private->formData = [_private->formData copy];
    copy->_private->formContentType = [_private->formContentType copy];
    copy->_private->formReferrer = [_private->formReferrer copy];
    copy->_private->RSSFeedReferrer = [_private->RSSFeedReferrer copy];

    return copy;
}

// FIXME: need to decide it this class ever returns URLs, and the name of this method
- (NSString *)URLString
{
    return _private->URLString;
}

// The first URL we loaded to get to where this history item points.  Includes both client
// and server redirects.
- (NSString *)originalURLString
{
    return _private->originalURLString;
}

- (NSString *)title
{
    return _private->title;
}

- (void)setAlternateTitle:(NSString *)alternateTitle
{
    NSString *newDisplayTitle;
    if (alternateTitle && [alternateTitle isEqualToString:_private->title]) {
        newDisplayTitle = [_private->title retain];
    } else {
        newDisplayTitle = [alternateTitle copy];
    }
    [_private->displayTitle release];
    _private->displayTitle = newDisplayTitle;

    [[NSNotificationCenter defaultCenter]
        postNotificationName: WebHistoryItemChangedNotification object: self userInfo: nil];
}


- (NSString *)alternateTitle;
{
    return _private->displayTitle;
}

- (NSImage *)icon
{
    // Always get fresh icon from database. It's a client's responsibility to watch
    // for updates to the database if desired.
    return [[WebIconDatabase sharedIconDatabase] iconForURL:_private->URLString withSize:WebIconSmallSize];
}


- (NSTimeInterval)lastVisitedTimeInterval
{
    return _private->lastVisitedTimeInterval;
}

- (unsigned)hash
{
    return [_private->URLString hash];
}

- (BOOL)isEqual:(id)anObject
{
    if (![anObject isMemberOfClass:[WebHistoryItem class]]) {
        return NO;
    }
    
    NSString *otherURL = ((WebHistoryItem *)anObject)->_private->URLString;
    return _private->URLString == otherURL || [_private->URLString isEqualToString:otherURL];
}

- (NSString *)description
{
    NSMutableString *result = [NSMutableString stringWithFormat:@"%@ %@", [super description], _private->URLString];
    if (_private->target) {
        [result appendFormat:@" in \"%@\"", _private->target];
    }
    if (_private->isTargetItem) {
        [result appendString:@" *target*"];
    }
    if (_private->formData) {
        [result appendString:@" *POST*"];
    }
    if (_private->subItems) {
        int currPos = [result length];
        int i;
        for (i = 0; i < (int)[_private->subItems count]; i++) {
            WebHistoryItem *child = [_private->subItems objectAtIndex:i];
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

@implementation WebHistoryItem (WebPrivate)

- (void)_retainIconInDatabase:(BOOL)retain
{
    if (_private->URLString) {
        WebIconDatabase *iconDB = [WebIconDatabase sharedIconDatabase];
        if (retain) {
            [iconDB retainIconForURL:_private->URLString];
        } else {
            [iconDB releaseIconForURL:_private->URLString];
        }
    }
}

+ (WebHistoryItem *)entryWithURL:(NSURL *)URL
{
    return [[[self alloc] initWithURL:URL title:nil] autorelease];
}

- (id)initWithURL:(NSURL *)URL title:(NSString *)title
{
    return [self initWithURLString:[URL _web_originalDataAsString] title:title lastVisitedTimeInterval:0];
}

- (id)initWithURL:(NSURL *)URL target:(NSString *)target parent:(NSString *)parent title:(NSString *)title
{
    self = [self initWithURLString:[URL _web_originalDataAsString] title:title lastVisitedTimeInterval:0];
    if (self) {
        _private->target = [target copy];
        _private->parent = [parent copy];
    }
    return self;
}

- (NSURL *)URL
{
    return _private->URLString ? [NSURL _web_URLWithDataAsString:_private->URLString] : nil;
}

- (NSString *)target
{
    return _private->target;
}

- (NSString *)parent
{
    return _private->parent;
}

- (void)setURLString:(NSString *)string
{
    if (!(string == _private->URLString || [string isEqual:_private->URLString])) {
        [self _retainIconInDatabase:NO];
        [_private->URLString release];
        _private->URLString = [string copy];
        [self _retainIconInDatabase:YES];
    }
    
    [[NSNotificationCenter defaultCenter]
        postNotificationName: WebHistoryItemChangedNotification object: self userInfo: nil];
}

- (void)setURL:(NSURL *)URL
{
    [self setURLString:[URL _web_originalDataAsString]];
}

// The first URL we loaded to get to where this history item points.  Includes both client
// and server redirects.
- (void)setOriginalURLString:(NSString *)URL
{
    NSString *newURL = [URL copy];
    [_private->originalURLString release];
    _private->originalURLString = newURL;

    [[NSNotificationCenter defaultCenter]
        postNotificationName: WebHistoryItemChangedNotification object: self userInfo: nil];
}

- (void)setTitle:(NSString *)title
{
    NSString *newTitle;
    if (title && [title isEqualToString:_private->displayTitle]) {
        newTitle = [_private->displayTitle retain];
    } else {
        newTitle = [title copy];
    }
    [_private->title release];
    _private->title = newTitle;

    [[NSNotificationCenter defaultCenter]
        postNotificationName: WebHistoryItemChangedNotification object: self userInfo: nil];
}

- (void)setTarget:(NSString *)target
{
    NSString *copy = [target copy];
    [_private->target release];
    _private->target = copy;
}

- (void)setParent:(NSString *)parent
{
    NSString *copy = [parent copy];
    [_private->parent release];
    _private->parent = copy;
}

- (void)_setLastVisitedTimeInterval:(NSTimeInterval)time
{
    if (_private->lastVisitedTimeInterval != time) {
        _private->lastVisitedTimeInterval = time;
        [_private->lastVisitedDate release];
        _private->lastVisitedDate = nil;
        _private->visitCount++;
    }

    [[NSNotificationCenter defaultCenter]
        postNotificationName: WebHistoryItemChangedNotification object: self userInfo: nil];
}

// FIXME:  Remove this accessor and related ivar.
- (NSCalendarDate *)_lastVisitedDate
{
    if (!_private->lastVisitedDate){
        _private->lastVisitedDate = [[NSCalendarDate alloc]
                    initWithTimeIntervalSinceReferenceDate:_private->lastVisitedTimeInterval];
    }
    return _private->lastVisitedDate;
}

- (int)visitCount
{
    return _private->visitCount;
}

- (void)setVisitCount:(int)count
{
    _private->visitCount = count;
}

- (void)setDocumentState:(NSArray *)state;
{
    NSArray *copy = [state copy];
    [_private->documentState release];
    _private->documentState = copy;
}

- (NSArray *)documentState
{
    return _private->documentState;
}

- (NSPoint)scrollPoint
{
    return _private->scrollPoint;
}

- (void)setScrollPoint:(NSPoint)scrollPoint
{
    _private->scrollPoint = scrollPoint;
}

- (BOOL)isTargetItem
{
    return _private->isTargetItem;
}

- (void)setIsTargetItem:(BOOL)flag
{
    _private->isTargetItem = flag;
}

// Main diff from the public method is that the public method will default to returning
// the top item if it can't find anything marked as target and has no kids
- (WebHistoryItem *)_recurseToFindTargetItem
{
    if (_private->isTargetItem) {
        return self;
    } else if (!_private->subItems) {
        return nil;
    } else {
        int i;
        for (i = [_private->subItems count]-1; i >= 0; i--) {
            WebHistoryItem *match = [[_private->subItems objectAtIndex:i] _recurseToFindTargetItem];
            if (match) {
                return match;
            }
        }
        return nil;
    }
}

- (WebHistoryItem *)targetItem
{
    if (_private->isTargetItem || !_private->subItems) {
        return self;
    } else {
        return [self _recurseToFindTargetItem];
    }
}

- (void)_setFormInfoFromRequest:(NSURLRequest *)request
{
    NSArray *newData = nil;
    NSString *newContentType = nil;
    NSString *newReferrer = nil;
    if ([[request HTTPMethod] _webkit_isCaseInsensitiveEqualToString:@"POST"]) {
        // save form state iff this is a POST

        // FIXME: Eventually we have to make this smart enough to handle the case where
        // we have a stream for the body to handle the "data interspersed with files" feature.
        NSData *body = [request HTTPBody];
        if (body) {
            body = [body copy];
            newData = [[NSArray alloc] initWithObjects:body, nil];
            [body release];
        }

        newContentType = [[request _web_HTTPContentType] copy];
        newReferrer = [[request _web_HTTPReferrer] copy];
    }

    [_private->formData release];
    _private->formData = newData;

    [_private->formContentType release];
    _private->formContentType = newContentType;
    
    [_private->formReferrer release];
    _private->formReferrer = newReferrer;
}

- (NSArray *)formData
{
    return _private->formData;
}

- (NSString *)formContentType
{
    return _private->formContentType;
}

- (NSString *)formReferrer
{
    return _private->formReferrer;
}

- (NSString *)RSSFeedReferrer
{
    return _private->RSSFeedReferrer;
}

- (void)setRSSFeedReferrer:(NSString *)referrer
{
    NSString *copy = [referrer copy];
    [_private->RSSFeedReferrer release];
    _private->RSSFeedReferrer = copy;
}

- (NSArray *)children
{
    return _private->subItems;
}

- (void)_mergeAutoCompleteHints:(WebHistoryItem *)otherItem
{
    if (otherItem != self) {
        _private->visitCount += otherItem->_private->visitCount;
    }
}

- (void)addChildItem:(WebHistoryItem *)item
{
    if (!_private->subItems) {
        _private->subItems = [[NSMutableArray arrayWithObject:item] retain];
    } else {
        [_private->subItems addObject:item];
    }
}

- (WebHistoryItem *)childItemWithName:(NSString *)name
{
    int i;
    for (i = (_private->subItems ? [_private->subItems count] : 0)-1; i >= 0; i--) {
        WebHistoryItem *child = [_private->subItems objectAtIndex:i];
        if ([[child target] isEqualToString:name]) {
            return child;
        }
    }
    return nil;
}

- (NSDictionary *)dictionaryRepresentation
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity:6];

    if (_private->URLString) {
        [dict setObject:_private->URLString forKey:@""];
    }
    if (_private->title) {
        [dict setObject:_private->title forKey:WebTitleKey];
    }
    if (_private->displayTitle) {
        [dict setObject:_private->displayTitle forKey:WebDisplayTitleKey];
    }
    if (_private->lastVisitedTimeInterval != 0.0) {
        // store as a string to maintain backward compatibility (see 3245793)
        [dict setObject:[NSString stringWithFormat:@"%.1lf", _private->lastVisitedTimeInterval]
                 forKey:WebLastVisitedTimeIntervalKey];
    }
    if (_private->visitCount) {
        [dict setObject:[NSNumber numberWithInt:_private->visitCount] forKey:WebVisitCountKey];
    }
    if (_private->subItems != nil) {
        NSMutableArray *childDicts = [NSMutableArray arrayWithCapacity:[_private->subItems count]];
        int i;
        for (i = [_private->subItems count]; i >= 0; i--) {
            [childDicts addObject: [[_private->subItems objectAtIndex:i] dictionaryRepresentation]];
        }
        [dict setObject: childDicts forKey:WebChildrenKey];
    }

    return dict;
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict
{
    NSString *URLString = [dict _webkit_stringForKey:@""];
    NSString *title = [dict _webkit_stringForKey:WebTitleKey];

    // Do an existence check to avoid calling doubleValue on a nil string. Leave
    // time interval at 0 if there's no value in dict.
    NSString *timeIntervalString = [dict _webkit_stringForKey:WebLastVisitedTimeIntervalKey];
    NSTimeInterval lastVisited = timeIntervalString == nil ? 0 : [timeIntervalString doubleValue];

    self = [self initWithURLString:URLString title:title lastVisitedTimeInterval:lastVisited];

    // Check if we've read a broken URL from the file that has non-Latin1 chars.  If so, try to convert
    // as if it was from user typing.
    if (![_private->URLString canBeConvertedToEncoding:NSISOLatin1StringEncoding]) {
        NSURL *tempURL = [NSURL _web_URLWithUserTypedString:_private->URLString];
        ASSERT(tempURL);
        [_private->URLString release];
        _private->URLString = [[tempURL _web_originalDataAsString] copy];
        [_private->originalURLString release];
        _private->originalURLString = [_private->URLString retain];
    }

    [self setAlternateTitle:[dict _webkit_stringForKey:WebDisplayTitleKey]];

    _private->visitCount = [dict _webkit_intForKey:WebVisitCountKey];

    NSArray *childDicts = [dict objectForKey:WebChildrenKey];
    if (childDicts) {
        _private->subItems = [[NSMutableArray alloc] initWithCapacity:[childDicts count]];
        int i;
        for (i = [childDicts count]; i >= 0; i--) {
            WebHistoryItem *child = [[WebHistoryItem alloc] initFromDictionaryRepresentation: [childDicts objectAtIndex:i]];
            [_private->subItems addObject: child];
        }
    }

    return self;
}

- (void)setAlwaysAttemptToUsePageCache: (BOOL)flag
{
    _private->alwaysAttemptToUsePageCache = flag;
}

- (BOOL)alwaysAttemptToUsePageCache
{
    return _private->alwaysAttemptToUsePageCache;
}


static WebWindowWatcher *_windowWatcher;
static NSMutableSet *_pendingPageCacheToRelease = nil;
static NSTimer *_pageCacheReleaseTimer = nil;

- (BOOL)hasPageCache;
{
    return _private->pageCache != nil;
}

+ (void)_invalidateReleaseTimer
{
    if (_pageCacheReleaseTimer){
        [_pageCacheReleaseTimer invalidate];
        [_pageCacheReleaseTimer release];
        _pageCacheReleaseTimer = nil;
    }
}

+ (void)_scheduleReleaseTimer
{
    if (!_pageCacheReleaseTimer){
        _pageCacheReleaseTimer = [[NSTimer scheduledTimerWithTimeInterval: 2.5 target:self selector:@selector(_releasePageCache:) userInfo:nil repeats:NO] retain];
        if (_pendingPageCacheToRelease == nil){
            _pendingPageCacheToRelease = [[NSMutableSet alloc] init];
        }
    }
}

- (void)_scheduleRelease
{
    LOG (PageCache, "Scheduling release of %@", [self URLString]);
    [WebHistoryItem _scheduleReleaseTimer];

    if (_private->pageCache){
        [_pendingPageCacheToRelease addObject: _private->pageCache];
        [_private->pageCache release]; // Last reference held by _pendingPageCacheToRelease.
        _private->pageCache = nil;
    }
    
    if (!_windowWatcher){
        _windowWatcher = [[WebWindowWatcher alloc] init];
        [[NSNotificationCenter defaultCenter] addObserver:_windowWatcher selector:@selector(windowWillClose:)
                        name:NSWindowWillCloseNotification object:nil];
    }
}

+ (void)_destroyAllPluginsInPendingPageCaches
{
    NSEnumerator *pageCaches = [_pendingPageCacheToRelease objectEnumerator];
    NSMutableDictionary *pageCache;
    
    while ((pageCache = [pageCaches nextObject]) != nil) {
        WebHTMLView *HTMLView = [pageCache objectForKey:WebPageCacheDocumentViewKey];
        if ([HTMLView isKindOfClass:[WebHTMLView class]]) {
            // Don't destroy plug-ins that are currently being viewed.
            if ([[[HTMLView _frame] frameView] documentView] != HTMLView) {
                [[HTMLView _pluginController] destroyAllPlugins];
            }
        }
    }
}

+ (void)_releaseAllPendingPageCaches
{
    LOG (PageCache, "releasing %d items\n", [_pendingPageCacheToRelease count]);
    [WebHistoryItem _invalidateReleaseTimer];
    // Plug-ins could retain anything including the WebHTMLView or the window.
    // To avoid any possible retain cycle, call destroyPlugin on all the plug-ins
    // instead of completely relying on dealloc of WebHTMLView.
    [self _destroyAllPluginsInPendingPageCaches];
    [_pendingPageCacheToRelease removeAllObjects];
}

+ (void)_releasePageCache: (NSTimer *)timer
{
    CGSRealTimeDelta userDelta;
    CFAbsoluteTime loadDelta;
    
    loadDelta = CFAbsoluteTimeGetCurrent()-[WebFrame _timeOfLastCompletedLoad];
    userDelta = CGSSecondsSinceLastInputEvent(kCGSAnyInputEventType);

    [_pageCacheReleaseTimer release];
    _pageCacheReleaseTimer = nil;

    if ((userDelta < 0.5 || loadDelta < 1.25) && [_pendingPageCacheToRelease count] < 42){
        LOG (PageCache, "postponing again because not quiescent for more than a second (%f since last input, %f since last load).", userDelta, loadDelta);
        [self _scheduleReleaseTimer];
        return;
    }
    else
        LOG (PageCache, "releasing, quiescent for more than a second (%f since last input, %f since last load).", userDelta, loadDelta);

    [WebHistoryItem _releaseAllPendingPageCaches];
}

- (void)setHasPageCache: (BOOL)f
{
    if (f && !_private->pageCache)
        _private->pageCache = [[NSMutableDictionary alloc] init];
    if (!f && _private->pageCache){
        [self _scheduleRelease];
    }
}

- (NSMutableDictionary *)pageCache
{
    return _private->pageCache;
}


@end

@implementation WebWindowWatcher
-(void)windowWillClose:(NSNotification *)notification
{
    [WebHistoryItem _releaseAllPendingPageCaches];
}
@end


