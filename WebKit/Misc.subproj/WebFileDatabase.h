/*	
    WebFileDatabase.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <Foundation/Foundation.h>

#import "WebDatabase.h"

@interface WebFileDatabase : WebDatabase 
{
    struct WebLRUFileList *lru;
    NSMutableArray *ops;
    NSMutableDictionary *setCache;
    NSMutableSet *removeCache;
    NSTimer *timer;
    NSTimeInterval touch;
    NSRecursiveLock *mutex;
}

- (void)performSetObject:(id)object forKey:(id)key;
- (void)performRemoveObjectForKey:(id)key;

@end
