//
//  WKURIEntry.m
//  WebKit
//
//  Created by Kenneth Kocienda on Thu Nov 29 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import "WKURIEntry.h"
#import "WebKitReallyPrivate.h"

#if 0
#import "WCURIEntry.h"

// *** Function to access WCURICache singleton

id <WCURIEntry> WCCreateURIEntry(void)
{
    return [[WKURIEntry alloc] init];
}
#endif

@implementation WKURIEntry

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
    _creationDate = [[NSDate alloc] init];
    _modificationDate = [[NSDate alloc] init];
    _lastVisitedDate = [[NSDate alloc] init];
    
    return self;
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
    return _image;
}

-(NSString *)comment
{
    return _comment;
}

-(NSDate *)creationDate;
{
    return _creationDate;
}

-(NSDate *)modificationDate;
{
    return _modificationDate;
}

-(NSDate *)lastVisitedDate
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

-(void)setModificationDate:(NSDate *)date
{
    if (date != _modificationDate) {
        [_modificationDate release];
        _modificationDate = [date retain];
    }
}

-(void)setLastVisitedDate:(NSDate *)date
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

    if ([anObject isMemberOfClass:[WKURIEntry class]]) {
        result = [_url isEqual:[((WKURIEntry *)anObject) url]];
    }
    
    return result;
}

-(NSString *)description
{
    return [NSString stringWithFormat:@"WKURIEntry %@", _url];
}
    
@end

