/*
    WebBackForwardList.m
    Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebHistoryItemPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebPreferencesPrivate.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebSystemBits.h>

@implementation WebBackForwardList

- (id)init
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    _entries = [[NSMutableArray alloc] init];
    _current = -1;
    _maximumSize = 100;		// typically set by browser app

    return self;
}

- (void)dealloc
{
    unsigned i;
    for (i = 0; i < [_entries count]; i++){
        WebHistoryItem *item = [_entries objectAtIndex: i];
        [item setHasPageCache: NO]; 
    }
    [_entries release];
    [super dealloc];
}

- (void)addEntry:(WebHistoryItem *)entry;
{
    // Toss anything in the forward list
    int currSize = [_entries count];
    if (_current != currSize-1 && _current != -1) {
        NSRange forwardRange = NSMakeRange(_current+1, currSize-(_current+1));
        NSArray *subarray;
        subarray = [_entries subarrayWithRange:forwardRange];
        unsigned i;
        for (i = 0; i < [subarray count]; i++){
            WebHistoryItem *item = [subarray objectAtIndex: i];
            [item setHasPageCache: NO];            
        }
        [_entries removeObjectsInRange: forwardRange];
        currSize -= forwardRange.length;
    }

    // Toss the first item if the list is getting too big, as long as we're not using it
    if (currSize == _maximumSize && _current != 0) {
        WebHistoryItem *item = [_entries objectAtIndex: 0];
        [item setHasPageCache: NO];
        [_entries removeObjectAtIndex:0];
        currSize--;
        _current--;
    }

    [_entries addObject:entry];
    _current++;
}

- (void)goBack
{
    ASSERT(_current > 0);
    _current--;
}

- (void)goForward
{
    ASSERT(_current < (int)[_entries count]-1);
    _current++;
}

- (void)goToEntry:(WebHistoryItem *)entry
{
    int index = [_entries indexOfObjectIdenticalTo:entry];
    ASSERT(index != NSNotFound);
    _current = index;
}

- (WebHistoryItem *)backEntry
{
    if (_current > 0) {
        return [_entries objectAtIndex:_current-1];
    } else {
        return nil;
    }
}

- (WebHistoryItem *)currentEntry;
{
    if (_current >= 0) {
        return [_entries objectAtIndex:_current];
    } else {
        return nil;
    }
}

- (WebHistoryItem *)forwardEntry
{
    if (_current < (int)[_entries count]-1) {
        return [_entries objectAtIndex:_current+1];
    } else {
        return nil;
    }
}

- (BOOL)containsEntry:(WebHistoryItem *)entry
{
    return [_entries indexOfObjectIdenticalTo:entry] != NSNotFound;
}

- (NSArray *)backListWithSizeLimit:(int)limit;
{
    if (_current > 0) {
        NSRange r;
        r.location = MAX(_current-limit, 0);
        r.length = _current - r.location;
        return [_entries subarrayWithRange:r];
    } else {
        return nil;
    }
}

- (NSArray *)forwardListWithSizeLimit:(int)limit;
{
    int lastEntry = (int)[_entries count]-1;
    if (_current < lastEntry) {
        NSRange r;
        r.location = _current+1;
        r.length =  MIN(_current+limit, lastEntry) - _current;
        return [_entries subarrayWithRange:r];
    } else {
        return nil;
    }
}

- (int)maximumSize
{
    return _maximumSize;
}

- (void)setMaximumSize:(int)size
{
    _maximumSize = size;
}


-(NSString *)description
{
    NSMutableString *result;
    int i;
    
    result = [NSMutableString stringWithCapacity:512];
    
    [result appendString:@"\n--------------------------------------------\n"];    
    [result appendString:@"WebBackForwardList:\n"];
    
    for (i = 0; i < (int)[_entries count]; i++) {
        if (i == _current) {
            [result appendString:@" >>>"]; 
        }
        else {
            [result appendString:@"    "]; 
        }   
        [result appendFormat:@"%2d) ", i];
        int currPos = [result length];
        [result appendString:[[_entries objectAtIndex:i] description]];

        // shift all the contents over.  a bit slow, but this is for debugging
        NSRange replRange = {currPos, [result length]-currPos};
        [result replaceOccurrencesOfString:@"\n" withString:@"\n        " options:0 range:replRange];
        
        [result appendString:@"\n"];
    }

    [result appendString:@"\n--------------------------------------------\n"];    

    return result;
}

- (void)clearPageCache
{
    int i;
    for (i = 0; i < (int)[_entries count]; i++) {
        [[_entries objectAtIndex:i] setHasPageCache:NO];
    }
}

static BOOL pageCacheSizeModified = NO;
static unsigned pageCacheSize = 4;

+ (void)setPageCacheSize: (unsigned)size
{
    pageCacheSizeModified = YES;
    pageCacheSize = size;
}

#ifndef NDEBUG
static BOOL loggedPageCacheSize = NO;
#endif

+ (unsigned)pageCacheSize
{
    if (!pageCacheSizeModified){
        unsigned s;
        vm_size_t memSize = WebSystemMainMemory();
        unsigned multiplier = 1;
        
        s = [[WebPreferences standardPreferences] _pageCacheSize];
        if (memSize > 1024 * 1024 * 1024)
            multiplier = 4;
        else if (memSize > 512 * 1024 * 1024)
            multiplier = 2;

#ifndef NDEBUG
        if (!loggedPageCacheSize){
            LOG (CacheSizes, "Page cache size set to %d pages.", s * multiplier);
            loggedPageCacheSize = YES;
        }
#endif

        return s * multiplier;
    }
    return pageCacheSize;
}

// On be default for now.
static BOOL usesPageCache = YES;

+ (void)setUsesPageCache: (BOOL)f
{
    usesPageCache = f ? YES : NO;
}

+ (BOOL)usesPageCache
{
    if ([WebBackForwardList pageCacheSize] == 0)
        return NO;
    return usesPageCache;
}

- (int)backListCount
{
    return _current;
}

- (WebHistoryItem *)entryAtIndex:(int)index
{
    // Do range checks without doing math on index to avoid overflow.
    if (index < -_current) {
        return [_entries objectAtIndex:0];
    }
    if (index >= (int)[_entries count] - _current) {
        return [_entries lastObject];
    }
    return [_entries objectAtIndex:index + _current];
}

@end
