//
//  WebHistoryPrivate.m
//  WebKit
//
//  Created by John Sullivan on Tue Feb 19 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebHistoryPrivate.h"

#import "WebHistoryItem.h"
#import <WebKit/WebKitLogging.h>

#import <WebFoundation/WebNSCalendarDateExtras.h>
#import <WebFoundation/WebNSURLExtras.h>


@interface WebHistoryPrivate (Private)
-(WebHistoryItem *)_entryForURLString:(NSString *)URLString;
@end

@implementation WebHistoryPrivate

//#define FIX_VISITED

#pragma mark OBJECT FRAMEWORK

+ (void)initialize
{
    [[NSUserDefaults standardUserDefaults] registerDefaults:
        [NSDictionary dictionaryWithObjectsAndKeys:
            @"1000", @"WebKitHistoryItemLimit",
            @"7", @"WebKitHistoryAgeInDaysLimit",
            nil]];    
}

- (id)initWithFile: (NSString *)file
{
    if (![super init]) {
        return nil;
    }
    
    _entriesByURL = [[NSMutableDictionary alloc] init];
    _datesWithEntries = [[NSMutableArray alloc] init];
    _entriesByDate = [[NSMutableArray alloc] init];
    _file = [file retain];

    // read history from disk
    [self loadHistory];
    
    return self;
}

- (void)dealloc
{
    [_entriesByURL release];
    [_datesWithEntries release];
    [_entriesByDate release];
    [_file release];
    
    [super dealloc];
}

#pragma mark MODIFYING CONTENTS

// Returns whether the day is already in the list of days,
// and fills in *index with the found or proposed index.
- (BOOL)findIndex: (int *)index forDay: (NSCalendarDate *)date
{
    int count;

    ASSERT_ARG(index, index != nil);

    //FIXME: just does linear search through days; inefficient if many days
    count = [_datesWithEntries count];
    for (*index = 0; *index < count; ++*index) {
        NSComparisonResult result = [date _web_compareDay: [_datesWithEntries objectAtIndex: *index]];
        if (result == NSOrderedSame) {
            return YES;
        }
        if (result == NSOrderedDescending) {
            return NO;
        }
    }

    return NO;
}

- (void)insertEntry: (WebHistoryItem *)entry atDateIndex: (int)dateIndex
{
    int index, count;
    NSMutableArray *entriesForDate;
    NSCalendarDate *entryDate;

    ASSERT_ARG(entry, entry != nil);
    ASSERT_ARG(dateIndex, dateIndex >= 0 && (uint)dateIndex < [_entriesByDate count]);

    //FIXME: just does linear search through entries; inefficient if many entries for this date
    entryDate = [entry lastVisitedDate];
    entriesForDate = [_entriesByDate objectAtIndex: dateIndex];
    count = [entriesForDate count];
    // optimized for inserting oldest to youngest
    for (index = 0; index < count; ++index) {
        if ([entryDate compare: [[entriesForDate objectAtIndex: index] lastVisitedDate]] != NSOrderedAscending) {
            break;
        }
    }

    [entriesForDate insertObject: entry atIndex: index];
}

- (BOOL)removeEntryForURLString: (NSString *)URLString
{
    NSMutableArray *entriesForDate;
    WebHistoryItem *entry;
    int dateIndex;
    BOOL foundDate;

    entry = [_entriesByURL objectForKey: URLString];
    if (entry == nil) {
        return NO;
    }

    [_entriesByURL removeObjectForKey: URLString];

    foundDate = [self findIndex: &dateIndex forDay: [entry lastVisitedDate]];

    ASSERT(foundDate);
    
    entriesForDate = [_entriesByDate objectAtIndex: dateIndex];
    [entriesForDate removeObjectIdenticalTo: entry];

    // remove this date entirely if there are no other entries on it
    if ([entriesForDate count] == 0) {
        [_entriesByDate removeObjectAtIndex: dateIndex];
        [_datesWithEntries removeObjectAtIndex: dateIndex];
    }

    return YES;
}


- (void)addEntry: (WebHistoryItem *)entry
{
    int dateIndex;
    NSString *URLString;

    ASSERT_ARG(entry, [entry lastVisitedDate] != nil);

#ifdef FIX_VISITED
    URLString = [[[entry URL] _web_canonicalize] absoluteString];
#else
    URLString = [[entry URL] absoluteString];
#endif
    [self removeEntryForURLString: URLString];

    if ([self findIndex: &dateIndex forDay: [entry lastVisitedDate]]) {
        // other entries already exist for this date
        [self insertEntry: entry atDateIndex: dateIndex];
    } else {
        // no other entries exist for this date
        [_datesWithEntries insertObject: [entry lastVisitedDate] atIndex: dateIndex];
        [_entriesByDate insertObject: [NSMutableArray arrayWithObject:entry] atIndex: dateIndex];
    }

    [_entriesByURL setObject: entry forKey: URLString];
}

