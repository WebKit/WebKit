/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#import "WebHistory.h"
#import "WebHistoryPrivate.h"

#import "WebHistoryItem.h"
#import "WebHistoryItemInternal.h"
#import "WebHistoryItemPrivate.h"
#import "WebKitLogging.h"
#import "WebNSURLExtras.h"
#import <Foundation/NSError.h>
#import <JavaScriptCore/Assertions.h>
#import <WebCore/WebCoreHistory.h>
#import <wtf/Vector.h>


NSString *WebHistoryItemsAddedNotification = @"WebHistoryItemsAddedNotification";
NSString *WebHistoryItemsRemovedNotification = @"WebHistoryItemsRemovedNotification";
NSString *WebHistoryAllItemsRemovedNotification = @"WebHistoryAllItemsRemovedNotification";
NSString *WebHistoryLoadedNotification = @"WebHistoryLoadedNotification";
NSString *WebHistoryItemsDiscardedWhileLoadingNotification = @"WebHistoryItemsDiscardedWhileLoadingNotification";
NSString *WebHistorySavedNotification = @"WebHistorySavedNotification";
NSString *WebHistoryItemsKey = @"WebHistoryItems";

static WebHistory *_sharedHistory = nil;



NSString *FileVersionKey = @"WebHistoryFileVersion";
NSString *DatesArrayKey = @"WebHistoryDates";

#define currentFileVersion 1

@implementation WebHistoryPrivate

#pragma mark OBJECT FRAMEWORK

+ (void)initialize
{
    [[NSUserDefaults standardUserDefaults] registerDefaults:
        [NSDictionary dictionaryWithObjectsAndKeys:
            @"1000", @"WebKitHistoryItemLimit",
            @"7", @"WebKitHistoryAgeInDaysLimit",
            nil]];    
}

- (id)init
{
    if (![super init]) {
        return nil;
    }
    
    _entriesByURL = [[NSMutableDictionary alloc] init];
    _entriesByDate = new DateToEntriesMap;

    return self;
}

- (void)dealloc
{
    [_entriesByURL release];
    [_orderedLastVisitedDays release];
    delete _entriesByDate;
    
    [super dealloc];
}

- (void)finalize
{
    delete _entriesByDate;
    [super finalize];
}

#pragma mark MODIFYING CONTENTS

WebHistoryDateKey timeIntervalForBeginningOfDay(NSTimeInterval interval)
{
    CFTimeZoneRef timeZone = CFTimeZoneCopyDefault();
    CFGregorianDate date = CFAbsoluteTimeGetGregorianDate(interval, timeZone);
    date.hour = 0;
    date.minute = 0;
    date.second = 0;
    NSTimeInterval result = CFGregorianDateGetAbsoluteTime(date, timeZone);
    CFRelease(timeZone);

    // Converting from double to int64_t is safe here as NSDate's useful range
    // is -2**48 .. 2**47 which will safely fit in an int64_t.
    return (WebHistoryDateKey)result;
}

