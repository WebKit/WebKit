/*	
    WebHistoryItem.h
    Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@interface WebHistoryItem : NSObject
{
    NSURL *_URL;
    NSString *_target;
    NSString *_parent;
    NSString *_title;
    NSString *_displayTitle;
    NSImage *_image;
    NSCalendarDate *_lastVisitedDate;
    NSPoint _scrollPoint;
    NSString *anchor;
}

+(WebHistoryItem *)entryWithURL:(NSURL *)URL;

- (id)init;
- (id)initWithURL:(NSURL *)URL title:(NSString *)title;
- (id)initWithURL:(NSURL *)URL title:(NSString *)title image:(NSImage *)image;
- (id)initWithURL:(NSURL *)URL target: (NSString *)target parent: (NSString *)parent title:(NSString *)title image:(NSImage *)image;

- (NSDictionary *)dictionaryRepresentation;
- (id)initFromDictionaryRepresentation:(NSDictionary *)dict;

- (NSURL *)URL;
- (NSString *)target;
- (NSString *)parent;
- (NSString *)title;
- (NSString *)displayTitle;
- (NSImage *)image;
- (NSCalendarDate *)lastVisitedDate;

- (void)setURL:(NSURL *)URL;
- (void)setTarget:(NSString *)target;
- (void)setParent:(NSString *)parent;
- (void)setTitle:(NSString *)title;
- (void)setDisplayTitle:(NSString *)displayTitle;
- (void)setImage:(NSImage *)image;
- (void)setLastVisitedDate:(NSCalendarDate *)date;
- (void)setScrollPoint: (NSPoint)p;
- (NSPoint)scrollPoint;
- (unsigned)hash;
- (BOOL)isEqual:(id)anObject;
- (NSString *)anchor;
- (void)setAnchor: (NSString *)anchor;

@end
