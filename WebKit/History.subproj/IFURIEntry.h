//
//  IFURIEntry.h
//  WebKit
//
//  Created by Kenneth Kocienda on Thu Nov 29 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>

@interface IFURIEntry : NSObject
{
    NSURL *_url;
    NSString *_title;
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

-(NSURL *)url;
-(NSString *)title;
-(NSImage *)image;
-(NSString *)comment;
-(NSCalendarDate *)creationDate;
-(NSCalendarDate *)modificationDate;
-(NSCalendarDate *)lastVisitedDate;

-(void)setURL:(NSURL *)url;
-(void)setTitle:(NSString *)title;
-(void)setImage:(NSImage *)image;
-(void)setComment:(NSString *)comment;
-(void)setModificationDate:(NSCalendarDate *)date;
-(void)setLastVisitedDate:(NSCalendarDate *)date;

-(unsigned)hash;
-(BOOL)isEqual:(id)anObject;

@end
