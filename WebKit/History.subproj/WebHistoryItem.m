/*	
    WebHistoryItem.m
    Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebHistoryItemPrivate.h>

#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebIconDatabase.h>
#import <WebKit/WebIconLoader.h>
#import <WebKit/WebKitLogging.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebNSDictionaryExtras.h>
#import <WebFoundation/WebNSURLExtras.h>

#import <CoreGraphics/CoreGraphicsPrivate.h>

@implementation WebHistoryItem

- (void)_retainIconInDatabase:(BOOL)retain
{
    NSURL *URL = [self URL];
    if (URL) {
        WebIconDatabase *iconDB = [WebIconDatabase sharedIconDatabase];
        if (retain) {
            [iconDB retainIconForSiteURL:URL];
        } else {
            [iconDB releaseIconForSiteURL:URL];
        }
    }
}

+ (WebHistoryItem *)entryWithURL:(NSURL *)URL
{
    return [[[self alloc] initWithURL:URL title:nil] autorelease];
}

- (id)init
{
    return [self initWithURL:nil title:nil];
}

- (id)initWithURL:(NSURL *)URL title:(NSString *)title
{
    return [self initWithURL:URL target:nil parent:nil title:title];
}

- (id)initWithURL:(NSURL *)URL target:(NSString *)target parent:(NSString *)parent title:(NSString *)title
{
    if (self != [super init])
    {
        return nil;
    }
    
    _URLString = [[URL absoluteString] copy];
    _target = [target copy];
    _parent = [parent copy];
    _title = [title copy];
    _lastVisitedDate = [[NSCalendarDate alloc] init];

    [self _retainIconInDatabase:YES];
    
    return self;
}

- (void)dealloc
{
    [self _retainIconInDatabase:NO];

    [_URLString release];
    [_originalURLString release];
    [_target release];
    [_parent release];
    [_title release];
    [_displayTitle release];
    [_icon release];
    [_lastVisitedDate release];
    [anchor release];
    [_documentState release];
    [_subItems release];
    [pageCache release];
    [_formData release];
    [_formContentType release];
    [_formReferrer release];

    [super dealloc];
}

- (NSURL *)URL
{
    return _URLString ? [NSURL _web_URLWithString:_URLString] : nil;
}

// FIXME: need to decide it this class ever returns URLs, and the name of this method
- (NSString *)URLString
{
    return _URLString;
}

// The first URL we loaded to get to where this history item points.  Includes both client
// and server redirects.
- (NSString *)originalURLString
{
    return _originalURLString;
}

- (NSString *)target
{
    return _target;
}

- (NSString *)parent
{
    return _parent;
}

- (NSString *)title
{
    return _title;
}

- (NSString *)displayTitle;
{
    return _displayTitle;
}

- (void)_setIcon:(NSImage *)newIcon
{
    [newIcon retain];
    [_icon release];
    _icon = newIcon;
}

- (NSImage *)icon
{
    if (!_loadedIcon) {
        NSImage *newIcon = [[WebIconDatabase sharedIconDatabase] iconForSiteURL:[self URL] withSize:WebIconSmallSize];
        [self _setIcon:newIcon];
        _loadedIcon = YES;
    }

    return _icon;
}


- (NSCalendarDate *)lastVisitedDate
{
    return _lastVisitedDate;
}

- (void)setURL:(NSURL *)URL
{
    NSString *string = [URL absoluteString];
    if (!(string == _URLString || [string isEqual:_URLString])) {
        [self _retainIconInDatabase:NO];
        [_URLString release];
        _URLString = [string copy];
        _loadedIcon = NO;
        [self _retainIconInDatabase:YES];
    }
}

// The first URL we loaded to get to where this history item points.  Includes both client
// and server redirects.
- (void)setOriginalURLString:(NSString *)URL
{
    NSString *newURL = [URL copy];
    [_originalURLString release];
    _originalURLString = newURL;
}

- (void)setTitle:(NSString *)title
{
    NSString *newTitle;
    if (title && [title isEqualToString:_displayTitle]) {
        newTitle = [_displayTitle retain];
    } else {
        newTitle = [title copy];
    }
    [_title release];
    _title = newTitle;
}

- (void)setTarget:(NSString *)target
{
    NSString *copy = [target copy];
    [_target release];
    _target = copy;
}

- (void)setParent:(NSString *)parent
{
    NSString *copy = [parent copy];
    [_parent release];
    _parent = copy;
}

- (void)setDisplayTitle:(NSString *)displayTitle
{
    NSString *newDisplayTitle;
    if (displayTitle && [displayTitle isEqualToString:_title]) {
        newDisplayTitle = [_title retain];
    } else {
        newDisplayTitle = [displayTitle copy];
    }
    [_displayTitle release];
    _displayTitle = newDisplayTitle;
}

- (void)setLastVisitedDate:(NSCalendarDate *)date
{
    [date retain];
    [_lastVisitedDate release];
    _lastVisitedDate = date;
}

- (void)setDocumentState:(NSArray *)state;
{
    NSArray *copy = [state copy];
    [_documentState release];
    _documentState = copy;
}

- (NSArray *)documentState
{
    return _documentState;
}

- (NSPoint)scrollPoint
{
    return _scrollPoint;
}

- (void)setScrollPoint:(NSPoint)scrollPoint
{
    _scrollPoint = scrollPoint;
}

- (unsigned)hash
{
    return [_URLString hash];
}

- (NSString *)anchor
{
    return anchor;
}

- (void)setAnchor:(NSString *)a
{
    NSString *copy = [a copy];
    [anchor release];
    anchor = copy;
}

- (BOOL)isTargetItem
{
    return _isTargetItem;
}

- (void)setIsTargetItem:(BOOL)flag
{
    _isTargetItem = flag;
}

// Main diff from the public method is that the public method will default to returning
// the top item if it can't find anything marked as target and has no kids
- (WebHistoryItem *)_recurseToFindTargetItem
{
    if (_isTargetItem) {
        return self;
    } else if (!_subItems) {
        return nil;
    } else {
        int i;
        for (i = [_subItems count]-1; i >= 0; i--) {
            WebHistoryItem *match = [[_subItems objectAtIndex:i] _recurseToFindTargetItem];
            if (match) {
                return match;
            }
        }
        return nil;
    }
}

- (WebHistoryItem *)targetItem
{
    if (_isTargetItem || !_subItems) {
        return self;
    } else {
        return [self _recurseToFindTargetItem];
    }
}

- (NSData *)formData
{
    return _formData;
}

- (void)setFormData:(NSData *)data
{
    NSData *copy = [data copy];
    [_formData release];
    _formData = copy;
}

- (NSString *)formContentType
{
    return _formContentType;
}

- (void)setFormContentType:(NSString *)type
{
    NSString *copy = [type copy];
    [_formContentType release];
    _formContentType = copy;
}

- (NSString *)formReferrer
{
    return _formReferrer;
}

- (void)setFormReferrer:(NSString *)referrer
{
    NSString *copy = [referrer copy];
    [_formReferrer release];
    _formReferrer = copy;
}

- (BOOL)isEqual:(id)anObject
{
    if (![anObject isMemberOfClass:[WebHistoryItem class]]) {
        return NO;
    }
    
    NSString *otherURL = ((WebHistoryItem *)anObject)->_URLString;
    return _URLString == otherURL || [_URLString isEqualToString:otherURL];
}

- (NSArray *)children
{
    return _subItems;
}

- (void)addChildItem:(WebHistoryItem *)item
{
    if (!_subItems) {
        _subItems = [[NSMutableArray arrayWithObject:item] retain];
    } else {
        [_subItems addObject:item];
    }
}

- (WebHistoryItem *)childItemWithName:(NSString *)name
{
    int i;
    for (i = (_subItems ? [_subItems count] : 0)-1; i >= 0; i--) {
        WebHistoryItem *child = [_subItems objectAtIndex:i];
        if ([[child target] isEqualToString:name]) {
            return child;
        }
    }
    return nil;
}

- (NSString *)description
{
    NSMutableString *result = [NSMutableString stringWithFormat:@"%@ %@", [super description], _URLString];
    if (_target) {
        [result appendFormat:@" in \"%@\"", _target];
    }
    if (_isTargetItem) {
        [result appendString:@" *target*"];
    }
    if (_formData) {
        [result appendString:@" *POST*"];
    }
    if (_subItems) {
        int currPos = [result length];
        int i;
        for (i = 0; i < (int)[_subItems count]; i++) {
            WebHistoryItem *child = [_subItems objectAtIndex:i];
            [result appendString:@"\n"];
            [result appendString:[child description]];
        }
        // shift all the contents over.  A bit slow, but hey, this is for debugging.
        NSRange replRange = {currPos, [result length]-currPos};
        [result replaceOccurrencesOfString:@"\n" withString:@"\n    " options:0 range:replRange];
    }
    return result;
}

- (NSDictionary *)dictionaryRepresentation
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity:6];

    if (_URLString) {
        [dict setObject:_URLString forKey:@""];
    }
    if (_title) {
        [dict setObject:_title forKey:@"title"];
    }
    if (_displayTitle) {
        [dict setObject:_displayTitle forKey:@"displayTitle"];
    }
    if (_lastVisitedDate) {
        [dict setObject:[NSString stringWithFormat:@"%lf", [_lastVisitedDate timeIntervalSinceReferenceDate]]
                 forKey:@"lastVisitedDate"];
    }
    if (_subItems != nil) {
        NSMutableArray *childDicts = [NSMutableArray arrayWithCapacity:[_subItems count]];
        int i;
        for (i = [_subItems count]; i >= 0; i--) {
            [childDicts addObject: [[_subItems objectAtIndex:i] dictionaryRepresentation]];
        }
        [dict setObject: childDicts forKey: @"children"];
    }
    
    return dict;
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict
{
    NSString *URLString = [dict _web_stringForKey:@""];
    NSString *title = [dict _web_stringForKey:@"title"];

    [self initWithURL:(URLString ? [NSURL _web_URLWithString:URLString] : nil) title:title];
    
    [self setDisplayTitle:[dict _web_stringForKey:@"displayTitle"]];
    NSString *date = [dict _web_stringForKey:@"lastVisitedDate"];
    if (date) {
        NSCalendarDate *calendarDate = [[NSCalendarDate alloc]
            initWithTimeIntervalSinceReferenceDate:[date doubleValue]];
        [self setLastVisitedDate:calendarDate];
        [calendarDate release];
    }
    
    NSArray *childDicts = [dict objectForKey:@"children"];
    if (childDicts) {
        _subItems = [[NSMutableArray alloc] initWithCapacity:[childDicts count]];
        int i;
        for (i = [childDicts count]; i >= 0; i--) {
            WebHistoryItem *child = [[WebHistoryItem alloc] initFromDictionaryRepresentation: [childDicts objectAtIndex:i]];
            [_subItems addObject: child];
        }
    }

    return self;
}

- (void)setAlwaysAttemptToUsePageCache: (BOOL)flag
{
    _alwaysAttemptToUsePageCache = flag;
}

- (BOOL)alwaysAttemptToUsePageCache
{
    return _alwaysAttemptToUsePageCache;
}


@end

@interface WebWindowWatcher : NSObject
@end

@implementation WebHistoryItem (WebPrivate)

static WebWindowWatcher *_windowWatcher;
static NSMutableSet *_pendingPageCacheToRelease = nil;
static NSTimer *_pageCacheReleaseTimer = nil;

- (BOOL)hasPageCache;
{
    return pageCache != nil;
}

+ (void)_invalidateReleaseTimer
{
    if (_pageCacheReleaseTimer){
        [_pageCacheReleaseTimer invalidate];
        [_pageCacheReleaseTimer release];
        _pageCacheReleaseTimer = nil;
    }
}

- (void)_scheduleRelease
{
    LOG (PageCache, "Scheduling release of %@", [self URLString]);
    if (!_pageCacheReleaseTimer){
        _pageCacheReleaseTimer = [[NSTimer scheduledTimerWithTimeInterval: 2.5 target:self selector:@selector(_releasePageCache:) userInfo:nil repeats:NO] retain];
        if (_pendingPageCacheToRelease == nil){
            _pendingPageCacheToRelease = [[NSMutableSet alloc] init];
        }
    }
    
    // Only add the pageCache on first scheduled attempt.
    if (pageCache){
        [_pendingPageCacheToRelease addObject: pageCache];
        [pageCache release]; // Last reference held by _pendingPageCacheToRelease.
        pageCache = 0;
    }
    
    if (!_windowWatcher){
        _windowWatcher = [[WebWindowWatcher alloc] init];
        [[NSNotificationCenter defaultCenter] addObserver:_windowWatcher selector:@selector(windowWillClose:)
                        name:NSWindowWillCloseNotification object:nil];
    }
}

+ (void)_releaseAllPendingPageCaches
{
    LOG (PageCache, "releasing %d items\n", [_pendingPageCacheToRelease count]);
    [WebHistoryItem _invalidateReleaseTimer];
    [_pendingPageCacheToRelease removeAllObjects];
}

- (void)_releasePageCache: (NSTimer *)timer
{
    CGSRealTimeDelta userDelta;
    CFAbsoluteTime loadDelta;
    
    loadDelta = CFAbsoluteTimeGetCurrent()-[WebFrame _timeOfLastCompletedLoad];
    userDelta = CGSSecondsSinceLastInputEvent(kCGSAnyInputEventType);

    [_pageCacheReleaseTimer release];
    _pageCacheReleaseTimer = nil;

    if ((userDelta < 0.5 || loadDelta < 1.25) && [_pendingPageCacheToRelease count] < 42){
        LOG (PageCache, "postponing again because not quiescent for more than a second (%f since last input, %f since last load).", userDelta, loadDelta);
        [self _scheduleRelease];
        return;
    }
    else
        LOG (PageCache, "releasing, quiescent for more than a second (%f since last input, %f since last load).", userDelta, loadDelta);

    [WebHistoryItem _releaseAllPendingPageCaches];
    
    LOG (PageCache, "Done releasing %p %@", self, [self URLString]);
}

- (void)setHasPageCache: (BOOL)f
{
    if (f && !pageCache)
        pageCache = [[NSMutableDictionary alloc] init];
    if (!f && pageCache){
        [self _scheduleRelease];
        pageCache = 0;
    }
}

- pageCache
{
    return pageCache;
}


@end

@implementation WebWindowWatcher
-(void)windowWillClose:(NSNotification *)notification
{
    [WebHistoryItem _releaseAllPendingPageCaches];
}
@end