- (BOOL)removeEntry: (WebHistoryItem *)entry
{
    WebHistoryItem *matchingEntry;
    NSString *URLString;

#ifdef FIX_VISITED
    URLString = [[[entry URL] _web_canonicalize] absoluteString];
#else
    URLString = [[entry URL] absoluteString];
#endif

    // If this exact object isn't stored, then make no change.
    // FIXME: Is this the right behavior if this entry isn't present, but another entry for the same URL is?
    // Maybe need to change the API to make something like removeEntryForURLString public instead.
    matchingEntry = [_entriesByURL objectForKey: URLString];
    if (matchingEntry != entry) {
        return NO;
    }

    [self removeEntryForURLString: URLString];

    return YES;
}

- (BOOL)removeEntries: (NSArray *)entries
{
    int index, count;

    count = [entries count];
    if (count == 0) {
        return NO;
    }

    for (index = 0; index < count; ++index) {
        [self removeEntry:[entries objectAtIndex:index]];
    }
    
    return YES;
}

- (BOOL)removeAllEntries
{
    if ([_entriesByURL count] == 0) {
        return NO;
    }

    [_entriesByDate removeAllObjects];
    [_datesWithEntries removeAllObjects];
    [_entriesByURL removeAllObjects];

    return YES;
}

- (void)addEntries:(NSArray *)newEntries
{
    NSEnumerator *enumerator;
    WebHistoryItem *entry;

    // There is no guarantee that the incoming entries are in any particular
    // order, but if this is called with a set of entries that were created by
    // iterating through the results of orderedLastVisitedDays and orderedEntriesLastVisitedOnDayy
    // then they will be ordered chronologically from newest to oldest. We can make adding them
    // faster (fewer compares) by inserting them from oldest to newest.
    enumerator = [newEntries reverseObjectEnumerator];
    while ((entry = [enumerator nextObject]) != nil) {
        [self addEntry:entry];
    }
}

#pragma mark DATE-BASED RETRIEVAL

- (NSArray *)orderedLastVisitedDays
{
    return _datesWithEntries;
}

- (NSArray *)orderedEntriesLastVisitedOnDay: (NSCalendarDate *)date
{
    int index;

    if ([self findIndex: &index forDay: date]) {
        return [_entriesByDate objectAtIndex: index];
    }

    return nil;
}

#pragma mark URL MATCHING

-(WebHistoryItem *)_entryForURLString:(NSString *)URLString
{
    return [_entriesByURL objectForKey: URLString];
}

- (BOOL)containsEntryForURLString: (NSString *)URLString
{
    return [self _entryForURLString:URLString] != nil;
}

- (BOOL)containsURL: (NSURL *)URL
{
#ifdef FIX_VISITED
    return [self _entryForURLString:[[URL _web_canonicalize] absoluteString]] != nil;
#else
    return [self _entryForURLString:[URL absoluteString]] != nil;
#endif
}

- (WebHistoryItem *)entryForURL:(NSURL *)URL
{
#ifdef FIX_VISITED
    return [self _entryForURLString:[[URL _web_canonicalize] absoluteString]];
#else
    return [self _entryForURLString:[URL absoluteString]];
#endif
}	

#pragma mark ARCHIVING/UNARCHIVING

// Return a date that marks the age limit for history entries saved to or
// loaded from disk. Any entry on this day or older should be rejected,
// as tested with -[NSCalendarDate compareDay:]
- (NSCalendarDate *)_ageLimitDate
{
    int ageInDaysLimit;

    ageInDaysLimit = [[NSUserDefaults standardUserDefaults] integerForKey: @"WebKitHistoryAgeInDaysLimit"];
    return [[NSCalendarDate calendarDate] dateByAddingYears:0 months:0 days:-ageInDaysLimit
                                                      hours:0 minutes:0 seconds:0];
}

