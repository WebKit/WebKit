/*	WCURIEntry.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>


@protocol WCURIEntry

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

#if defined(__cplusplus)
extern "C" {
#endif

// *** Factory method for WCURIEntry objects

id <WCURIEntry> WCCreateURIEntry(); 

#if defined(__cplusplus)
} // extern "C"
#endif
