/*	IFDatabase.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>


@interface IFDatabase : NSObject 
{
    NSString *path;
    unsigned count;
    BOOL isOpen;
    unsigned sizeLimit;
    unsigned usage;
}

-(void)setObject:(id)object forKey:(id)key;
-(void)removeObjectForKey:(id)key;
-(void)removeAllObjects;
-(id)objectForKey:(id)key;
-(NSEnumerator *)keys;

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

-(unsigned)count;
-(unsigned)sizeLimit;
-(void)setSizeLimit:(unsigned)limit;
-(unsigned)usage;

@end
