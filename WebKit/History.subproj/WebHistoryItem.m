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
    [_target release];
    [_parent release];
    [_title release];
    [_displayTitle release];
    [_icon release];
    [_lastVisitedDate release];
    [anchor release];
    [_documentState release];
    
    [super dealloc];
}

- (NSURL *)URL
{
    return [NSURL _web_URLWithString:_URLString];
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

- (void)setTitle:(NSString *)title
{
    NSString *copy = [title copy];
    [_title release];
    _title = copy;
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
    NSString *copy = [displayTitle copy];
    [_displayTitle release];
    _displayTitle = copy;
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

- (BOOL)isEqual:(id)anObject
{
    if (![anObject isMemberOfClass:[WebHistoryItem class]]) {
        return NO;
    }
    
    NSString *otherURL = ((WebHistoryItem *)anObject)->_URLString;
    return _URLString == otherURL || [_URLString isEqualToString:otherURL];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"WebHistoryItem %@", _URLString];
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

    return dict;
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict
{
    NSString *URL = [dict _web_stringForKey:@""];
    NSString *title = [dict _web_stringForKey:@"title"];

    [self initWithURL:[NSURL _web_URLWithString:URL] title:title];
    
    [self setDisplayTitle:[dict _web_stringForKey:@"displayTitle"]];
    NSString *date = [dict _web_stringForKey:@"lastVisitedDate"];
    if (date) {
        NSCalendarDate *calendarDate = [[NSCalendarDate alloc]
            initWithTimeIntervalSinceReferenceDate:[date doubleValue]];
        [self setLastVisitedDate:calendarDate];
        [calendarDate release];
    }
    
    return self;
}
    
@end