// Returns whether the day is already in the list of days,
// and fills in *key with the key used to access its location
- (BOOL)findKey:(WebHistoryDateKey*)key forDay:(NSTimeInterval)date
{
    ASSERT_ARG(key, key != nil);
    *key = timeIntervalForBeginningOfDay(date);
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

- (BOOL)_removeItemFromDateCaches:(WebHistoryItem *)entry
{
    WebHistoryDateKey dateKey;
    BOOL foundDate = [self findKey:&dateKey forDay:[entry lastVisitedTimeInterval]];
 
    if (!foundDate)
        return NO;

    DateToEntriesMap::iterator it = _entriesByDate->find(dateKey);
    NSMutableArray *entriesForDate = it->second.get();
    [entriesForDate removeObjectIdenticalTo:entry];
    
    // remove this date entirely if there are no other entries on it
    if ([entriesForDate count] == 0) {
        _entriesByDate->remove(it);
        // Clear _orderedLastVisitedDays so it will be regenerated when next requested.
        [_orderedLastVisitedDays release];
        _orderedLastVisitedDays = nil;
    }
    
    return YES;
}

- (BOOL)removeItemForURLString: (NSString *)URLString
{
    WebHistoryItem *entry = [_entriesByURL objectForKey: URLString];
    if (entry == nil) {
        return NO;
    }

    [_entriesByURL removeObjectForKey: URLString];
    
#if ASSERT_DISABLED
    [self _removeItemFromDateCaches:entry];
#else
    BOOL itemWasInDateCaches = [self _removeItemFromDateCaches:entry];
    ASSERT(itemWasInDateCaches);
#endif

    return YES;
}

- (void)_addItemToDateCaches:(WebHistoryItem *)entry
{
    WebHistoryDateKey dateKey;
    if ([self findKey:&dateKey forDay:[entry lastVisitedTimeInterval]])
        // other entries already exist for this date
        [self insertItem:entry forDateKey:dateKey];
    else {
        // no other entries exist for this date
        NSMutableArray *entries = [[NSMutableArray alloc] initWithObjects:&entry count:1];
        _entriesByDate->set(dateKey, entries);
        [entries release];
        // Clear _orderedLastVisitedDays so it will be regenerated when next requested.
        [_orderedLastVisitedDays release];
        _orderedLastVisitedDays = nil;
    }
}

- (void)addItem:(WebHistoryItem *)entry
{
    ASSERT_ARG(entry, entry);
    ASSERT_ARG(entry, [entry lastVisitedTimeInterval] != 0);

    NSString *URLString = [entry URLString];

    WebHistoryItem *oldEntry = [_entriesByURL objectForKey:URLString];
    if (oldEntry) {
        // The last reference to oldEntry might be this dictionary, so we hold onto a reference
        // until we're done with oldEntry.
        [oldEntry retain];
        [self removeItemForURLString:URLString];

        // If we already have an item with this URL, we need to merge info that drives the
        // URL autocomplete heuristics from that item into the new one.
        [entry _mergeAutoCompleteHints:oldEntry];
        [oldEntry release];
    }

    [self _addItemToDateCaches:entry];
    [_entriesByURL setObject:entry forKey:URLString];
}

- (void)setLastVisitedTimeInterval:(NSTimeInterval)time forItem:(WebHistoryItem *)entry
{
#if ASSERT_DISABLED
    [self _removeItemFromDateCaches:entry];
#else
    BOOL entryWasPresent = [self _removeItemFromDateCaches:entry];
    ASSERT(entryWasPresent);
#endif
    
    [entry _setLastVisitedTimeInterval:time];
    [self _addItemToDateCaches:entry];

    // Don't send notification until entry is back in the right place in the date caches,
    // since observers might fetch history by date when they receive the notification.
    [[NSNotificationCenter defaultCenter]
        postNotificationName:WebHistoryItemChangedNotification object:entry userInfo:nil];
}

- (BOOL)removeItem: (WebHistoryItem *)entry
{
    WebHistoryItem *matchingEntry;
    NSString *URLString;

    URLString = [entry URLString];

    // If this exact object isn't stored, then make no change.
    // FIXME: Is this the right behavior if this entry isn't present, but another entry for the same URL is?
    // Maybe need to change the API to make something like removeEntryForURLString public instead.
    matchingEntry = [_entriesByURL objectForKey: URLString];
    if (matchingEntry != entry) {
        return NO;
    }

    [self removeItemForURLString: URLString];

    return YES;
}

- (BOOL)removeItems: (NSArray *)entries
{
    int index, count;

    count = [entries count];
    if (count == 0) {
        return NO;
    }

    for (index = 0; index < count; ++index) {
        [self removeItem:[entries objectAtIndex:index]];
    }
    
    return YES;
}

- (BOOL)removeAllItems
{
    if ([_entriesByURL count] == 0) {
        return NO;
    }

    _entriesByDate->clear();
    [_entriesByURL removeAllObjects];

    return YES;
}

- (void)addItems:(NSArray *)newEntries
{
    NSEnumerator *enumerator;
    WebHistoryItem *entry;

    // There is no guarantee that the incoming entries are in any particular
    // order, but if this is called with a set of entries that were created by
    // iterating through the results of orderedLastVisitedDays and orderedItemsLastVisitedOnDayy
    // then they will be ordered chronologically from newest to oldest. We can make adding them
    // faster (fewer compares) by inserting them from oldest to newest.
    enumerator = [newEntries reverseObjectEnumerator];
    while ((entry = [enumerator nextObject]) != nil) {
        [self addItem:entry];
    }
}

#pragma mark DATE-BASED RETRIEVAL

- (NSArray *)orderedLastVisitedDays
{
    if (!_orderedLastVisitedDays) {
        Vector<int> daysAsTimeIntervals;
        daysAsTimeIntervals.reserveCapacity(_entriesByDate->size());
        DateToEntriesMap::const_iterator end = _entriesByDate->end();
        for (DateToEntriesMap::const_iterator it = _entriesByDate->begin(); it != end; ++it)
            daysAsTimeIntervals.append(it->first);

        std::sort(daysAsTimeIntervals.begin(), daysAsTimeIntervals.end());
        size_t count = daysAsTimeIntervals.size();
        _orderedLastVisitedDays = [[NSMutableArray alloc] initWithCapacity:count];
        for (int i = count - 1; i >= 0; i--) {
            NSTimeInterval interval = daysAsTimeIntervals[i];
            NSCalendarDate *date = [[NSCalendarDate alloc] initWithTimeIntervalSinceReferenceDate:interval];
            [_orderedLastVisitedDays addObject:date];
            [date release];
        }
    }
    return _orderedLastVisitedDays;
}

- (NSArray *)orderedItemsLastVisitedOnDay: (NSCalendarDate *)date
{
    WebHistoryDateKey dateKey;
    if ([self findKey:&dateKey forDay:[date timeIntervalSinceReferenceDate]])
        return _entriesByDate->get(dateKey).get();

    return nil;
}

#pragma mark URL MATCHING

- (WebHistoryItem *)itemForURLString:(NSString *)URLString
{
    return [_entriesByURL objectForKey: URLString];
}

- (BOOL)containsItemForURLString: (NSString *)URLString
{
    return [self itemForURLString:URLString] != nil;
}

- (BOOL)containsURL: (NSURL *)URL
{
    return [self itemForURLString:[URL _web_originalDataAsString]] != nil;
}

- (WebHistoryItem *)itemForURL:(NSURL *)URL
{
    return [self itemForURLString:[URL _web_originalDataAsString]];
}

#pragma mark ARCHIVING/UNARCHIVING

- (void)setHistoryAgeInDaysLimit:(int)limit
{
    ageInDaysLimitSet = YES;
    ageInDaysLimit = limit;
}

- (int)historyAgeInDaysLimit
{
    if (ageInDaysLimitSet)
        return ageInDaysLimit;
    return [[NSUserDefaults standardUserDefaults] integerForKey: @"WebKitHistoryAgeInDaysLimit"];
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
    return [[NSUserDefaults standardUserDefaults] integerForKey: @"WebKitHistoryItemLimit"];
}

// Return a date that marks the age limit for history entries saved to or
// loaded from disk. Any entry older than this item should be rejected.
- (NSCalendarDate *)_ageLimitDate
{
    return [[NSCalendarDate calendarDate] dateByAddingYears:0 months:0 days:-[self historyAgeInDaysLimit]
                                                      hours:0 minutes:0 seconds:0];
}

// Return a flat array of WebHistoryItems. Ignores the date and item count limits; these are
// respected when loading instead of when saving, so that clients can learn of discarded items
// by listening to WebHistoryItemsDiscardedWhileLoadingNotification.
- (NSArray *)arrayRepresentation
{
    NSMutableArray *arrayRep = [NSMutableArray array];

    Vector<int> dateKeys;
    dateKeys.reserveCapacity(_entriesByDate->size());
    DateToEntriesMap::const_iterator end = _entriesByDate->end();
    for (DateToEntriesMap::const_iterator it = _entriesByDate->begin(); it != end; ++it)
        dateKeys.append(it->first);

    std::sort(dateKeys.begin(), dateKeys.end());
    for (int dateIndex = dateKeys.size() - 1; dateIndex >= 0; dateIndex--) {
        NSArray *entries = _entriesByDate->get(dateKeys[dateIndex]).get();
        int entryCount = [entries count];
        for (int entryIndex = 0; entryIndex < entryCount; ++entryIndex)
            [arrayRep addObject:[[entries objectAtIndex:entryIndex] dictionaryRepresentation]];
    }

    return arrayRep;
}

- (BOOL)_loadHistoryGutsFromURL:(NSURL *)URL savedItemsCount:(int *)numberOfItemsLoaded collectDiscardedItemsInto:(NSMutableArray *)discardedItems error:(NSError **)error
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
        NSData *data = [NSURLConnection sendSynchronousRequest:[NSURLRequest requestWithURL:URL] returningResponse:nil error:error];
        if (data && [data length] > 0) {
            dictionary = [NSPropertyListSerialization propertyListFromData:data
                mutabilityOption:NSPropertyListImmutable
                format:nil
                errorDescription:nil];
        }
    }

    // We used to support NSArrays here, but that was before Safari 1.0 shipped. We will no longer support
    // that ancient format, so anything that isn't an NSDictionary is bogus.
    if (![dictionary isKindOfClass:[NSDictionary class]])
        return NO;

    NSNumber *fileVersionObject = [dictionary objectForKey:FileVersionKey];
    int fileVersion;
    // we don't trust data obtained from elsewhere, so double-check
    if (fileVersionObject != nil && [fileVersionObject isKindOfClass:[NSNumber class]]) {
        fileVersion = [fileVersionObject intValue];
    } else {
        LOG_ERROR("history file version can't be determined, therefore not loading");
        return NO;
    }
    if (fileVersion > currentFileVersion) {
        LOG_ERROR("history file version is %d, newer than newest known version %d, therefore not loading", fileVersion, currentFileVersion);
        return NO;
    }    

    NSArray *array = [dictionary objectForKey:DatesArrayKey];

    int itemCountLimit = [self historyItemLimit];
    NSTimeInterval ageLimitDate = [[self _ageLimitDate] timeIntervalSinceReferenceDate];
    NSEnumerator *enumerator = [array objectEnumerator];
    BOOL ageLimitPassed = NO;
    BOOL itemLimitPassed = NO;
    ASSERT(*numberOfItemsLoaded == 0);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSDictionary *itemAsDictionary;
    while ((itemAsDictionary = [enumerator nextObject]) != nil) {
        WebHistoryItem *item = [[WebHistoryItem alloc] initFromDictionaryRepresentation:itemAsDictionary];

        // item without URL is useless; data on disk must have been bad; ignore
        if ([item URLString]) {
            // Test against date limit. Since the items are ordered newest to oldest, we can stop comparing
            // once we've found the first item that's too old.
            if (!ageLimitPassed && [item lastVisitedTimeInterval] <= ageLimitDate)
                ageLimitPassed = YES;

            if (ageLimitPassed || itemLimitPassed)
                [discardedItems addObject:item];
            else {
                [self addItem:item];
                ++(*numberOfItemsLoaded);
                if (*numberOfItemsLoaded == itemCountLimit)
                    itemLimitPassed = YES;

                // Draining the autorelease pool every 50 iterations was found by experimentation to be optimal
                if (*numberOfItemsLoaded % 50 == 0) {
                    [pool drain];
                    pool = [[NSAutoreleasePool alloc] init];
                }
            }
        }
        [item release];
    }
    [pool drain];

    return YES;
}

