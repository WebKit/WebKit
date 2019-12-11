/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "SearchPopupMenuCocoa.h"

namespace WebCore {

static NSString * const dateKey = @"date";
static NSString * const itemsKey = @"items";
static NSString * const searchesKey = @"searches";
static NSString * const searchStringKey = @"searchString";

static NSString *searchFieldRecentSearchesStorageDirectory()
{
    NSString *appName = [[NSBundle mainBundle] bundleIdentifier];
    if (!appName)
        appName = [[NSProcessInfo processInfo] processName];
    
    return [[NSHomeDirectory() stringByAppendingPathComponent:@"Library/WebKit"] stringByAppendingPathComponent:appName];
}

static NSString *searchFieldRecentSearchesPlistPath()
{
    return [searchFieldRecentSearchesStorageDirectory() stringByAppendingPathComponent:@"RecentSearches.plist"];
}

static RetainPtr<NSMutableDictionary> readSearchFieldRecentSearchesPlist()
{
    return adoptNS([[NSMutableDictionary alloc] initWithContentsOfFile:searchFieldRecentSearchesPlistPath()]);
}

static WallTime toSystemClockTime(NSDate *date)
{
    ASSERT(date);
    return WallTime::fromRawSeconds(date.timeIntervalSince1970);
}

static NSDate *toNSDateFromSystemClock(WallTime time)
{
    return [NSDate dateWithTimeIntervalSince1970:time.secondsSinceEpoch().seconds()];
}

static NSMutableArray *typeCheckedRecentSearchesArray(NSMutableDictionary *itemsDictionary, NSString *name)
{
    NSMutableDictionary *nameDictionary = [itemsDictionary objectForKey:name];
    if (![nameDictionary isKindOfClass:[NSDictionary class]])
        return nil;

    NSMutableArray *recentSearches = [nameDictionary objectForKey:searchesKey];
    if (![recentSearches isKindOfClass:[NSArray class]])
        return nil;

    return recentSearches;
}

static NSDate *typeCheckedDateInRecentSearch(NSDictionary *recentSearch)
{
    if (![recentSearch isKindOfClass:[NSDictionary class]])
        return nil;

    NSDate *date = [recentSearch objectForKey:dateKey];
    if (![date isKindOfClass:[NSDate class]])
        return nil;

    return date;
}

static RetainPtr<NSDictionary> typeCheckedRecentSearchesRemovingRecentSearchesAddedAfterDate(NSDate *date)
{
    if ([date isEqualToDate:[NSDate distantPast]])
        return nil;

    RetainPtr<NSMutableDictionary> recentSearchesPlist = readSearchFieldRecentSearchesPlist();
    NSMutableDictionary *itemsDictionary = [recentSearchesPlist objectForKey:itemsKey];
    if (![itemsDictionary isKindOfClass:[NSDictionary class]])
        return nil;

    RetainPtr<NSMutableArray> keysToRemove = adoptNS([[NSMutableArray alloc] init]);
    for (NSString *key in itemsDictionary) {
        if (![key isKindOfClass:[NSString class]])
            return nil;

        NSMutableArray *recentSearches = typeCheckedRecentSearchesArray(itemsDictionary, key);
        if (!recentSearches)
            return nil;

        RetainPtr<NSMutableArray> entriesToRemove = adoptNS([[NSMutableArray alloc] init]);
        for (NSDictionary *recentSearch in recentSearches) {
            NSDate *dateAdded = typeCheckedDateInRecentSearch(recentSearch);
            if (!dateAdded)
                return nil;

            if ([dateAdded compare:date] == NSOrderedDescending)
                [entriesToRemove addObject:recentSearch];
        }

        [recentSearches removeObjectsInArray:entriesToRemove.get()];
        if (!recentSearches.count)
            [keysToRemove addObject:key];
    }

    [itemsDictionary removeObjectsForKeys:keysToRemove.get()];
    if (!itemsDictionary.count)
        return nil;

    return recentSearchesPlist;
}

static void writeEmptyRecentSearchesPlist()
{
    auto emptyItemsDictionary = adoptNS([[NSDictionary alloc] init]);
    auto emptyRecentSearchesDictionary = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:emptyItemsDictionary.get(), itemsKey, nil]);
    [emptyRecentSearchesDictionary writeToFile:searchFieldRecentSearchesPlistPath() atomically:YES];
}

void saveRecentSearches(const String& name, const Vector<RecentSearch>& searchItems)
{
    if (name.isEmpty())
        return;

    RetainPtr<NSDictionary> recentSearchesPlist = readSearchFieldRecentSearchesPlist();
    RetainPtr<NSMutableDictionary> itemsDictionary = [recentSearchesPlist objectForKey:itemsKey];
    // The NSMutableDictionary method we use to read the property list guarantees we get only
    // mutable containers, but it does not guarantee the file has a dictionary as expected.
    if (![itemsDictionary isKindOfClass:[NSDictionary class]]) {
        itemsDictionary = adoptNS([[NSMutableDictionary alloc] init]);
        recentSearchesPlist = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:itemsDictionary.get(), itemsKey, nil]);
    }

    if (searchItems.isEmpty())
        [itemsDictionary removeObjectForKey:name];
    else {
        RetainPtr<NSMutableArray> items = adoptNS([[NSMutableArray alloc] initWithCapacity:searchItems.size()]);
        for (auto& searchItem : searchItems)
            [items addObject:adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:searchItem.string, searchStringKey, toNSDateFromSystemClock(searchItem.time), dateKey, nil]).get()];

        [itemsDictionary setObject:adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:items.get(), searchesKey, nil]).get() forKey:name];
    }

    [recentSearchesPlist writeToFile:searchFieldRecentSearchesPlistPath() atomically:YES];
}

Vector<RecentSearch> loadRecentSearches(const String& name)
{
    Vector<RecentSearch> searchItems;

    if (name.isEmpty())
        return searchItems;

    RetainPtr<NSMutableDictionary> recentSearchesPlist = readSearchFieldRecentSearchesPlist();
    if (!recentSearchesPlist)
        return searchItems;

    NSMutableDictionary *items = [recentSearchesPlist objectForKey:itemsKey];
    if (![items isKindOfClass:[NSDictionary class]])
        return searchItems;

    NSArray *recentSearches = typeCheckedRecentSearchesArray(items, name);
    if (!recentSearches)
        return searchItems;
    
    for (NSDictionary *item in recentSearches) {
        NSDate *date = typeCheckedDateInRecentSearch(item);
        if (!date)
            continue;

        NSString *searchString = [item objectForKey:searchStringKey];
        if (![searchString isKindOfClass:[NSString class]])
            continue;
        
        searchItems.append({ String{ searchString }, toSystemClockTime(date) });
    }

    return searchItems;
}

void removeRecentlyModifiedRecentSearches(WallTime oldestTimeToRemove)
{
    NSDate *date = toNSDateFromSystemClock(oldestTimeToRemove);
    auto recentSearchesPlist = typeCheckedRecentSearchesRemovingRecentSearchesAddedAfterDate(date);
    if (recentSearchesPlist)
        [recentSearchesPlist writeToFile:searchFieldRecentSearchesPlistPath() atomically:YES];
    else
        writeEmptyRecentSearchesPlist();
}

}
