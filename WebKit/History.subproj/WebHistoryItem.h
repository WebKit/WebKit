//
//  IFURIEntry.h
//  WebKit
//
//  Created by Kenneth Kocienda on Thu Nov 29 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
//#import <WCURIEntry.h>

//@interface IFURIEntry : NSObject <WCURIEntry>
@interface IFURIEntry : NSObject
{
    NSURL *_url;
    NSString *_title;
    NSImage *_image;
    NSString *_comment;
    NSDate *_creationDate;
    NSDate *_modificationDate;
    NSDate *_lastVisitedDate;
}

-(id)init;
-(id)initWithURL:(NSURL *)url title:(NSString *)title;
-(id)initWithURL:(NSURL *)url title:(NSString *)title image:(NSImage *)image;
-(id)initWithURL:(NSURL *)url title:(NSString *)title image:(NSImage *)image comment:(NSString *)comment;

-(NSURL *)url;
-(NSString *)title;
-(NSImage *)image;
-(NSString *)comment;
-(NSDate *)creationDate;
-(NSDate *)modificationDate;
-(NSDate *)lastVisitedDate;

-(void)setURL:(NSURL *)url;
-(void)setTitle:(NSString *)title;
-(void)setImage:(NSImage *)image;
-(void)setComment:(NSString *)comment;
-(void)setModificationDate:(NSDate *)date;
-(void)setLastVisitedDate:(NSDate *)date;

-(unsigned)hash;
-(BOOL)isEqual:(id)anObject;

@end