- (BOOL)loadFromURL:(NSURL *)URL collectDiscardedItemsInto:(NSMutableArray *)discardedItems error:(NSError **)error
{
    int numberOfItems;
    double start, duration;
    BOOL result;

    start = CFAbsoluteTimeGetCurrent();
    result = [self _loadHistoryGutsFromURL:URL savedItemsCount:&numberOfItems collectDiscardedItemsInto:discardedItems error:error];

    if (result) {
        duration = CFAbsoluteTimeGetCurrent() - start;
        LOG(Timing, "loading %d history entries from %@ took %f seconds",
            numberOfItems, URL, duration);
    }

    return result;
}

- (BOOL)_saveHistoryGuts: (int *)numberOfItemsSaved URL:(NSURL *)URL error:(NSError **)error
{
    *numberOfItemsSaved = 0;

    // FIXME:  Correctly report error when new API is ready.
    if (error)
        *error = nil;

    NSArray *array = [self arrayRepresentation];
    NSDictionary *dictionary = [NSDictionary dictionaryWithObjectsAndKeys:
        array, DatesArrayKey,
        [NSNumber numberWithInt:currentFileVersion], FileVersionKey,
        nil];
    NSData *data = [NSPropertyListSerialization dataFromPropertyList:dictionary format:NSPropertyListBinaryFormat_v1_0 errorDescription:nil];
    if (![data writeToURL:URL atomically:YES]) {
        LOG_ERROR("attempt to save %@ to %@ failed", dictionary, URL);
        return NO;
    }
    
    *numberOfItemsSaved = [array count];
    return YES;
}

