//
//  IFBackForwardList.m
//  WebKit
//
//  Created by Kenneth Kocienda on Thu Nov 29 2001.
//  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
//

#import "IFBackForwardList.h"

#import "IFURIEntry.h"
#import "IFURIList.h"

@implementation IFBackForwardList

-(id)init
{
    if (self != [super init])
    {
        return nil;
    }
    
    uriList = [[IFURIList alloc] init];
    [uriList setAllowsDuplicates:YES];
    index = 0;
    mutex = [[NSLock alloc] init];

    return self;
}

-(void)dealloc
{
    [uriList release];
    [mutex release];

    [super dealloc];
}

-(void)addEntry:(IFURIEntry *)entry
{
    [mutex lock];
    if (index > 0) {
        [uriList removeEntriesToIndex:index];
        index = 0;
    }
    [uriList addEntry:entry];
    [mutex unlock];
}

-(void)goBack
{
    [mutex lock];
    index++;
    [mutex unlock];
}

-(IFURIEntry *)backEntry
{
    IFURIEntry *result;
    int count;
    
    [mutex lock];
    count = [uriList count];
    if (count > 1 && index < (count - 1)) {
        result = [uriList entryAtIndex:index+1];
    } else {
        result = nil;
    }
    [mutex unlock];

    return result;
}

-(IFURIEntry *)currentEntry
{
    IFURIEntry *result;
    
    [mutex lock];
    result = [uriList entryAtIndex:index];
    [mutex unlock];

    return result;
}

-(IFURIEntry *)forwardEntry
{
    IFURIEntry *result;

    [mutex lock];
    if (index > 0) {
        result = [uriList entryAtIndex:index-1];
    } else {
        result = nil;
    }
    [mutex unlock];

    return result;
}

-(void)goForward
{
    [mutex lock];
    index--;
    [mutex unlock];
}

-(BOOL)canGoBack
{
    BOOL result;
    int count;
    
    [mutex lock];
    count = [uriList count];
    result = (count > 1 && index < (count - 1));
    [mutex unlock];
    
    return result;
}

-(BOOL)canGoForward
{
    BOOL result;

    [mutex lock];
    result = (index > 0);
    [mutex unlock];
    
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

    [mutex lock];
    
    result = [NSMutableString stringWithCapacity:512];
    
    [result appendString:@"\n--------------------------------------------\n"];    
    [result appendString:@"IFBackForwardList:\n"];
    
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

    [mutex unlock];

    return result;
}

@end
