/*	
    WebHistoryItem.m
    Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebHistoryItem.h>

#import <WebKit/WebIconDatabase.h>
#import <WebKit/WebIconLoader.h>

#import <WebFoundation/WebNSDictionaryExtras.h>
#import <WebFoundation/WebNSURLExtras.h>


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

@end

@implementation WebHistoryItem (WebPrivate)

- (BOOL)hasPageCache;
{
    return pageCache != nil;
}

- (void)setHasPageCache: (BOOL)f
{
    if (f && !pageCache)
        pageCache = [[NSMutableDictionary alloc] init];
    if (!f && pageCache){
        [pageCache release];
        pageCache = 0;
    }
}

- pageCache
{
    return pageCache;
}


@end
