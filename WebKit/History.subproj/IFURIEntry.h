//
//  IFURIEntry.h
//  WebKit
//
//  Created by Kenneth Kocienda on Thu Nov 29 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface IFURIEntry : NSObject
{
    NSURL *_url;
    NSString *_title;
    NSString *_displayTitle;
    NSImage *_image;
    NSCalendarDate *_lastVisitedDate;
}

-(id)init;
-(id)initWithURL:(NSURL *)url title:(NSString *)title;
-(id)initWithURL:(NSURL *)url title:(NSString *)title image:(NSImage *)image;

- (NSDictionary *)dictionaryRepresentation;
- (id)initFromDictionaryRepresentation:(NSDictionary *)dict;

-(NSURL *)url;
-(NSString *)title;
-(NSString *)displayTitle;
-(NSImage *)image;
-(NSCalendarDate *)lastVisitedDate;

-(void)setURL:(NSURL *)url;
-(void)setTitle:(NSString *)title;
-(void)setDisplayTitle:(NSString *)displayTitle;
-(void)setImage:(NSImage *)image;
-(void)setLastVisitedDate:(NSCalendarDate *)date;

-(unsigned)hash;
-(BOOL)isEqual:(id)anObject;

@end
