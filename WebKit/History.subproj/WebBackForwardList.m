//
//  WebBackForwardList.m
//  WebKit
//
//  Created by Kenneth Kocienda on Thu Nov 29 2001.
//  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
//

#import "WebBackForwardList.h"

#import "WebHistoryItem.h"
#import "WebHistoryList.h"

@implementation WebBackForwardList

-(id)init
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    uriList = [[WebHistoryList alloc] init];
    [uriList setAllowsDuplicates:YES];
    index = 0;

    return self;
}

-(void)dealloc
{
    [uriList release];
    [super dealloc];
}

-(void)addEntry:(WebHistoryItem *)entry
{
    // If the last entry matches this new entry, don't add a new one, since we are
    // presumably just doing a reload.
    if ([uriList count] > index) {
        WebHistoryItem *lastEntry = [uriList entryAtIndex:index];
        if ([[lastEntry URL] isEqual:[entry URL]]
            && ([lastEntry target] == [entry target] || [[lastEntry target] isEqual:[entry target]])
            && ([lastEntry parent] == [entry parent] || [[lastEntry parent] isEqual:[entry parent]])) {
            return;
        }
    }

    if (index > 0) {
        [uriList removeEntriesToIndex:index];
        index = 0;
    }
    [uriList addEntry:entry];
}

-(void)goBack
{
    index++;
}

-(void)goBackToIndex: (int)pos
{
}

- (WebHistoryItem *)backEntryAtIndex: (int)pos
{
    WebHistoryItem *result;
    int count;
    
    count = [uriList count];
    if (count > 1 && index+pos < (count - 1)) {
        result = [uriList entryAtIndex:index+1+pos];
    } else {
        result = nil;
    }

    return result;
}

-(WebHistoryItem *)backEntry
{
    WebHistoryItem *result;
    int count;
    
    count = [uriList count];
    if (count > 1 && index < (count - 1)) {
        result = [uriList entryAtIndex:index+1];
    } else {
        result = nil;
    }

    return result;
}

-(WebHistoryItem *)currentEntry
{
    WebHistoryItem *result;
    
    if (index < 0 || index >= [uriList count])
        return  nil;
        
    result = [uriList entryAtIndex:index];

    return result;
}

-(WebHistoryItem *)forwardEntry
{
    WebHistoryItem *result;

    if (index > 0) {
        result = [uriList entryAtIndex:index-1];
    } else {
        result = nil;
    }

    return result;
}

-(void)goForward
{
    index--;
}

-(void)goForwardToIndex: (int)pos
{
}

-(BOOL)canGoBack
{
    BOOL result;
    int count;
    
    count = [uriList count];
    result = (count > 1 && index < (count - 1));
    
    return result;
}

-(BOOL)canGoForward
{
    BOOL result;

    result = (index > 0);
    
    return result;
}

-(NSArray *)backList
{
    // FIXME: implement this some day
    return nil;
}

-(NSArray *)forwardList
{
    // FIXME: implement this some day
    return nil;
}

-(NSString *)description
{
    NSMutableString *result;
    int i;
    
    result = [NSMutableString stringWithCapacity:512];
    
    [result appendString:@"\n--------------------------------------------\n"];    
    [result appendString:@"WebBackForwardList:\n"];
    
    for (i = 0; i < [uriList count]; i++) {
        if (i == index) {
            [result appendString:@" >>>"]; 
        }
        else {
            [result appendString:@"    "]; 
        }   
        [result appendFormat:@" %d) ", i]; 
        [result appendString:[[uriList entryAtIndex:i] description]]; 
        [result appendString:@"\n"]; 
    }

    [result appendString:@"\n--------------------------------------------\n"];    

    return result;
}

@end
