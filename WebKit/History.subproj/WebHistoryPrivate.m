//
//  IFWebHistoryPrivate.m
//  WebKit
//
//  Created by John Sullivan on Tue Feb 19 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "IFWebHistoryPrivate.h"

#import <WebFoundation/IFNSCalendarDateExtensions.h>
#import <WebKit/WebKitDebug.h>
#import "IFURIEntry.h"

@interface IFWebHistoryPrivate (Private)
-(IFURIEntry *)_entryForURLString:(NSString *)urlString;
@end

@implementation IFWebHistoryPrivate

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
    
    _urlDictionary = [[NSMutableDictionary alloc] init];
    _datesWithEntries = [[NSMutableArray alloc] init];
    _entriesByDate = [[NSMutableArray alloc] init];
    _file = [file retain];

    // read history from disk
    [self loadHistory];
    
    return self;
}

- (void)dealloc
{
    [_urlDictionary release];
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

    WEBKIT_ASSERT_VALID_ARG (index, index != nil);

    //FIXME: just does linear search through days; inefficient if many days
    count = [_datesWithEntries count];
    for (*index = 0; *index < count; ++*index) {
        NSComparisonResult result = [date _IF_compareDay: [_datesWithEntries objectAtIndex: *index]];
        if (result == NSOrderedSame) {
            return YES;
        }
        if (result == NSOrderedDescending) {
            return NO;
        }
    }

    return NO;
}

- (void)insertEntry: (IFURIEntry *)entry atDateIndex: (int)dateIndex
{
    int index, count;
    NSMutableArray *entriesForDate;
    NSCalendarDate *entryDate;

    WEBKIT_ASSERT_VALID_ARG (entry, entry != nil);
    WEBKIT_ASSERT_VALID_ARG (dateIndex, dateIndex >= 0 && (uint)dateIndex < [_entriesByDate count]);

    //FIXME: just does linear search through entries; inefficient if many entries for this date
    entryDate = [entry lastVisitedDate];
    entriesForDate = [_entriesByDate objectAtIndex: dateIndex];
    count = [entriesForDate count];
    for (index = 0; index < count; ++index) {
        if ([entryDate compare: [[entriesForDate objectAtIndex: index] lastVisitedDate]] != NSOrderedAscending) {
            break;
        }
    }

    [entriesForDate insertObject: entry atIndex: index];
}

- (BOOL)removeEntryForURLString: (NSString *)urlString
{
    NSMutableArray *entriesForDate;
    IFURIEntry *entry;
    int dateIndex;
    BOOL foundDate;

    entry = [_urlDictionary objectForKey: urlString];
    if (entry == nil) {
        return NO;
    }

    [_urlDictionary removeObjectForKey: urlString];

    foundDate = [self findIndex: &dateIndex forDay: [entry lastVisitedDate]];

    WEBKIT_ASSERT (foundDate);
    
    entriesForDate = [_entriesByDate objectAtIndex: dateIndex];
    [entriesForDate removeObject: entry];

    // remove this date entirely if there are no other entries on it
    if ([entriesForDate count] == 0) {
        [_entriesByDate removeObjectAtIndex: dateIndex];
        [_datesWithEntries removeObjectAtIndex: dateIndex];
    }

    return YES;
}


- (void)addEntry: (IFURIEntry *)entry
{
    int dateIndex;
    NSString *urlString;

    WEBKIT_ASSERT_VALID_ARG (entry, [entry lastVisitedDate] != nil);

    urlString = [[entry url] absoluteString];
    [self removeEntryForURLString: urlString];

    if ([self findIndex: &dateIndex forDay: [entry lastVisitedDate]]) {
        // other entries already exist for this date
        [self insertEntry: entry atDateIndex: dateIndex];
    } else {
        // no other entries exist for this date
        [_datesWithEntries insertObject: [entry lastVisitedDate] atIndex: dateIndex];
        [_entriesByDate insertObject: [NSMutableArray arrayWithObject:entry] atIndex: dateIndex];
    }

    [_urlDictionary setObject: entry forKey: urlString];
}

