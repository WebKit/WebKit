/*
    WebHistoryItemPrivate.h
    Copyright 2001, 2002, Apple, Inc. All rights reserved.
 */
#import <Cocoa/Cocoa.h>

#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebHistoryItem.h>

@interface WebHistoryItem (WebPrivate)
- (void)_retainIconInDatabase:(BOOL)retain;
+ (void)_releaseAllPendingPageCaches;
- (BOOL)hasPageCache;
- (void)setHasPageCache: (BOOL)f;
- (NSMutableDictionary *)pageCache;

+ (WebHistoryItem *)entryWithURL:(NSURL *)URL;

- (id)initWithURL:(NSURL *)URL title:(NSString *)title;
- (id)initWithURL:(NSURL *)URL target:(NSString *)target parent:(NSString *)parent title:(NSString *)title;

- (NSDictionary *)dictionaryRepresentation;
- (id)initFromDictionaryRepresentation:(NSDictionary *)dict;

- (NSString *)parent;
- (NSURL *)URL;
- (NSString *)target;
- (NSPoint)scrollPoint;
- (NSArray *)documentState;
- (BOOL)isTargetItem;
- (NSData *)formData;
- (NSString *)formContentType;
- (NSString *)formReferrer;
- (int)visitCount;

- (void)_mergeAutoCompleteHints:(WebHistoryItem *)otherItem;

- (void)setURL:(NSURL *)URL;
- (void)setOriginalURLString:(NSString *)URL;
- (void)setTarget:(NSString *)target;
- (void)setParent:(NSString *)parent;
- (void)setTitle:(NSString *)title;
- (void)setLastVisitedDate:(NSCalendarDate *)date;
- (void)setScrollPoint:(NSPoint)p;
- (void)setDocumentState:(NSArray *)state;
- (void)setIsTargetItem:(BOOL)flag;
- (void)setAnchor:(NSString *)anchor;
- (void)setFormData:(NSData *)data;
- (void)setFormContentType:(NSString *)type;
- (void)setFormReferrer:(NSString *)referrer;
- (void)setVisitCount:(int)count;

- (NSArray *)children;
- (void)addChildItem:(WebHistoryItem *)item;
- (WebHistoryItem *)childItemWithName:(NSString *)name;
- (WebHistoryItem *)targetItem;

- (void)setAlwaysAttemptToUsePageCache: (BOOL)flag;
- (BOOL)alwaysAttemptToUsePageCache;

@end

@interface WebBackForwardList (WebPrivate)
- (void)_clearPageCache;
- (void)_setUsesPageCache: (BOOL)f;
- (BOOL)_usesPageCache;
@end

