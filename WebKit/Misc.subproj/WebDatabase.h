/*	
    WebDatabase.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <Foundation/Foundation.h>

@interface WebDatabase : NSObject 
{
    NSString *path;
    unsigned count;
    BOOL isOpen;
    unsigned sizeLimit;
    unsigned usage;
}

- (void)setObject:(id)object forKey:(id)key;
- (void)removeObjectForKey:(id)key;
- (void)removeAllObjects;
- (id)objectForKey:(id)key;

@end


@interface WebDatabase (WebDatabaseCreation)

- (id)initWithPath:(NSString *)thePath;

@end


@interface WebDatabase (WebDatabaseManagement)

- (void)open;
- (void)close;
- (void)sync;

- (NSString *)path;
- (BOOL)isOpen;

- (unsigned)count;
- (unsigned)sizeLimit;
- (void)setSizeLimit:(unsigned)limit;
- (unsigned)usage;

@end
