//
//  WebHistoryPrivate.m
//  WebKit
//
//  Created by John Sullivan on Tue Feb 19 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebHistoryPrivate.h"

#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebKitLogging.h>

#import <WebFoundation/NSError.h>
#import <WebFoundation/NSURLConnection.h>
#import <WebFoundation/NSURLRequest.h>

#import <WebFoundation/WebNSCalendarDateExtras.h>
#import <WebFoundation/WebNSURLExtras.h>

NSString *FileVersionKey = @"WebHistoryFileVersion";
NSString *DatesArrayKey = @"WebHistoryDates";

#define currentFileVersion	1

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

- (id)init
{
    if (![super init]) {
        return nil;
    }
    
    _entriesByURL = [[NSMutableDictionary alloc] init];
    _datesWithEntries = [[NSMutableArray alloc] init];
    _entriesByDate = [[NSMutableArray alloc] init];

    return self;
}

- (void)dealloc
{
    [_entriesByURL release];
    [_datesWithEntries release];
    [_entriesByDate release];
    
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

- (void)insertItem: (WebHistoryItem *)entry atDateIndex: (int)dateIndex
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

- (BOOL)removeItemForURLString: (NSString *)URLString
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


- (void)addItem: (WebHistoryItem *)entry
{
    int dateIndex;
    NSString *URLString;

    ASSERT_ARG(entry, [entry lastVisitedDate] != nil);

#ifdef FIX_VISITED
    URLString = [[[entry URL] _web_canonicalize] absoluteString];
#else
    URLString = [[entry URL] absoluteString];
#endif

    // If we already have an item with this URL, we need to merge info that drives the
    // URL autocomplete heuristics from that item into the new one.
    WebHistoryItem *oldEntry = [_entriesByURL objectForKey: URLString];
    if (oldEntry) {
        [entry _mergeAutoCompleteHints:oldEntry];
    }

    [self removeItemForURLString: URLString];

    if ([self findIndex: &dateIndex forDay: [entry lastVisitedDate]]) {
        // other entries already exist for this date
        [self insertItem: entry atDateIndex: dateIndex];
    } else {
        // no other entries exist for this date
        [_datesWithEntries insertObject: [entry lastVisitedDate] atIndex: dateIndex];
        [_entriesByDate insertObject: [NSMutableArray arrayWithObject:entry] atIndex: dateIndex];
    }

    [_entriesByURL setObject: entry forKey: URLString];
}

- (BOOL)removeItem: (WebHistoryItem *)entry
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

    [_entriesByDate removeAllObjects];
    [_datesWithEntries removeAllObjects];
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
    return _datesWithEntries;
}

- (NSArray *)orderedItemsLastVisitedOnDay: (NSCalendarDate *)date
{
    int index;

    if ([self findIndex: &index forDay: date]) {
        return [_entriesByDate objectAtIndex: index];
    }

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
#ifdef FIX_VISITED
    return [self itemForURLString:[[URL _web_canonicalize] absoluteString]] != nil;
#else
    return [self itemForURLString:[URL absoluteString]] != nil;
#endif
}

- (WebHistoryItem *)itemForURL:(NSURL *)URL
{
#ifdef FIX_VISITED
    return [self itemForURLString:[[URL _web_canonicalize] absoluteString]];
#else
    return [self itemForURLString:[URL absoluteString]];
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

- (BOOL)_loadHistoryGuts: (int *)numberOfItemsLoaded URL:(NSURL *)URL error:(NSError **)error
{
    NSEnumerator *enumerator;
    int index;
    int limit;
    NSCalendarDate *ageLimitDate;
    NSDictionary *fileAsDictionary;
    BOOL ageLimitPassed;

    *numberOfItemsLoaded = 0;

    NSData *data = [NSURLConnection sendSynchronousRequest:[NSURLRequest requestWithURL:URL] returningResponse:nil error:error];
    fileAsDictionary = [NSPropertyListSerialization propertyListFromData:data mutabilityOption:NSPropertyListImmutable format:nil errorDescription:nil];
    if (fileAsDictionary == nil) {
        // Couldn't read a dictionary; let's see if we can read an old-style array instead
        NSArray *fileAsArray = [NSArray arrayWithContentsOfURL:URL];
        if (fileAsArray == nil) {
#if !ERROR_DISABLED
            if ([URL isFileURL] && [[NSFileManager defaultManager] fileExistsAtPath: [URL path]]) {
                ERROR("unable to read history from file %@; perhaps contents are corrupted", [URL path]);
            }
#endif
            return NO;
        } else {
            // Convert old-style array into new-style dictionary
            fileAsDictionary = [NSDictionary dictionaryWithObjectsAndKeys:
                fileAsArray, DatesArrayKey,
                [NSNumber numberWithInt:1], FileVersionKey,
                nil];
        }
    }

    NSNumber *fileVersionObject = [fileAsDictionary objectForKey:FileVersionKey];
    int fileVersion;
    // we don't trust data read from disk, so double-check
    if (fileVersionObject != nil && [fileVersionObject isKindOfClass:[NSNumber class]]) {
        fileVersion = [fileVersionObject intValue];
    } else {
        ERROR("history file version can't be determined, therefore not loading");
        return NO;
    }
    if (fileVersion > currentFileVersion) {
        ERROR("history file version is %d, newer than newest known version %d, therefore not loading", fileVersion, currentFileVersion);
        return NO;
    }    

    NSArray *array = [fileAsDictionary objectForKey:DatesArrayKey];
        
    limit = [[NSUserDefaults standardUserDefaults] integerForKey: @"WebKitHistoryItemLimit"];
    ageLimitDate = [self _ageLimitDate];
    index = 0;
    // reverse dates so you're loading the oldest first, to minimize the number of comparisons
    enumerator = [array reverseObjectEnumerator];
    ageLimitPassed = NO;

    NSDictionary *itemAsDictionary;
    while ((itemAsDictionary = [enumerator nextObject]) != nil) {
        WebHistoryItem *entry;

        entry = [[[WebHistoryItem alloc] initFromDictionaryRepresentation:itemAsDictionary] autorelease];

        if ([entry URL] == nil || [entry lastVisitedDate] == nil) {
            // entry without URL is useless; data on disk must have been bad; ignore this one
            // entry without lastVisitDate should never happen; ignore that one
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
        
        [self addItem: entry];
        if (++index >= limit) {
            break;
        }
    }

    *numberOfItemsLoaded = MIN(index, limit);
    return YES;    
}

- (BOOL)loadFromURL:(NSURL *)URL error:(NSError **)error
{
    int numberOfItems;
    double start, duration;
    BOOL result;

    start = CFAbsoluteTimeGetCurrent();
    result = [self _loadHistoryGuts: &numberOfItems URL:URL error:error];

    if (result) {
        duration = CFAbsoluteTimeGetCurrent() - start;
        LOG(Timing, "loading %d history entries from %@ took %f seconds",
            numberOfItems, [URL absoluteString], duration);
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
    if (![dictionary writeToURL:URL atomically:YES]) {
        ERROR("attempt to save %@ to %@ failed", dictionary, [URL absoluteString]);
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
            numberOfItems, [URL absoluteString], duration);
    }

    return result;
}

@end
