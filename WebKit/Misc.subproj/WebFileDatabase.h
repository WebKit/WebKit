/*	WebFileDatabase.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import "WebDatabase.h"
#import "WebLRUFileList.h"

@interface WebFileDatabase : WebDatabase 
{
    WebLRUFileList *lru;
    NSMutableArray *ops;
    NSMutableDictionary *setCache;
    NSMutableSet *removeCache;
    NSTimer *timer;
    NSTimeInterval touch;
    NSRecursiveLock *mutex;
}

-(void)performSetObject:(id)object forKey:(id)key;
-(void)performRemoveObjectForKey:(id)key;

@end
