/*	
        WebHistory.h
	Copyright 2001, 2002, Apple Computer, Inc.

        Public header file.
*/
#import <Foundation/Foundation.h>

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
@interface WebHistory : NSObject {
@private
    WebHistoryPrivate *_historyPrivate;
}

/*!
    @method sharedHistory
    @abstract Returns a shared WebHistory instance initialized with the default history file.
    @result A WebHistory object.
*/
+ (WebHistory *)sharedHistory;

/*!
    @method createSharedHistoryWithFile:
    @param file The file to use to initialize the WebHistory.
    @result Returns a WebHistory initialized with the contents of file.
*/
+ (WebHistory *)createSharedHistoryWithFile: (NSString*)file;

/*!
    @method initWithFile:
    @abstract The designated initializer for WebHistory.
    @result Returns an initialized WebHistory.
*/
- initWithFile: (NSString *)file;

/*!
    @method addEntry:
    @param entry The history item to add to the WebHistory.
*/
- (void)addEntry: (WebHistoryItem *)entry;

/*!
    @method addEntries:
    @param newEntries An array of WebHistoryItems to add to the WebHistory.
*/
- (void)addEntries:(NSArray *)newEntries;

/*!
    @method removeEntry:
    @param entry The WebHistoryItem to remove from the WebHistory.
*/
- (void)removeEntry: (WebHistoryItem *)entry;

/*!
    @method removeEntries:
    @param entries An array of WebHistoryItems to remove from the WebHistory.
*/
- (void)removeEntries: (NSArray *)entries;

/*!
    @method removeAllEntries
*/
- (void)removeAllEntries;

/*!
    @method orderedLastVisitedDays
    @discussion Get an array of NSCalendarDates, each one representing a unique day that contains one
    or more history entries, ordered from most recent to oldest.
    @result Returns an array of NSCalendarDates for which history items exist in the WebHistory.
*/
- (NSArray *)orderedLastVisitedDays;

/*!
    @method orderedEntriesLastVisitedOnDay:
    @discussion Get an array of WebHistoryItem that were last visited on the day represented by the
    specified NSCalendarDate, ordered from most recent to oldest.
    @param calendarDate A date identifying the unique day of interest.
    @result Returns an array of WebHistoryItems last visited on the indicated day.
*/
- (NSArray *)orderedEntriesLastVisitedOnDay: (NSCalendarDate *)calendarDate;

/*!
    @method containsURL:
    @abstract Return whether a URL is in the WebHistory.
    @param URL The URL for which to search the WebHistory.
    @discussion This method is useful for implementing a visited-link mechanism.
    @result YES if WebHistory contains a history item for the given URL, otherwise NO.
*/
- (BOOL)containsURL: (NSURL *)URL;

/*!
    @method entryForURL:
    @abstract Get an item for a specific URL
    @param URL The URL of the history item to search for
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
    @result Returns YES if successful, NO otherwise.
*/
- (BOOL)loadHistory;

/*!
    @method saveHistory
    @discussion Save history to file. It is the client's responsibility to call this at appropriate times.
    @result Returns YES if successful, NO otherwise.
*/
- (BOOL)saveHistory;

@end