- (BOOL)saveToURL:(NSURL *)URL error:(NSError **)error
{
    int numberOfItems;
    double start, duration;
    BOOL result;

    start = CFAbsoluteTimeGetCurrent();
    result = [self _saveHistoryGuts: &numberOfItems URL:URL error:error];

    if (result) {
        duration = CFAbsoluteTimeGetCurrent() - start;
        LOG(Timing, "saving %d history entries to %@ took %f seconds",
            numberOfItems, URL, duration);
    }

    return result;
}

@end

@interface _WebCoreHistoryProvider : NSObject  <WebCoreHistoryProvider> 
{
    WebHistory *history;
}
- initWithHistory: (WebHistory *)h;
@end

@implementation _WebCoreHistoryProvider
- initWithHistory: (WebHistory *)h
{
    history = [h retain];
    return self;
}

static inline bool matchLetter(char c, char lowercaseLetter)
{
    return (c | 0x20) == lowercaseLetter;
}

static inline bool matchUnicodeLetter(UniChar c, UniChar lowercaseLetter)
{
    return (c | 0x20) == lowercaseLetter;
}

#define BUFFER_SIZE 2048

- (BOOL)containsItemForURLLatin1:(const char *)latin1 length:(unsigned)length
{
    const char *latin1Str = latin1;
    char staticStrBuffer[BUFFER_SIZE];
    char *strBuffer = NULL;
    BOOL needToAddSlash = FALSE;

    if (length >= 6 &&
        matchLetter(latin1[0], 'h') &&
        matchLetter(latin1[1], 't') &&
        matchLetter(latin1[2], 't') &&
        matchLetter(latin1[3], 'p') &&
        (latin1[4] == ':' 
         || (matchLetter(latin1[4], 's') && latin1[5] == ':'))) {
        int pos = latin1[4] == ':' ? 5 : 6;
        // skip possible initial two slashes
        if (latin1[pos] == '/' && latin1[pos + 1] == '/') {
            pos += 2;
        }

        char *nextSlash = strchr(latin1 + pos, '/');
        if (nextSlash == NULL) {
            needToAddSlash = TRUE;
        }
    }

    if (needToAddSlash) {
        if (length + 1 <= BUFFER_SIZE) {
            strBuffer = staticStrBuffer;
        } else {
            strBuffer = (char*)malloc(length + 2);
        }
        memcpy(strBuffer, latin1, length + 1);
        strBuffer[length] = '/';
        strBuffer[length+1] = '\0';
        length++;

        latin1Str = strBuffer;
    }

    CFStringRef str = CFStringCreateWithCStringNoCopy(NULL, latin1Str, kCFStringEncodingWindowsLatin1, kCFAllocatorNull);
    BOOL result = [history containsItemForURLString:(id)str];
    CFRelease(str);

    if (strBuffer != staticStrBuffer) {
        free(strBuffer);
    }

    return result;
}