// Return a flat array of WebHistoryItems. Leaves out entries older than the age limit.
// Stops filling array when item count limit is reached, even if there are currently
// more entries than that.
- (NSArray *)arrayRepresentation
{
    int dateCount, dateIndex;
    int limit;
    int totalSoFar;
    NSMutableArray *arrayRep;
    NSCalendarDate *ageLimitDate;

    arrayRep = [NSMutableArray array];

    limit = [[NSUserDefaults standardUserDefaults] integerForKey: @"WebKitHistoryItemLimit"];
    ageLimitDate = [self _ageLimitDate];
    totalSoFar = 0;
    
    dateCount = [_entriesByDate count];
    for (dateIndex = 0; dateIndex < dateCount; ++dateIndex) {
        int entryCount, entryIndex;
        NSArray *entries;

        // skip remaining days if they are older than the age limit
        if ([[_datesWithEntries objectAtIndex:dateIndex] _web_compareDay:ageLimitDate] != NSOrderedDescending) {
            break;
        }

        entries = [_entriesByDate objectAtIndex:dateIndex];
        entryCount = [entries count];
        for (entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
            if (totalSoFar++ >= limit) {
                break;
            }
            [arrayRep addObject: [[entries objectAtIndex:entryIndex] dictionaryRepresentation]];
        }
    }

    return arrayRep;
}

- (NSString *)file
{
    return _file;
}

- (BOOL)_loadHistoryGuts: (int *)numberOfItemsLoaded
{
    NSString *path;
    NSArray *array;
    NSEnumerator *enumerator;
    NSDictionary *dictionary;
    int index;
    int limit;
    NSCalendarDate *ageLimitDate;
    BOOL ageLimitPassed;

    *numberOfItemsLoaded = 0;

    path = [self file];
    if (path == nil) {
        ERROR("couldn't load history; couldn't find or create directory to store it in");
        return NO;
    }

    array = [NSArray arrayWithContentsOfFile: path];
    if (array == nil) {
        if ([[NSFileManager defaultManager] fileExistsAtPath: path]) {
            ERROR("attempt to read history from %@ failed; perhaps contents are corrupted", path);
        } // else file doesn't exist, which is a normal initial state, so don't spam
        return NO;
    }

    limit = [[NSUserDefaults standardUserDefaults] integerForKey: @"WebKitHistoryItemLimit"];
    ageLimitDate = [self _ageLimitDate];
    index = 0;
    // reverse dates so you're loading the oldest first, to minimize the number of comparisons
    enumerator = [array reverseObjectEnumerator];
    ageLimitPassed = NO;

    while ((dictionary = [enumerator nextObject]) != nil) {
        WebHistoryItem *entry;

        entry = [[[WebHistoryItem alloc] initFromDictionaryRepresentation: dictionary] autorelease];

        if ([entry URL] == nil) {
            // entry without URL is useless; data on disk must have been bad; ignore this one
            continue;
        }

        // test against date limit
        if (!ageLimitPassed) {
            if ([[entry lastVisitedDate] _web_compareDay:ageLimitDate] != NSOrderedDescending) {
                continue;
            } else {
                ageLimitPassed = YES;
            }
        }
        
        [self addEntry: entry];
        if (++index >= limit) {
            break;
        }
    }

    *numberOfItemsLoaded = MIN(index, limit);
    return YES;    
}

- (BOOL)loadHistory
{
    int numberOfItems;
    double start, duration;
    BOOL result;

    start = CFAbsoluteTimeGetCurrent();
    result = [self _loadHistoryGuts: &numberOfItems];

    if (result) {
        duration = CFAbsoluteTimeGetCurrent() - start;
        LOG(Timing, "loading %d history entries from %@ took %f seconds",
            numberOfItems, [self file], duration);
    }

    return result;
}

- (BOOL)_saveHistoryGuts: (int *)numberOfItemsSaved
{
    NSString *path;
    NSArray *array;
    *numberOfItemsSaved = 0;

    path = [self file];
    if (path == nil) {
        ERROR("couldn't save history; couldn't find or create directory to store it in");
        return NO;
    }

    array = [self arrayRepresentation];
    if (![array writeToFile:path atomically:YES]) {
        ERROR("attempt to save %@ to %@ failed", array, path);
        return NO;
    }
    
    *numberOfItemsSaved = [array count];
    return YES;
}

- (BOOL)saveHistory
{
    int numberOfItems;
    double start, duration;
    BOOL result;

    start = CFAbsoluteTimeGetCurrent();
    result = [self _saveHistoryGuts: &numberOfItems];

    if (result) {
        duration = CFAbsoluteTimeGetCurrent() - start;
        LOG(Timing, "saving %d history entries to %@ took %f seconds",
            numberOfItems, [self file], duration);
    }

    return result;
}

@end
