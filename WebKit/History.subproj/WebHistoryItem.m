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
    return [self initWithURL:nil title:nil image:nil comment:nil];
}

-(id)initWithURL:(NSURL *)url title:(NSString *)title
{
    return [self initWithURL:url title:title image:nil comment:nil];
}

-(id)initWithURL:(NSURL *)url title:(NSString *)title image:(NSImage *)image
{
    return [self initWithURL:url title:title image:image comment:nil];
}

-(id)initWithURL:(NSURL *)url title:(NSString *)title image:(NSImage *)image comment:(NSString *)comment
{
    if (self != [super init])
    {
        return nil;
    }
    
    _url = [url retain];
    _title = [title retain];
    _image = [image retain];
    _comment = [comment retain];
    _creationDate = [[NSCalendarDate alloc] init];
    _modificationDate = [[NSCalendarDate alloc] init];
    _lastVisitedDate = [[NSCalendarDate alloc] init];
    
    return self;
}

- (void)dealloc
{
    [_url release];
    [_title release];
    [_image release];
    [_comment release];
    [_creationDate release];
    [_modificationDate release];
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
        defaultImage = [[NSImage alloc] initByReferencingFile: pathForDefaultImage];
        loadedDefaultImage = YES;
    }

    return defaultImage;
}

-(NSString *)comment
{
    return _comment;
}

-(NSCalendarDate *)creationDate;
{
    return _creationDate;
}

-(NSCalendarDate *)modificationDate;
{
    return _modificationDate;
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

-(void)setImage:(NSImage *)image
{
    if (image != _image) {
        [_image release];
        _image = [image retain];
    }
}

-(void)setComment:(NSString *)comment
{
    if (comment != _comment) {
        [_comment release];
        _comment = [comment retain];
    }
}

-(void)setModificationDate:(NSCalendarDate *)date
{
    if (date != _modificationDate) {
        [_modificationDate release];
        _modificationDate = [date retain];
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
    [dict setObject: [_url absoluteString] forKey: @"url"];
    if (_title != nil) {
        [dict setObject: _title forKey: @"title"];
    }
    if (_comment != nil) {
        [dict setObject: _comment forKey: @"comment"];
    }
    if (_creationDate != nil) {
        [dict setObject: [NSString stringWithFormat:@"%lf", [_creationDate timeIntervalSinceReferenceDate]]
                 forKey: @"creationDate"];
    }
    if (_modificationDate != nil) {
        [dict setObject: [NSString stringWithFormat:@"%lf", [_modificationDate timeIntervalSinceReferenceDate]]
                 forKey: @"modificationDate"];
    }
    if (_lastVisitedDate != nil) {
        [dict setObject: [NSString stringWithFormat:@"%lf", [_lastVisitedDate timeIntervalSinceReferenceDate]]
                 forKey: @"lastVisitedDate"];
    }

    return dict;
}

- (id)initFromDictionaryRepresentation:(NSDictionary *)dict
{
    // FIXME: doesn't save/restore images yet
    if ((self = [super init]) != nil) {
        _url = [[NSURL URLWithString: [dict objectForKey: @"url"]] retain];
        _title = [[dict objectForKey: @"title"] retain];
        _comment = [[dict objectForKey: @"comment"] retain];
        _creationDate = [[[NSCalendarDate alloc] initWithTimeIntervalSinceReferenceDate:
            [[dict objectForKey: @"creationDate"] doubleValue]] retain];
        _modificationDate = [[[NSCalendarDate alloc] initWithTimeIntervalSinceReferenceDate:
            [[dict objectForKey: @"modificationDate"] doubleValue]] retain];
        _lastVisitedDate = [[[NSCalendarDate alloc] initWithTimeIntervalSinceReferenceDate:
            [[dict objectForKey: @"lastVisitedDate"] doubleValue]] retain];
    }

    return self;
}
    
@end