#define UNICODE_BUFFER_SIZE 1024

- (BOOL)containsItemForURLUnicode:(const UniChar *)unicode length:(unsigned)length
{
    const UniChar *unicodeStr = unicode;
    UniChar staticStrBuffer[UNICODE_BUFFER_SIZE];
    UniChar *strBuffer = NULL;
    BOOL needToAddSlash = FALSE;

    if (length >= 6 &&
        matchUnicodeLetter(unicode[0], 'h') &&
        matchUnicodeLetter(unicode[1], 't') &&
        matchUnicodeLetter(unicode[2], 't') &&
        matchUnicodeLetter(unicode[3], 'p') &&
        (unicode[4] == ':' 
         || (matchUnicodeLetter(unicode[4], 's') && unicode[5] == ':'))) {

        unsigned pos = unicode[4] == ':' ? 5 : 6;

        // skip possible initial two slashes
        if (pos + 1 < length && unicode[pos] == '/' && unicode[pos + 1] == '/') {
            pos += 2;
        }

        while (pos < length && unicode[pos] != '/') {
            pos++;
        }

        if (pos == length) {
            needToAddSlash = TRUE;
        }
    }

    if (needToAddSlash) {
        if (length + 1 <= UNICODE_BUFFER_SIZE) {
            strBuffer = staticStrBuffer;
        } else {
            strBuffer = (UniChar*)malloc(sizeof(UniChar) * (length + 1));
        }
        memcpy(strBuffer, unicode, 2 * length);
        strBuffer[length] = '/';
        length++;

        unicodeStr = strBuffer;
    }

    CFStringRef str = CFStringCreateWithCharactersNoCopy(NULL, unicodeStr, length, kCFAllocatorNull);
    BOOL result = [history containsItemForURLString:(id)str];
    CFRelease(str);

    if (strBuffer != staticStrBuffer) {
        free(strBuffer);
    }

    return result;
}

