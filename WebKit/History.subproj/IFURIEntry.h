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
    NSString *_comment;
    NSCalendarDate *_creationDate;
    NSCalendarDate *_modificationDate;
    NSCalendarDate *_lastVisitedDate;
}

-(id)init;
-(id)initWithURL:(NSURL *)url title:(NSString *)title;
-(id)initWithURL:(NSURL *)url title:(NSString *)title image:(NSImage *)image;
-(id)initWithURL:(NSURL *)url title:(NSString *)title image:(NSImage *)image comment:(NSString *)comment;

- (NSDictionary *)dictionaryRepresentation;
- (id)initFromDictionaryRepresentation:(NSDictionary *)dict;

-(NSURL *)url;
-(NSString *)title;
-(NSString *)displayTitle;
-(NSImage *)image;
-(NSString *)comment;
-(NSCalendarDate *)creationDate;
-(NSCalendarDate *)modificationDate;
-(NSCalendarDate *)lastVisitedDate;

-(void)setURL:(NSURL *)url;
-(void)setTitle:(NSString *)title;
-(void)setDisplayTitle:(NSString *)displayTitle;
-(void)setImage:(NSImage *)image;
-(void)setComment:(NSString *)comment;
-(void)setModificationDate:(NSCalendarDate *)date;
-(void)setLastVisitedDate:(NSCalendarDate *)date;

-(unsigned)hash;
-(BOOL)isEqual:(id)anObject;

@end
