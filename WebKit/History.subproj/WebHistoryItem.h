/*	
    WebHistoryItem.h
    Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class NSURL;

/*!
    @class WebHistoryItem
*/
@interface WebHistoryItem : NSObject
{
    NSString *_URLString;
    NSString *_originalURLString;
    NSString *_target;
    NSString *_parent;
    NSString *_title;
    NSString *_displayTitle;
    NSCalendarDate *_lastVisitedDate;
    NSPoint _scrollPoint;
    NSString *anchor;
    NSArray *_documentState;
    NSMutableArray *_subItems;
    NSMutableDictionary *pageCache;
    BOOL _isTargetItem;
    BOOL _alwaysAttemptToUsePageCache;
    // info used to repost form data
    NSData *_formData;
    NSString *_formContentType;
    NSString *_formReferrer;    
}

+ (WebHistoryItem *)entryWithURL:(NSURL *)URL;

- (id)initWithURL:(NSURL *)URL title:(NSString *)title;
- (id)initWithURL:(NSURL *)URL target:(NSString *)target parent:(NSString *)parent title:(NSString *)title;

- (NSDictionary *)dictionaryRepresentation;
- (id)initFromDictionaryRepresentation:(NSDictionary *)dict;

- (NSURL *)URL;
- (NSString *)URLString;
- (NSString *)originalURLString;
- (NSString *)target;
- (NSString *)parent;
- (NSString *)title;
- (NSString *)displayTitle;
- (NSImage *)icon;
- (NSCalendarDate *)lastVisitedDate;
- (NSPoint)scrollPoint;
- (NSArray *)documentState;
- (BOOL)isTargetItem;
- (NSString *)anchor;
- (NSData *)formData;
- (NSString *)formContentType;
- (NSString *)formReferrer;    

- (void)setURL:(NSURL *)URL;
- (void)setOriginalURLString:(NSString *)URL;
- (void)setTarget:(NSString *)target;
- (void)setParent:(NSString *)parent;
- (void)setTitle:(NSString *)title;
- (void)setDisplayTitle:(NSString *)displayTitle;
- (void)setLastVisitedDate:(NSCalendarDate *)date;
- (void)setScrollPoint:(NSPoint)p;
- (void)setDocumentState:(NSArray *)state;
- (void)setIsTargetItem:(BOOL)flag;
- (void)setAnchor:(NSString *)anchor;
- (void)setFormData:(NSData *)data;
- (void)setFormContentType:(NSString *)type;
- (void)setFormReferrer:(NSString *)referrer;    

- (NSArray *)children;
- (void)addChildItem:(WebHistoryItem *)item;
- (WebHistoryItem *)childItemWithName:(NSString *)name;
- (WebHistoryItem *)targetItem;

- (void)setAlwaysAttemptToUsePageCache: (BOOL)flag;
- (BOOL)alwaysAttemptToUsePageCache;

@end