- (void)dealloc
{
    [history release];
    [super dealloc];
}

@end

@implementation WebHistory

+ (WebHistory *)optionalSharedHistory
{
    return _sharedHistory;
}


+ (void)setOptionalSharedHistory: (WebHistory *)history
{
    // FIXME.  Need to think about multiple instances of WebHistory per application
    // and correct synchronization of history file between applications.
    [WebCoreHistory setHistoryProvider: [[[_WebCoreHistoryProvider alloc] initWithHistory: history] autorelease]];
    if (_sharedHistory != history){
        [_sharedHistory release];
        _sharedHistory = [history retain];
    }
}

- (id)init
{
    if ((self = [super init]) != nil) {
        _historyPrivate = [[WebHistoryPrivate alloc] init];
    }

    return self;
}

- (void)dealloc
{
    [_historyPrivate release];
    [super dealloc];
}

#pragma mark MODIFYING CONTENTS

- (void)_sendNotification:(NSString *)name entries:(NSArray *)entries
{
    NSDictionary *userInfo = [NSDictionary dictionaryWithObjectsAndKeys:entries, WebHistoryItemsKey, nil];
    [[NSNotificationCenter defaultCenter]
        postNotificationName: name object: self userInfo: userInfo];
}

- (WebHistoryItem *)addItemForURL: (NSURL *)URL
{
    WebHistoryItem *entry = [[WebHistoryItem alloc] initWithURL:URL title:nil];
    [entry _setLastVisitedTimeInterval: [NSDate timeIntervalSinceReferenceDate]];
    [self addItem: entry];
    [entry release];
    return entry;
}


