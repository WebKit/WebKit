/*	IFDatabase.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>


@interface IFDatabase : NSObject 
{
    NSString *path;
    unsigned count;
    BOOL isOpen;
}

-(void)setObject:(id)object forKey:(id)key;
-(void)removeObjectForKey:(id)key;
-(void)removeAllObjects;
-(id)objectForKey:(id)key;
-(NSEnumerator *)keys;
-(unsigned)count;

@end


@interface IFDatabase (IFDatabaseCreation)

-(id)initWithPath:(NSString *)thePath;

@end


@interface IFDatabase (IFDatabaseManagement)

-(BOOL)open;
-(BOOL)close;
-(void)sync;

-(NSString *)path;
-(BOOL)isOpen;

@end
