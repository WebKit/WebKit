//
//  IFURIEntry.m
//  WebKit
//
//  Created by Kenneth Kocienda on Thu Nov 29 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import "IFURIEntry.h"
#import "WebKitReallyPrivate.h"

@implementation IFURIEntry

-(id)init
{
    return [self initWithURL:nil title:nil image:nil];
}

-(id)initWithURL:(NSURL *)url title:(NSString *)title
{
    return [self initWithURL:url title:title image:nil];
}

-(id)initWithURL:(NSURL *)url title:(NSString *)title image:(NSImage *)image
{
    if (self != [super init])
    {
        return nil;
    }
    
    _url = [url retain];
    _title = [title retain];
    _image = [image retain];
    _lastVisitedDate = [[NSCalendarDate alloc] init];
    
    return self;
}

- (void)dealloc
{
    [_url release];
    [_title release];
    [_image release];
    [_lastVisitedDate release];
    
    [super dealloc];
}

-(NSURL *)url
{
    return _url;
}

-(NSString *)title
{
    return _title;
}

-(NSString *)displayTitle;
{
    return _displayTitle;
}

-(NSImage *)image
{
    static NSImage *defaultImage = nil;
    static BOOL loadedDefaultImage = NO;
    
    if (_image != nil) {
        return _image;
    }

    // Attempt to load default image only once, to avoid performance penalty of repeatedly
    // trying and failing to find it.
    if (!loadedDefaultImage) {
        NSString *pathForDefaultImage =
            [[NSBundle bundleForClass:[self class]] pathForResource:@"url_icon" ofType:@"tiff"];
        if (pathForDefaultImage != nil) {
            defaultImage = [[NSImage alloc] initByReferencingFile: pathForDefaultImage];
        }
        loadedDefaultImage = YES;
    }

    return defaultImage;
}

-(NSCalendarDate *)lastVisitedDate
{
    return _lastVisitedDate;
}

-(void)setURL:(NSURL *)url
{
    if (url != _url) {
        [_url release];
        _url = [url retain];
    }
}

-(void)setTitle:(NSString *)title
{
    if (title != _title) {
        [_title release];
        _title = [title retain];
    }
}

-(void)setDisplayTitle:(NSString *)displayTitle
{
    if (displayTitle != _displayTitle) {
        [_displayTitle release];
        _displayTitle = [displayTitle retain];
    }
}

-(void)setImage:(NSImage *)image
{
    if (image != _image) {
        [_image release];
        _image = [image retain];
    }
}

-(void)setLastVisitedDate:(NSCalendarDate *)date
{
    if (date != _lastVisitedDate) {
        [_lastVisitedDate release];
        _lastVisitedDate = [date retain];
    }
}

-(unsigned)hash
{
    return [_url hash];
}

-(BOOL)isEqual:(id)anObject
{
    BOOL result;
    
    result = NO;

    if ([anObject isMemberOfClass:[IFURIEntry class]]) {
        result = [_url isEqual:[((IFURIEntry *)anObject) url]];
    }
    
    return result;
}

-(NSString *)description
{
    return [NSString stringWithFormat:@"IFURIEntry %@", _url];
}


- (NSDictionary *)dictionaryRepresentation
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity: 6];

    // FIXME: doesn't save/restore images yet
    if (_url != nil) {
        [dict setObject: [_url absoluteString] forKey: @"url"];
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

    return dict;
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict
{
    NSString *storedURLString;

    [super init];
    
    // FIXME: doesn't save/restore images yet
    storedURLString = [dict objectForKey: @"url"];
    if (storedURLString != nil) {
        _url = [[NSURL URLWithString:storedURLString] retain];
    }
    _title = [[dict objectForKey: @"title"] retain];
    _displayTitle = [[dict objectForKey: @"displayTitle"] retain];
    _lastVisitedDate = [[[NSCalendarDate alloc] initWithTimeIntervalSinceReferenceDate:
        [[dict objectForKey: @"lastVisitedDate"] doubleValue]] retain];

    return self;
}
    
@end
