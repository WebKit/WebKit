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

@interface IFWebHistoryPrivate (Private)
- (void)loadHistory;
@end

@implementation IFWebHistoryPrivate

#pragma mark OBJECT FRAMEWORK

- (id)init
{
    if ((self = [super init]) != nil) {
        _urlDictionary = [[NSMutableDictionary alloc] init];
        _datesWithEntries = [[NSMutableArray alloc] init];
        _entriesByDate = [[NSMutableArray alloc] init];

        // read history from disk
        [self loadHistory];
    }
    
    return self;
}

- (void)dealloc
{
    [_urlDictionary release];
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

    WEBKIT_ASSERT_VALID_ARG (index, index != nil);

    //FIXME: just does linear search through days; inefficient if many days
    count = [_datesWithEntries count];
    for (*index = 0; *index < count; ++*index) {
        NSComparisonResult result = [date compareDay: [_datesWithEntries objectAtIndex: *index]];
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
    IFURIEntry *entry;
    int dateIndex;

    entry = [_urlDictionary objectForKey: urlString];
    if (entry == nil) {
        return NO;
    }

    [_urlDictionary removeObjectForKey: urlString];

    if ([self findIndex: &dateIndex forDay: [entry lastVisitedDate]]) {
        NSMutableArray *entriesForDate = [_entriesByDate objectAtIndex: dateIndex];
        [entriesForDate removeObject: entry];

        // remove this date entirely if there are no other entries on it
        if ([entriesForDate count] == 0) {
            [_entriesByDate removeObjectAtIndex: dateIndex];
            [_datesWithEntries removeObjectAtIndex: dateIndex];
        }
    } else {
        WEBKITDEBUG2("'%s' was in url dictionary but its date %s was not in date index",
              DEBUG_OBJECT([entry url]), DEBUG_OBJECT([entry lastVisitedDate]));
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
        [_entriesByDate insertObject: [[NSMutableArray alloc] initWithObjects: entry, nil] atIndex: dateIndex];
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

- (BOOL)removeEntriesForDay: (NSCalendarDate *)calendarDate
{
    int index;
    NSEnumerator *entriesForDay;
    IFURIEntry *entry;

    if (![self findIndex: &index forDay: calendarDate]) {
        return NO;
    }

    entriesForDay = [[_entriesByDate objectAtIndex: index] objectEnumerator];
    while ((entry = [entriesForDay nextObject]) != nil) {
        [_urlDictionary removeObjectForKey: [[entry url] absoluteString]];
    }

    [_datesWithEntries removeObjectAtIndex: index];
    [_entriesByDate removeObjectAtIndex: index];

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

- (BOOL)containsURL: (NSURL *)url
{
    return [_urlDictionary objectForKey: [url absoluteString]] != nil;
}

#pragma mark ARCHIVING/UNARCHIVING

- (NSArray *)arrayRepresentation
{
    int dateCount, dateIndex;
    NSMutableArray *arrayRep;

    arrayRep = [NSMutableArray array];

    dateCount = [_entriesByDate count];
    for (dateIndex = 0; dateIndex < dateCount; ++dateIndex) {
        int entryCount, entryIndex;
        NSArray *entries;

        entries = [_entriesByDate objectAtIndex:dateIndex];
        entryCount = [entries count];
        for (entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
            [arrayRep addObject: [[entries objectAtIndex:entryIndex] dictionaryRepresentation]];
        }
    }

    return arrayRep;
}

static NSString* GetRefPath(FSRef* ref)
{
    // This function was cribbed from NSSavePanel.m
    CFURLRef    url  = CFURLCreateFromFSRef((CFAllocatorRef)NULL, ref);
    CFStringRef path = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    CFRelease(url);
    return [(NSString*)path autorelease];
}

static NSString* FindFolderPath(OSType domainType, OSType folderType)
{
    // create URL from FSRef. Then get path string in our POSIX style that we so know and love
    // This function was cribbed from NSSavePanel.m, 'cept the createFolder parameter flopped.
    FSRef     folderRef;
    if (FSFindFolder(domainType, folderType, YES /*createFolder*/, &folderRef) == noErr) {
        return GetRefPath(&folderRef);
    } else {
        return nil;
    }
}

- (NSString *)historyFilePath
{
    NSFileManager *fileManager;
    NSString *applicationSupportDirectoryPath;
    NSString *appName;
    NSString *appInAppSupportPath;
    BOOL fileExists;
    BOOL isDirectory;

    // FIXME: Ken's going to put the equivalent of FindFolderPath in IFNSFileManagerExtensions; use
    // that when it's available and delete the previous two functions.
    applicationSupportDirectoryPath = FindFolderPath(kUserDomain, kApplicationSupportFolderType);
    if (applicationSupportDirectoryPath == nil) {
        WEBKITDEBUG("FindFolderPath(kUserDomain, kApplicationSupportFolderType) failed\n");
        return nil;
    }

    appName = [[[NSBundle mainBundle] infoDictionary] objectForKey: (NSString *)kCFBundleExecutableKey];
    if (appName == nil) {
        WEBKITDEBUG("Couldn't get application name using kCFBundleExecutableKey\n");
        return nil;
    }

    appInAppSupportPath = [applicationSupportDirectoryPath stringByAppendingPathComponent: appName];
    fileManager = [NSFileManager defaultManager];
    fileExists = [fileManager fileExistsAtPath: appInAppSupportPath isDirectory: &isDirectory];
    if (fileExists && !isDirectory) {
        WEBKITDEBUG1("Non-directory file exists at %s\n", DEBUG_OBJECT(appInAppSupportPath));
        return nil;
    } else if (!fileExists && ![fileManager createDirectoryAtPath: appInAppSupportPath attributes: nil]) {
        WEBKITDEBUG1("Couldn't create directory %s\n", DEBUG_OBJECT(appInAppSupportPath));
        return nil;
    }

    return [appInAppSupportPath stringByAppendingPathComponent: @"History.plist"];
}

- (void)loadHistory
{
    NSString *path;
    NSArray *array;
    int index, count;
    
    path = [self historyFilePath];
    if (path == nil) {
        WEBKITDEBUG("couldn't load history; couldn't find or create directory to store it in\n");
        return;
    }

    array = [NSArray arrayWithContentsOfFile: path];
    if (array == nil) {
        if (![[NSFileManager defaultManager] fileExistsAtPath: path]) {
            WEBKITDEBUG1("no history file found at %s\n",
                         DEBUG_OBJECT(path));
        } else {
            WEBKITDEBUG1("attempt to read history from %s failed; perhaps contents are corrupted\n",
                         DEBUG_OBJECT(path));
        }
        return;
    }

    count = [array count];
    for (index = 0; index < count; ++index) {
        IFURIEntry *entry = [[IFURIEntry alloc] initFromDictionaryRepresentation:
            [array objectAtIndex: index]];
        [self addEntry: entry];
    }
}

- (void)saveHistory
{
    NSString *path;
    NSArray *array;

    path = [self historyFilePath];
    if (path == nil) {
        WEBKITDEBUG("couldn't save history; couldn't find or create directory to store it in\n");
        return;
    }

    array = [self arrayRepresentation];
    if (![array writeToFile:path atomically:NO]) {
        WEBKITDEBUG2("attempt to save %s to %s failed\n", DEBUG_OBJECT(array), DEBUG_OBJECT(path));
    }
}

@end