- (void)addItem: (WebHistoryItem *)entry
{
    LOG (History, "adding %@", entry);
    [_historyPrivate addItem: entry];
    [self _sendNotification: WebHistoryItemsAddedNotification
                    entries: [NSArray arrayWithObject:entry]];
}

- (void)removeItem: (WebHistoryItem *)entry
{
    if ([_historyPrivate removeItem: entry]) {
        [self _sendNotification: WebHistoryItemsRemovedNotification
                        entries: [NSArray arrayWithObject:entry]];
    }
}

- (void)removeItems: (NSArray *)entries
{
    if ([_historyPrivate removeItems:entries]) {
        [self _sendNotification: WebHistoryItemsRemovedNotification
                        entries: entries];
    }
}

- (void)removeAllItems
{
    if ([_historyPrivate removeAllItems]) {
        [[NSNotificationCenter defaultCenter]
            postNotificationName: WebHistoryAllItemsRemovedNotification
                          object: self];
    }
}

- (void)addItems:(NSArray *)newEntries
{
    [_historyPrivate addItems:newEntries];
    [self _sendNotification: WebHistoryItemsAddedNotification
                    entries: newEntries];
}

- (void)setLastVisitedTimeInterval:(NSTimeInterval)time forItem:(WebHistoryItem *)entry
{
    [_historyPrivate setLastVisitedTimeInterval:time forItem:entry];
}

#pragma mark DATE-BASED RETRIEVAL

- (NSArray *)orderedLastVisitedDays
{
    return [_historyPrivate orderedLastVisitedDays];
}

- (NSArray *)orderedItemsLastVisitedOnDay: (NSCalendarDate *)date
{
    return [_historyPrivate orderedItemsLastVisitedOnDay: date];
}

#pragma mark URL MATCHING

- (BOOL)containsItemForURLString: (NSString *)URLString
{
    return [_historyPrivate containsItemForURLString: URLString];
}

- (BOOL)containsURL: (NSURL *)URL
{
    return [_historyPrivate containsURL: URL];
}

- (WebHistoryItem *)itemForURL:(NSURL *)URL
{
    return [_historyPrivate itemForURL:URL];
}

#pragma mark SAVING TO DISK

- (BOOL)loadFromURL:(NSURL *)URL error:(NSError **)error
{
    NSMutableArray *discardedItems = [NSMutableArray array];
    
    if ([_historyPrivate loadFromURL:URL collectDiscardedItemsInto:discardedItems error:error]) {
        [[NSNotificationCenter defaultCenter]
            postNotificationName:WebHistoryLoadedNotification
                          object:self];
        
        if ([discardedItems count] > 0)
            [self _sendNotification:WebHistoryItemsDiscardedWhileLoadingNotification entries:discardedItems];
        
        return YES;
    }
    return NO;
}

- (BOOL)saveToURL:(NSURL *)URL error:(NSError **)error
{
    // FIXME:  Use new foundation API to get error when ready.
    if([_historyPrivate saveToURL:URL error:error]){
        [[NSNotificationCenter defaultCenter]
            postNotificationName: WebHistorySavedNotification
                          object: self];
        return YES;
    }
    return NO;    
}

- (WebHistoryItem *)_itemForURLString:(NSString *)URLString
{
    return [_historyPrivate itemForURLString: URLString];
}

- (NSCalendarDate*)ageLimitDate
{
    return [_historyPrivate _ageLimitDate];
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
