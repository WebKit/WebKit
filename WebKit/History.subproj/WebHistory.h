/*	
        WebHistory.h
	Copyright 2001, 2002, Apple Computer, Inc.

        Public header file.
        
        FIXME  Strip down this API.
*/
#import <Foundation/Foundation.h>

// FIXME  Cannot inherit from WebCoreHistory
#import <WebCore/WebCoreHistory.h>

@class WebHistoryItem;
@class WebHistoryPrivate;

// Notifications sent when history is modified.
// The first two come with a userInfo dictionary with a single key "Entries", which contains
// an array of entries that were added or removed.

// posted from addEntry: and addEntries:
extern NSString *WebHistoryEntriesAddedNotification;
// posted from removeEntry: and removeEntries:
extern NSString *WebHistoryEntriesRemovedNotification;
// posted from removeAllEntries
extern NSString *WebHistoryAllEntriesRemovedNotification;
// posted from loadHistory
extern NSString *WebHistoryLoadedNotification;

/*!
    @class WebHistory
    @discussion WebHistory is used to track pages that have been loaded
    by WebKit.
*/
@interface WebHistory : WebCoreHistory {
@private
    WebHistoryPrivate *_historyPrivate;
}

+ (WebHistory *)sharedHistory;

/*!
    @method webHistoryWithFile:
    @param file The file to use to initialize the WebHistory.
    @result Returns a WebHistory initialized with the contents of file.
*/
+ (WebHistory *)webHistoryWithFile: (NSString *)file;

/*!
    @method initWithFile:
    @abstract The designated initializer for WebHistory.
    @result Returns an initialized WebHistory.
*/
- initWithFile: (NSString *)file;

/*!
    @method addEntry:
    @param entry
*/
- (void)addEntry: (WebHistoryItem *)entry;

/*!
    @method addEntryForURLString:
    @param URL
    @result Newly created WebHistoryItem
*/
- (WebHistoryItem *)addEntryForURL: (NSURL *)URL;

/*!
    @method addEntries:
    @param newEntries
*/
- (void)addEntries:(NSArray *)newEntries;

/*!
    @method removeEntry:
    @param entry
*/
- (void)removeEntry: (WebHistoryItem *)entry;

/*!
    @method removeEntries:
    @param entries
*/
- (void)removeEntries: (NSArray *)entries;

/*!
    @method removeAllEntries
*/
- (void)removeAllEntries;

/*!
    @method orderedLastVisitedDays
    @discussion Get an array of NSCalendarDate, each one representing a unique day that contains one
    or more history entries, ordered from most recent to oldest.
    @result Returns an array of WebHistoryItems
*/
- (NSArray *)orderedLastVisitedDays;

/*!
    @method orderedEntriesLastVisitedOnDay:
    @discussion Get an array of WebHistoryItem that were last visited on the day represented by the
    specified NSCalendarDate, ordered from most recent to oldest.
    @param calendarDate
    @result Returns an array of WebHistoryItems
*/
- (NSArray *)orderedEntriesLastVisitedOnDay: (NSCalendarDate *)calendarDate;

/*!
    @method containsURL:
    @param URL
    @discussion testing contents for visited-link mechanism
*/
- (BOOL)containsURL: (NSURL *)URL;

/*!
    @method entryForURL:
    @discussion Get an item for a specific URL
    @param URL
    @result Returns an item matching the URL
*/
- (WebHistoryItem *)entryForURL:(NSURL *)URL;

/*!
    @method file
    @discussion The file path used for storing history, specified in -[WebHistory initWithFile:] or +[WebHistory webHistoryWithFile:]
    @result Returns the file path used to store the history.
*/
- (NSString *)file;

/*!
    @method loadHistory
    @discussion Load history from file. This happens automatically at init time, and need not normally be called.
    @result Returns YES if successful, not otherwise.
*/
- (BOOL)loadHistory;

/*!
    @method saveHistory
    @discussion Save history to file. It is the client's responsibility to call this at appropriate times.
    @result Returns YES if successful, not otherwise.
*/
- (BOOL)saveHistory;

@end
