/*	
    WebHistoryItem.m
    Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebIconLoader.h>
#import <WebKit/WebKitReallyPrivate.h>

#import <WebFoundation/WebNSURLExtras.h>

@implementation WebHistoryItem

+(WebHistoryItem *)entryWithURL:(NSURL *)URL
{
    return [[[self alloc] initWithURL:URL title:nil] autorelease];
}

-(id)init
{
    return [self initWithURL:nil title:nil];
}

-(id)initWithURL:(NSURL *)URL title:(NSString *)title
{
    return [self initWithURL:URL target: nil parent: nil title:title];
}

-(id)initWithURL:(NSURL *)URL target: (NSString *)target parent: (NSString *)parent title:(NSString *)title
{
    if (self != [super init])
    {
        return nil;
    }
    
    _URL = [URL retain];
    _target = [target retain];
    _parent = [parent retain];
    _title = [title retain];
    _lastVisitedDate = [[NSCalendarDate alloc] init];
    
    return self;
}

- (void)dealloc
{
    [_URL release];
    [_target release];
    [_parent release];
    [_title release];
    [_displayTitle release];
    [_icon release];
    [_iconURL release];
    [_lastVisitedDate release];
    
    [super dealloc];
}

-(NSURL *)URL
{
    return _URL;
}

- (NSURL *)iconURL
{
    return _iconURL;
}

-(NSString *)target
{
    return _target;
}

-(NSString *)parent
{
    return _parent;
}

-(NSString *)title
{
    return _title;
}

-(NSString *)displayTitle;
{
    return _displayTitle;
}

-(NSImage *)icon
{
    if(!_icon && !_loadedIcon){
        if(_iconURL){
            _icon = [[WebIconLoader iconLoaderWithURL:_iconURL] iconFromCache];
        }else if([_URL isFileURL]){
            _icon = [WebIconLoader iconForFileAtPath:[_URL path]];
        }
        [_icon retain];
        _loadedIcon = YES;
    }

    return _icon ? _icon : [WebIconLoader defaultIcon];
}


-(NSCalendarDate *)lastVisitedDate
{
    return _lastVisitedDate;
}

-(void)setURL:(NSURL *)URL
{
    if (URL != _URL) {
        [_URL release];
        _URL = [URL retain];
    }
}

- (void)setIconURL:(NSURL *)iconURL
{
    if (iconURL != _iconURL) {
        [_iconURL release];
        _iconURL = [iconURL retain];
        _loadedIcon = NO;
    }
}


-(void)setTitle:(NSString *)title
{
    if (title != _title) {
        [_title release];
        _title = [title retain];
    }
}

-(void)setTarget:(NSString *)target
{
    if (target != _target) {
        [_target release];
        _target = [target retain];
    }
}

-(void)setParent:(NSString *)parent
{
    if (parent != _parent) {
        [_parent release];
        _parent = [parent retain];
    }
}

-(void)setDisplayTitle:(NSString *)displayTitle
{
    if (displayTitle != _displayTitle) {
        [_displayTitle release];
        _displayTitle = [displayTitle retain];
    }
}

-(void)setLastVisitedDate:(NSCalendarDate *)date
{
    if (date != _lastVisitedDate) {
        [_lastVisitedDate release];
        _lastVisitedDate = [date retain];
    }
}

-(NSPoint)scrollPoint
{
    return _scrollPoint;
}

-(void)setScrollPoint: (NSPoint)scrollPoint
{
    _scrollPoint = scrollPoint;
}

-(unsigned)hash
{
    return [_URL hash];
}

- (NSString *)anchor
{
    return anchor;
}

- (void)setAnchor: (NSString *)a
{
    [a retain];
    [anchor release];
    anchor = a;
}


-(BOOL)isEqual:(id)anObject
{
    BOOL result;
    
    result = NO;

    if ([anObject isMemberOfClass:[WebHistoryItem class]]) {
        result = [_URL isEqual:[anObject URL]];
    }
    
    return result;
}

-(NSString *)description
{
    return [NSString stringWithFormat:@"WebHistoryItem %@", _URL];
}


- (NSDictionary *)dictionaryRepresentation
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity: 6];

    if (_URL != nil) {
        [dict setObject: [_URL absoluteString] forKey: @""];
    }
    if (_title != nil) {
        [dict setObject: _title forKey: @"title"];
    }
    if (_displayTitle != nil) {
        [dict setObject: _displayTitle forKey: @"displayTitle"];
    }
    if (_lastVisitedDate != nil) {
        [dict setObject: [NSString stringWithFormat:@"%lf", [_lastVisitedDate timeIntervalSinceReferenceDate]]
                 forKey: @"lastVisitedDate"];
    }
#if 0
// FIXME 8/15/2002 -- temporarily removing support for storing iconURL (favIcon), due to architecture issues
    if (_iconURL != nil) {
        [dict setObject: [_iconURL absoluteString] forKey: @"iconURL"];
    }
#endif

    return dict;
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict
{
    NSString *storedURLString, *iconURLString;

    [super init];
    
    storedURLString = [dict objectForKey: @""];
    if (storedURLString != nil) {
        _URL = [[NSURL _web_URLWithString:storedURLString] retain];
    }
    
    iconURLString = [dict objectForKey:@"iconURL"];
    if(iconURLString){
        _iconURL = [[NSURL _web_URLWithString:iconURLString] retain];
    }
    
    _title = [[dict objectForKey: @"title"] copy];
    _displayTitle = [[dict objectForKey: @"displayTitle"] copy];
    _lastVisitedDate = [[NSCalendarDate alloc] initWithTimeIntervalSinceReferenceDate:
        [[dict objectForKey: @"lastVisitedDate"] doubleValue]];
    
    
    return self;
}
    
@end