- (BOOL)removeEntry: (IFURIEntry *)entry
{
    IFURIEntry *matchingEntry;
    NSString *urlString;

    urlString = [[entry url] absoluteString];

    // If this exact object isn't stored, then make no change.
    // FIXME: Is this the right behavior if this entry isn't present, but another entry for the same URL is?
    // Maybe need to change the API to make something like removeEntryForURLString public instead.
    matchingEntry = [_urlDictionary objectForKey: urlString];
    if (matchingEntry != entry) {
        return NO;
    }

    [self removeEntryForURLString: urlString];

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
    if ([_urlDictionary count] == 0) {
        return NO;
    }

    [_entriesByDate removeAllObjects];
    [_datesWithEntries removeAllObjects];
    [_urlDictionary removeAllObjects];

    return YES;
}

- (void)addEntries:(NSArray *)newEntries
{
    NSEnumerator *enumerator;
    IFURIEntry *entry;

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

- (IFURIEntry *)updateURL:(NSString *)newURLString
                    title:(NSString *)newTitle
             displayTitle:(NSString *)newDisplayTitle
                   forURL:(NSString *)oldURLString
{
    IFURIEntry *entry;

    WEBKIT_ASSERT (oldURLString != nil);

    entry = [self _entryForURLString:oldURLString];
    if (entry == nil) {
        return nil;
    }

    if (newURLString != nil) {
        [entry setURL:[NSURL URLWithString:newURLString]];
    }

    if (newTitle != nil) {
        [entry setTitle:newTitle];
    }

    if (newDisplayTitle != nil) {
        [entry setDisplayTitle:newDisplayTitle];
    }

    return entry;
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

#pragma mark STRING-BASED RETRIEVAL

- (NSArray *)entriesWithAddressContainingString: (NSString *)string
{
    // FIXME: not yet implemented
    return nil;
}

- (NSArray *)entriesWithTitleOrAddressContainingString: (NSString *)string
{
    // FIXME: not yet implemented
    return nil;
}

#pragma mark URL MATCHING

-(IFURIEntry *)_entryForURLString:(NSString *)urlString
{
    return [_urlDictionary objectForKey: urlString];
}

- (BOOL)containsURL: (NSURL *)url
{
    return [self _entryForURLString:[url absoluteString]] != nil;
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

// Return a flat array of IFURIEntries. Leaves out entries older than the age limit.
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
        if ([[_datesWithEntries objectAtIndex:dateIndex] _IF_compareDay:ageLimitDate] != NSOrderedDescending) {
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
        WEBKITDEBUG("couldn't load history; couldn't find or create directory to store it in\n");
        return NO;
    }

    array = [NSArray arrayWithContentsOfFile: path];
    if (array == nil) {
        if (![[NSFileManager defaultManager] fileExistsAtPath: path]) {
            WEBKITDEBUG("no history file found at %s\n",
                            DEBUG_OBJECT(path));
        } else {
            WEBKITDEBUG("attempt to read history from %s failed; perhaps contents are corrupted\n",
                            DEBUG_OBJECT(path));
        }
        return NO;
    }

    limit = [[NSUserDefaults standardUserDefaults] integerForKey: @"WebKitHistoryItemLimit"];
    ageLimitDate = [self _ageLimitDate];
    index = 0;
    // reverse dates so you're loading the oldest first, to minimize the number of comparisons
    enumerator = [array reverseObjectEnumerator];
    ageLimitPassed = NO;

    while ((dictionary = [enumerator nextObject]) != nil) {
        IFURIEntry *entry;

        entry = [[[IFURIEntry alloc] initFromDictionaryRepresentation: dictionary] autorelease];

        if ([entry url] == nil) {
            // entry without url is useless; data on disk must have been bad; ignore this one
            continue;
        }

        // test against date limit
        if (!ageLimitPassed) {
            if ([[entry lastVisitedDate] _IF_compareDay:ageLimitDate] != NSOrderedDescending) {
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

    if (result == YES) {
        duration = CFAbsoluteTimeGetCurrent() - start;
        WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "loading %d history entries from %s took %f seconds\n",
                           numberOfItems, DEBUG_OBJECT([self file]), duration);
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
        WEBKITDEBUG("couldn't save history; couldn't find or create directory to store it in\n");
        return NO;
    }

    array = [self arrayRepresentation];
    if (![array writeToFile:path atomically:YES]) {
        WEBKITDEBUG("attempt to save %s to %s failed\n", DEBUG_OBJECT(array), DEBUG_OBJECT(path));
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

    if (result == YES) {
        duration = CFAbsoluteTimeGetCurrent() - start;
        WEBKITDEBUGLEVEL (WEBKIT_LOG_TIMING, "saving %d history entries to %s took %f seconds\n",
                           numberOfItems, DEBUG_OBJECT([self file]), duration);
    }

    return result;
}

@end
