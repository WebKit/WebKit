/*
 * Copyright (C) 2005-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebHistoryInternal.h"

#import "HistoryPropertyList.h"
#import "WebHistoryItemInternal.h"
#import "WebKitLogging.h"
#import "WebNSURLExtras.h"
#import "WebVisitedLinkStore.h"
#import <WebCore/HistoryItem.h>
#import <pal/spi/cocoa/NSCalendarDateSPI.h>

#if PLATFORM(IOS_FAMILY)
#import <WebCore/WebCoreThreadMessage.h>
#endif

using namespace WebCore;

typedef int64_t WebHistoryDateKey;
typedef HashMap<WebHistoryDateKey, RetainPtr<NSMutableArray>> DateToEntriesMap;

NSString *WebHistoryItemsAddedNotification = @"WebHistoryItemsAddedNotification";
NSString *WebHistoryItemsRemovedNotification = @"WebHistoryItemsRemovedNotification";
NSString *WebHistoryAllItemsRemovedNotification = @"WebHistoryAllItemsRemovedNotification";
NSString *WebHistoryLoadedNotification = @"WebHistoryLoadedNotification";
NSString *WebHistoryItemsDiscardedWhileLoadingNotification = @"WebHistoryItemsDiscardedWhileLoadingNotification";
NSString *WebHistorySavedNotification = @"WebHistorySavedNotification";
NSString *WebHistoryItemsKey = @"WebHistoryItems";

static RetainPtr<WebHistory>& sharedHistory()
{
    static NeverDestroyed<RetainPtr<WebHistory>> _sharedHistory;
    return _sharedHistory;
}

NSString *FileVersionKey = @"WebHistoryFileVersion";
NSString *DatesArrayKey = @"WebHistoryDates";

#define currentFileVersion 1

class WebHistoryWriter : public HistoryPropertyListWriter {
public:
    WebHistoryWriter(DateToEntriesMap*);

private:
    virtual void writeHistoryItems(BinaryPropertyListObjectStream&);

    DateToEntriesMap* m_entriesByDate;
    Vector<int> m_dateKeys;
};

@interface WebHistory ()
- (void)_sendNotification:(NSString *)name entries:(NSArray *)entries;
@end

@interface WebHistoryPrivate : NSObject {
@private
    RetainPtr<NSMutableDictionary> _entriesByURL;
    std::unique_ptr<DateToEntriesMap> _entriesByDate;
    RetainPtr<NSMutableArray> _orderedLastVisitedDays;
    BOOL itemLimitSet;
    int itemLimit;
    BOOL ageInDaysLimitSet;
    int ageInDaysLimit;
}

- (WebHistoryItem *)visitedURL:(NSURL *)url withTitle:(NSString *)title;

- (BOOL)addItem:(WebHistoryItem *)entry discardDuplicate:(BOOL)discardDuplicate;
- (void)addItems:(NSArray *)newEntries;
- (BOOL)removeItem:(WebHistoryItem *)entry;
- (BOOL)removeItems:(NSArray *)entries;
- (BOOL)removeAllItems;
- (void)rebuildHistoryByDayIfNeeded:(WebHistory *)webHistory;

- (NSArray *)orderedLastVisitedDays;
- (BOOL)containsURL:(NSURL *)URL;
- (WebHistoryItem *)itemForURL:(NSURL *)URL;
- (WebHistoryItem *)itemForURLString:(NSString *)URLString;
- (NSArray *)allItems;

- (BOOL)loadFromURL:(NSURL *)URL collectDiscardedItemsInto:(NSMutableArray *)discardedItems error:(NSError **)error;
- (BOOL)saveToURL:(NSURL *)URL error:(NSError **)error;

- (void)setHistoryItemLimit:(int)limit;
- (int)historyItemLimit;
- (void)setHistoryAgeInDaysLimit:(int)limit;
- (int)historyAgeInDaysLimit;

@end

@implementation WebHistoryPrivate

// MARK: OBJECT FRAMEWORK

+ (void)initialize
{
    [[NSUserDefaults standardUserDefaults] registerDefaults:@{
        @"WebKitHistoryItemLimit": @"1000",
        @"WebKitHistoryAgeInDaysLimit": @"7",
    }];
}

- (id)init
{
    self = [super init];
    if (!self)
        return nil;
    
    _entriesByURL = adoptNS([[NSMutableDictionary alloc] init]);
    _entriesByDate = makeUnique<DateToEntriesMap>();

    return self;
}

// MARK: MODIFYING CONTENTS

static void getDayBoundaries(NSTimeInterval interval, NSTimeInterval& beginningOfDay, NSTimeInterval& beginningOfNextDay)
{
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:interval];
    
    NSCalendar *calendar = [NSCalendar calendarWithIdentifier:NSCalendarIdentifierGregorian];
    
    NSDate *beginningOfDayDate = nil;
    NSTimeInterval dayLength;
    [calendar rangeOfUnit:NSCalendarUnitDay startDate:&beginningOfDayDate interval:&dayLength forDate:date];
    
    beginningOfDay = beginningOfDayDate.timeIntervalSinceReferenceDate;
    beginningOfNextDay = beginningOfDay + dayLength;
}

static inline NSTimeInterval beginningOfDay(NSTimeInterval date)
{
    static NSTimeInterval cachedBeginningOfDay = NAN;
    static NSTimeInterval cachedBeginningOfNextDay;
    if (!(date >= cachedBeginningOfDay && date < cachedBeginningOfNextDay))
        getDayBoundaries(date, cachedBeginningOfDay, cachedBeginningOfNextDay);
    return cachedBeginningOfDay;
}

static inline WebHistoryDateKey dateKey(NSTimeInterval date)
{
    // Converting from double (NSTimeInterval) to int64_t (WebHistoryDateKey) is
    // safe here because all sensible dates are in the range -2**48 .. 2**47 which
    // safely fits in an int64_t.
    return beginningOfDay(date);
}

// Returns whether the day is already in the list of days,
// and fills in *key with the key used to access its location
- (BOOL)findKey:(WebHistoryDateKey*)key forDay:(NSTimeInterval)date
{
    ASSERT_ARG(key, key);
    *key = dateKey(date);
    return _entriesByDate->contains(*key);
}

- (void)insertItem:(WebHistoryItem *)entry forDateKey:(WebHistoryDateKey)dateKey
{
    ASSERT_ARG(entry, entry != nil);
    ASSERT(_entriesByDate->contains(dateKey));

    NSMutableArray *entriesForDate = _entriesByDate->get(dateKey).get();
    NSTimeInterval entryDate = [entry lastVisitedTimeInterval];

    unsigned count = [entriesForDate count];

    // The entries for each day are stored in a sorted array with the most recent entry first
    // Check for the common cases of the entry being newer than all existing entries or the first entry of the day
    if (!count || [[entriesForDate objectAtIndex:0] lastVisitedTimeInterval] < entryDate) {
        [entriesForDate insertObject:entry atIndex:0];
        return;
    }
    // .. or older than all existing entries
    if (count > 0 && [[entriesForDate objectAtIndex:count - 1] lastVisitedTimeInterval] >= entryDate) {
        [entriesForDate insertObject:entry atIndex:count];
        return;
    }

    unsigned low = 0;
    unsigned high = count;
    while (low < high) {
        unsigned mid = low + (high - low) / 2;
        if ([[entriesForDate objectAtIndex:mid] lastVisitedTimeInterval] >= entryDate)
            low = mid + 1;
        else
            high = mid;
    }

    // low is now the index of the first entry that is older than entryDate
    [entriesForDate insertObject:entry atIndex:low];
}

- (BOOL)removeItemFromDateCaches:(WebHistoryItem *)entry
{
    WebHistoryDateKey dateKey;
    BOOL foundDate = [self findKey:&dateKey forDay:[entry lastVisitedTimeInterval]];
 
    if (!foundDate)
        return NO;

    DateToEntriesMap::iterator it = _entriesByDate->find(dateKey);
    NSMutableArray *entriesForDate = it->value.get();
    [entriesForDate removeObjectIdenticalTo:entry];
    
    // remove this date entirely if there are no other entries on it
    if ([entriesForDate count] == 0) {
        _entriesByDate->remove(it);
        // Clear _orderedLastVisitedDays so it will be regenerated when next requested.
        _orderedLastVisitedDays = nil;
    }
    
    return YES;
}

- (BOOL)removeItemForURLString:(NSString *)URLString
{
    WebHistoryItem *entry = [_entriesByURL objectForKey:URLString];
    if (!entry)
        return NO;

    [_entriesByURL removeObjectForKey:URLString];
    
#if !ASSERT_ENABLED
    [self removeItemFromDateCaches:entry];
#else
    BOOL itemWasInDateCaches = [self removeItemFromDateCaches:entry];
    ASSERT(itemWasInDateCaches);
#endif

    if (![_entriesByURL count])
        WebVisitedLinkStore::removeAllVisitedLinks();

    return YES;
}

- (void)addItemToDateCaches:(WebHistoryItem *)entry
{
    WebHistoryDateKey dateKey;
    if ([self findKey:&dateKey forDay:[entry lastVisitedTimeInterval]])
        // other entries already exist for this date
        [self insertItem:entry forDateKey:dateKey];
    else {
        // no other entries exist for this date
        auto entries = adoptNS([[NSMutableArray alloc] initWithObjects:&entry count:1]);
        _entriesByDate->set(dateKey, entries.get());
        // Clear _orderedLastVisitedDays so it will be regenerated when next requested.
        _orderedLastVisitedDays = nil;
    }
}

- (WebHistoryItem *)visitedURL:(NSURL *)url withTitle:(NSString *)title
{
    ASSERT(url);
    ASSERT(title);
    
    NSString *URLString = [url _web_originalDataAsString];
    if (!URLString)
        URLString = @"";
    auto entry = retainPtr([_entriesByURL objectForKey:URLString]);

    if (entry) {
        LOG(History, "Updating global history entry %@", entry.get());
        // Remove the item from date caches before changing its last visited date.  Otherwise we might get duplicate entries
        // as seen in <rdar://problem/6570573>.
        BOOL itemWasInDateCaches = [self removeItemFromDateCaches:entry.get()];
        ASSERT_UNUSED(itemWasInDateCaches, itemWasInDateCaches);

        [entry _visitedWithTitle:title];
    } else {
        LOG(History, "Adding new global history entry for %@", url);
        entry = adoptNS([[WebHistoryItem alloc] initWithURLString:URLString title:title lastVisitedTimeInterval:[NSDate timeIntervalSinceReferenceDate]]);
        [_entriesByURL setObject:entry.get() forKey:URLString];
    }
    
    [self addItemToDateCaches:entry.get()];

    return entry.autorelease();
}

- (BOOL)addItem:(WebHistoryItem *)entry discardDuplicate:(BOOL)discardDuplicate
{
    ASSERT_ARG(entry, entry);
    ASSERT_ARG(entry, [entry lastVisitedTimeInterval] != 0);

    NSString *URLString = [entry URLString];

#if !PLATFORM(IOS_FAMILY)
    if (auto oldEntry = retainPtr([_entriesByURL objectForKey:URLString])) {
        if (discardDuplicate)
            return NO;

        [self removeItemForURLString:URLString];
    }

    [self addItemToDateCaches:entry];
    [_entriesByURL setObject:entry forKey:URLString];
#else
    if (auto otherEntry = retainPtr([_entriesByURL objectForKey:URLString])) {
        if (discardDuplicate)
            return NO;

        if ([otherEntry lastVisitedTimeInterval] < [entry lastVisitedTimeInterval]) {
            [self removeItemForURLString:URLString];
            [self addItemToDateCaches:entry];
            [_entriesByURL setObject:entry forKey:URLString];
        } else
            return NO; // Special case for merges when new items may be older than pre-existing entries.
    } else {
        [self addItemToDateCaches:entry];
        [_entriesByURL setObject:entry forKey:URLString];
    }
#endif
    
    return YES;
}

- (void)rebuildHistoryByDayIfNeeded:(WebHistory *)webHistory
{
    // We clear all the values to present a consistent state when sending the notifications.
    // We keep a reference to the entries for rebuilding the history after the notification.
    auto entryArrays = copyToVector(_entriesByDate->values());
    _entriesByDate->clear();
    
    auto entriesByURL = std::exchange(_entriesByURL, nil);
    
    _orderedLastVisitedDays = nil;
    
    NSArray *allEntries = [entriesByURL allValues];
    [webHistory _sendNotification:WebHistoryAllItemsRemovedNotification entries:allEntries];
    
    // Next, we rebuild the history, restore the states, and notify the clients.
    _entriesByURL = entriesByURL;
    for (size_t dayIndex = 0; dayIndex < entryArrays.size(); ++dayIndex) {
        for (WebHistoryItem *entry in (entryArrays[dayIndex]).get())
            [self addItemToDateCaches:entry];
    }
    [webHistory _sendNotification:WebHistoryItemsAddedNotification entries:allEntries];
}

- (BOOL)removeItem:(WebHistoryItem *)entry
{
    NSString *URLString = [entry URLString];

    // If this exact object isn't stored, then make no change.
    // FIXME: Is this the right behavior if this entry isn't present, but another entry for the same URL is?
    // Maybe need to change the API to make something like removeEntryForURLString public instead.
    WebHistoryItem *matchingEntry = [_entriesByURL objectForKey:URLString];
    if (matchingEntry != entry)
        return NO;

    [self removeItemForURLString:URLString];

    return YES;
}

- (BOOL)removeItems:(NSArray *)entries
{
    NSUInteger count = [entries count];
    if (!count)
        return NO;

    for (NSUInteger index = 0; index < count; ++index)
        [self removeItem:[entries objectAtIndex:index]];
    
    return YES;
}

- (BOOL)removeAllItems
{
    if (_entriesByDate->isEmpty())
        return NO;

    _entriesByDate->clear();
    [_entriesByURL removeAllObjects];

    // Clear _orderedLastVisitedDays so it will be regenerated when next requested.
    _orderedLastVisitedDays = nil;

    WebVisitedLinkStore::removeAllVisitedLinks();

    return YES;
}

- (void)addItems:(NSArray *)newEntries
{
    // There is no guarantee that the incoming entries are in any particular
    // order, but if this is called with a set of entries that were created by
    // iterating through the results of orderedLastVisitedDays and orderedItemsLastVisitedOnDayy
    // then they will be ordered chronologically from newest to oldest. We can make adding them
    // faster (fewer compares) by inserting them from oldest to newest.
    NSEnumerator *enumerator = [newEntries reverseObjectEnumerator];
    while (WebHistoryItem *entry = [enumerator nextObject])
        [self addItem:entry discardDuplicate:NO];
}

// MARK: DATE-BASED RETRIEVAL

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

- (NSArray *)orderedLastVisitedDays
{
    if (!_orderedLastVisitedDays) {
        Vector<int> daysAsTimeIntervals;
        daysAsTimeIntervals.reserveCapacity(_entriesByDate->size());
        DateToEntriesMap::const_iterator end = _entriesByDate->end();
        for (DateToEntriesMap::const_iterator it = _entriesByDate->begin(); it != end; ++it)
            daysAsTimeIntervals.append(it->key);

        std::sort(daysAsTimeIntervals.begin(), daysAsTimeIntervals.end());
        size_t count = daysAsTimeIntervals.size();
        _orderedLastVisitedDays = adoptNS([[NSMutableArray alloc] initWithCapacity:count]);
        for (int i = count - 1; i >= 0; i--) {
            NSTimeInterval interval = daysAsTimeIntervals[i];
            auto date = adoptNS([[NSCalendarDate alloc] initWithTimeIntervalSinceReferenceDate:interval]);
            [_orderedLastVisitedDays addObject:date.get()];
        }
    }
    return _orderedLastVisitedDays.get();
}

- (NSArray *)orderedItemsLastVisitedOnDay:(NSCalendarDate *)date
{
    WebHistoryDateKey dateKey;
    if (![self findKey:&dateKey forDay:[date timeIntervalSinceReferenceDate]])
        return nil;
    return _entriesByDate->get(dateKey).get();
}

ALLOW_DEPRECATED_DECLARATIONS_END

// MARK: URL MATCHING

- (WebHistoryItem *)itemForURLString:(NSString *)URLString
{
    return [_entriesByURL objectForKey:URLString];
}

- (BOOL)containsURL:(NSURL *)URL
{
    return [self itemForURLString:[URL _web_originalDataAsString]] != nil;
}

- (WebHistoryItem *)itemForURL:(NSURL *)URL
{
    return [self itemForURLString:[URL _web_originalDataAsString]];
}

- (NSArray *)allItems
{
    return [_entriesByURL allValues];
}

// MARK: ARCHIVING/UNARCHIVING

- (void)setHistoryAgeInDaysLimit:(int)limit
{
    ageInDaysLimitSet = YES;
    ageInDaysLimit = limit;
}

- (int)historyAgeInDaysLimit
{
    if (ageInDaysLimitSet)
        return ageInDaysLimit;
    return [[NSUserDefaults standardUserDefaults] integerForKey:@"WebKitHistoryAgeInDaysLimit"];
}

- (void)setHistoryItemLimit:(int)limit
{
    itemLimitSet = YES;
    itemLimit = limit;
}

- (int)historyItemLimit
{
    if (itemLimitSet)
        return itemLimit;
    return [[NSUserDefaults standardUserDefaults] integerForKey:@"WebKitHistoryItemLimit"];
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

// Return a date that marks the age limit for history entries saved to or
// loaded from disk. Any entry older than this item should be rejected.
- (NSCalendarDate *)ageLimitDate
{
    return [[NSCalendarDate calendarDate] dateByAddingYears:0 months:0 days:-[self historyAgeInDaysLimit]
                                                      hours:0 minutes:0 seconds:0];
}

ALLOW_DEPRECATED_DECLARATIONS_END

- (BOOL)loadHistoryGutsFromURL:(NSURL *)URL savedItemsCount:(int *)numberOfItemsLoaded collectDiscardedItemsInto:(NSMutableArray *)discardedItems error:(NSError **)error
{
    *numberOfItemsLoaded = 0;
    NSDictionary *dictionary = nil;

    // Optimize loading from local file, which is faster than using the general URL loading mechanism
    if ([URL isFileURL]) {
        dictionary = [NSDictionary dictionaryWithContentsOfFile:[URL path]];
        if (!dictionary) {
#if !LOG_DISABLED
            if ([[NSFileManager defaultManager] fileExistsAtPath:[URL path]])
                LOG_ERROR("unable to read history from file %@; perhaps contents are corrupted", [URL path]);
#endif
            // else file doesn't exist, which is normal the first time
            return NO;
        }
    } else {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        NSData *data = [NSURLConnection sendSynchronousRequest:[NSURLRequest requestWithURL:URL] returningResponse:nil error:error];
        ALLOW_DEPRECATED_DECLARATIONS_END
        if (data.length)
            dictionary = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:nullptr error:nullptr];
    }

    // We used to support NSArrays here, but that was before Safari 1.0 shipped. We will no longer support
    // that ancient format, so anything that isn't an NSDictionary is bogus.
    if (![dictionary isKindOfClass:[NSDictionary class]])
        return NO;

    NSNumber *fileVersionObject = [dictionary objectForKey:FileVersionKey];
    int fileVersion;
    // we don't trust data obtained from elsewhere, so double-check
    if (!fileVersionObject || ![fileVersionObject isKindOfClass:[NSNumber class]]) {
        LOG_ERROR("history file version can't be determined, therefore not loading");
        return NO;
    }
    fileVersion = [fileVersionObject intValue];
    if (fileVersion > currentFileVersion) {
        LOG_ERROR("history file version is %d, newer than newest known version %d, therefore not loading", fileVersion, currentFileVersion);
        return NO;
    }    

    NSArray *array = [dictionary objectForKey:DatesArrayKey];

    int itemCountLimit = [self historyItemLimit];
    NSTimeInterval ageLimitDate = [[self ageLimitDate] timeIntervalSinceReferenceDate];
    BOOL ageLimitPassed = NO;
    BOOL itemLimitPassed = NO;
    ASSERT(*numberOfItemsLoaded == 0);

    for (NSDictionary *itemAsDictionary in array) {
        @autoreleasepool {
            auto item = adoptNS([[WebHistoryItem alloc] initFromDictionaryRepresentation:itemAsDictionary]);

            // item without URL is useless; data on disk must have been bad; ignore
            if ([item URLString]) {
                // Test against date limit. Since the items are ordered newest to oldest, we can stop comparing
                // once we've found the first item that's too old.
                if (!ageLimitPassed && [item lastVisitedTimeInterval] <= ageLimitDate)
                    ageLimitPassed = YES;

                if (ageLimitPassed || itemLimitPassed)
                    [discardedItems addObject:item.get()];
                else {
                    if ([self addItem:item.get() discardDuplicate:YES])
                        ++(*numberOfItemsLoaded);
                    if (*numberOfItemsLoaded == itemCountLimit)
                        itemLimitPassed = YES;
                }
            }
        }
    }

    return YES;
}

- (BOOL)loadFromURL:(NSURL *)URL collectDiscardedItemsInto:(NSMutableArray *)discardedItems error:(NSError **)error
{
#if !LOG_DISABLED
    double start = CFAbsoluteTimeGetCurrent();
#endif

    int numberOfItems;
    if (![self loadHistoryGutsFromURL:URL savedItemsCount:&numberOfItems collectDiscardedItemsInto:discardedItems error:error])
        return NO;

#if !LOG_DISABLED
    double duration = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "loading %d history entries from %@ took %f seconds", numberOfItems, URL, duration);
#endif

    return YES;
}

- (NSData *)data
{
    if (_entriesByDate->isEmpty()) {
        static NSData *emptyHistoryData = [[NSData alloc] init];
        return emptyHistoryData;
    }

    // Ignores the date and item count limits; these are respected when loading instead of when saving, so
    // that clients can learn of discarded items by listening to WebHistoryItemsDiscardedWhileLoadingNotification.
    WebHistoryWriter writer(_entriesByDate.get());
    writer.writePropertyList();
    return retainPtr((NSData *)writer.releaseData().get()).autorelease();
}

- (BOOL)saveToURL:(NSURL *)URL error:(NSError **)error
{
#if !LOG_DISABLED
    double start = CFAbsoluteTimeGetCurrent();
#endif

    BOOL result = [[self data] writeToURL:URL options:0 error:error];

#if !LOG_DISABLED
    double duration = CFAbsoluteTimeGetCurrent() - start;
    LOG(Timing, "saving history to %@ took %f seconds", URL, duration);
#endif

    return result;
}

- (void)addVisitedLinksToVisitedLinkStore:(WebVisitedLinkStore&)visitedLinkStore
{
    for (NSString *urlString in _entriesByURL.get())
        visitedLinkStore.addVisitedLink(urlString);
}

@end

@implementation WebHistory

+ (WebHistory *)optionalSharedHistory
{
    return sharedHistory().get();
}

+ (void)setOptionalSharedHistory:(WebHistory *)history
{
    if (sharedHistory() == history)
        return;
    // FIXME: Need to think about multiple instances of WebHistory per application
    // and correct synchronization of history file between applications.
    sharedHistory() = history;

    WebVisitedLinkStore::setShouldTrackVisitedLinks(history);
    WebVisitedLinkStore::removeAllVisitedLinks();
}

- (void)timeZoneChanged:(NSNotification *)notification
{
    [_historyPrivate rebuildHistoryByDayIfNeeded:self];
}

- (id)init
{
    self = [super init];
    if (!self)
        return nil;
    _historyPrivate = [[WebHistoryPrivate alloc] init];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(timeZoneChanged:)
                                                 name:NSSystemTimeZoneDidChangeNotification
                                               object:nil];
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:NSSystemTimeZoneDidChangeNotification
                                                  object:nil];
    [_historyPrivate release];
    [super dealloc];
}

// MARK: MODIFYING CONTENTS

- (void)_sendNotification:(NSString *)name entries:(NSArray *)entries
{
    NSDictionary *userInfo = @{ WebHistoryItemsKey: entries };
#if PLATFORM(IOS_FAMILY)
    WebThreadPostNotification(name, self, userInfo);
#else    
    [[NSNotificationCenter defaultCenter] postNotificationName:name object:self userInfo:userInfo];
#endif
}

- (void)removeItems:(NSArray *)entries
{
    if ([_historyPrivate removeItems:entries]) {
        [self _sendNotification:WebHistoryItemsRemovedNotification
                        entries:entries];
    }
}

- (void)removeAllItems
{
    NSArray *entries = [_historyPrivate allItems];
    if ([_historyPrivate removeAllItems])
        [self _sendNotification:WebHistoryAllItemsRemovedNotification entries:entries];
}

- (void)addItems:(NSArray *)newEntries
{
    [_historyPrivate addItems:newEntries];
    [self _sendNotification:WebHistoryItemsAddedNotification
                    entries:newEntries];
}

// MARK: DATE-BASED RETRIEVAL

- (NSArray *)orderedLastVisitedDays
{
    return [_historyPrivate orderedLastVisitedDays];
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

- (NSArray *)orderedItemsLastVisitedOnDay:(NSCalendarDate *)date
{
    return [_historyPrivate orderedItemsLastVisitedOnDay:date];
}

ALLOW_DEPRECATED_DECLARATIONS_END

// MARK: URL MATCHING

- (BOOL)containsURL:(NSURL *)URL
{
    return [_historyPrivate containsURL:URL];
}

- (WebHistoryItem *)itemForURL:(NSURL *)URL
{
    return [_historyPrivate itemForURL:URL];
}

// MARK: SAVING TO DISK

- (BOOL)loadFromURL:(NSURL *)URL error:(NSError **)error
{
    auto discardedItems = adoptNS([[NSMutableArray alloc] init]);
    if (![_historyPrivate loadFromURL:URL collectDiscardedItemsInto:discardedItems.get() error:error])
        return NO;

#if PLATFORM(IOS_FAMILY)
    WebThreadPostNotification(WebHistoryLoadedNotification, self, nil);
#else        
    [[NSNotificationCenter defaultCenter]
        postNotificationName:WebHistoryLoadedNotification
                      object:self];
#endif

    if ([discardedItems count])
        [self _sendNotification:WebHistoryItemsDiscardedWhileLoadingNotification entries:discardedItems.get()];

    return YES;
}

- (BOOL)saveToURL:(NSURL *)URL error:(NSError **)error
{
    if (![_historyPrivate saveToURL:URL error:error])
        return NO;
#if PLATFORM(IOS_FAMILY)
    WebThreadPostNotification(WebHistorySavedNotification, self, nil);
#else        
    [[NSNotificationCenter defaultCenter]
        postNotificationName:WebHistorySavedNotification
                      object:self];
#endif
    return YES;
}

- (void)setHistoryItemLimit:(int)limit
{
    [_historyPrivate setHistoryItemLimit:limit];
}

- (int)historyItemLimit
{
    return [_historyPrivate historyItemLimit];
}

- (void)setHistoryAgeInDaysLimit:(int)limit
{
    [_historyPrivate setHistoryAgeInDaysLimit:limit];
}

- (int)historyAgeInDaysLimit
{
    return [_historyPrivate historyAgeInDaysLimit];
}

@end

@implementation WebHistory (WebPrivate)

- (WebHistoryItem *)_itemForURLString:(NSString *)URLString
{
    return [_historyPrivate itemForURLString:URLString];
}

- (NSArray *)allItems
{
    return [_historyPrivate allItems];
}

- (NSData *)_data
{
    return [_historyPrivate data];
}

+ (void)_setVisitedLinkTrackingEnabled:(BOOL)visitedLinkTrackingEnabled
{
    WebVisitedLinkStore::setShouldTrackVisitedLinks(visitedLinkTrackingEnabled);
}

+ (void)_removeAllVisitedLinks
{
    WebVisitedLinkStore::removeAllVisitedLinks();
}

@end

@implementation WebHistory (WebInternal)

- (void)_visitedURL:(NSURL *)url withTitle:(NSString *)title method:(NSString *)method wasFailure:(BOOL)wasFailure
{
    WebHistoryItem *entry = [_historyPrivate visitedURL:url withTitle:title];

    HistoryItem* item = core(entry);
    item->setLastVisitWasFailure(wasFailure);

    entry->_private->_redirectURLs = nullptr;

    [self _sendNotification:WebHistoryItemsAddedNotification entries:@[entry]];
}

- (void)_addVisitedLinksToVisitedLinkStore:(WebVisitedLinkStore &)visitedLinkStore
{
    [_historyPrivate addVisitedLinksToVisitedLinkStore:visitedLinkStore];
}
@end

WebHistoryWriter::WebHistoryWriter(DateToEntriesMap* entriesByDate)
    : m_entriesByDate(entriesByDate)
{
    m_dateKeys.reserveCapacity(m_entriesByDate->size());
    DateToEntriesMap::const_iterator end = m_entriesByDate->end();
    for (DateToEntriesMap::const_iterator it = m_entriesByDate->begin(); it != end; ++it)
        m_dateKeys.append(it->key);
    std::sort(m_dateKeys.begin(), m_dateKeys.end());
}

void WebHistoryWriter::writeHistoryItems(BinaryPropertyListObjectStream& stream)
{
    for (int dateIndex = m_dateKeys.size() - 1; dateIndex >= 0; dateIndex--) {
        NSArray *entries = m_entriesByDate->get(m_dateKeys[dateIndex]).get();
        NSUInteger entryCount = [entries count];
        for (NSUInteger entryIndex = 0; entryIndex < entryCount; ++entryIndex)
            writeHistoryItem(stream, [entries objectAtIndex:entryIndex]);
    }
}
