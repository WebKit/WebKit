/*	IFDatabase.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Foundation/NSPrivateDecls.h>
#import "IFDatabase.h"

// implementation IFDatabase ------------------------------------------------------------------------

@implementation IFDatabase

-(void)setObject:(id)object forKey:(id)key
{
    NSRequestConcreteImplementation(self, _cmd, [IFDatabase class]);
}

-(void)removeObjectForKey:(id)key
{
    NSRequestConcreteImplementation(self, _cmd, [IFDatabase class]);
}

-(void)removeAllObjects
{
    NSRequestConcreteImplementation(self, _cmd, [IFDatabase class]);
}

-(id)objectForKey:(id)key
{
    NSRequestConcreteImplementation(self, _cmd, [IFDatabase class]);
    return nil;
}

-(NSEnumerator *)keys
{
    NSRequestConcreteImplementation(self, _cmd, [IFDatabase class]);
    return nil;
}

-(unsigned)count
{
    return count;
}

@end


// implementation IFDatabase (IFDatabaseCreation) --------------------------------------------------------

@implementation IFDatabase (IFDatabaseCreation)

-(id)initWithPath:(NSString *)thePath
{
    if ((self = [super init])) {
    
        path = [[thePath stringByStandardizingPath] copy];
        isOpen = NO;
    
        return self;
    }
    
    return nil;
}

-(void)dealloc
{
    [path release];

    [super dealloc];
}

@end


// implementation IFDatabase (IFDatabaseManagement) ------------------------------------------------------

@implementation IFDatabase (IFDatabaseManagement)

-(BOOL)open
{
    NSRequestConcreteImplementation(self, _cmd, [IFDatabase class]);
    return NO;
}

-(BOOL)close
{
    NSRequestConcreteImplementation(self, _cmd, [IFDatabase class]);
    return NO;
}

-(void)sync
{
    NSRequestConcreteImplementation(self, _cmd, [IFDatabase class]);
}

-(NSString *)path
{
    return path;
}

-(BOOL)isOpen
{
    return isOpen;
}

@end
