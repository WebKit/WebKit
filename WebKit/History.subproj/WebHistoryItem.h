/*	
    WebHistoryItem.h
    Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@interface WebHistoryItem : NSObject
{
    NSURL *_url;
    NSString *_target;
    NSString *_title;
    NSString *_displayTitle;
    NSImage *_image;
    NSCalendarDate *_lastVisitedDate;
    NSPoint _scrollPoint;
}

-(id)init;
-(id)initWithURL:(NSURL *)url title:(NSString *)title;
-(id)initWithURL:(NSURL *)url title:(NSString *)title image:(NSImage *)image;
-(id)initWithURL:(NSURL *)url target: (NSString *)target title:(NSString *)title image:(NSImage *)image;

-(NSDictionary *)dictionaryRepresentation;
-(id)initFromDictionaryRepresentation:(NSDictionary *)dict;

-(NSURL *)url;
-(NSString *)target;
-(NSString *)title;
-(NSString *)displayTitle;
-(NSImage *)image;
-(NSCalendarDate *)lastVisitedDate;

-(void)setURL:(NSURL *)url;
-(void)setTarget:(NSString *)target;
-(void)setTitle:(NSString *)title;
-(void)setDisplayTitle:(NSString *)displayTitle;
-(void)setImage:(NSImage *)image;
-(void)setLastVisitedDate:(NSCalendarDate *)date;
-(void)setScrollPoint: (NSPoint)p;
- (NSPoint)scrollPoint;
-(unsigned)hash;
-(BOOL)isEqual:(id)anObject;

@end
