/*	
    WebDatabase.m
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <Foundation/NSPrivateDecls.h>
#import <WebKit/WebDatabase.h>

// implementation WebDatabase ------------------------------------------------------------------------

@implementation WebDatabase

-(void)setObject:(id)object forKey:(id)key
{
    NSRequestConcreteImplementation(self, _cmd, [WebDatabase class]);
}

-(void)removeObjectForKey:(id)key
{
    NSRequestConcreteImplementation(self, _cmd, [WebDatabase class]);
}

-(void)removeAllObjects
{
    NSRequestConcreteImplementation(self, _cmd, [WebDatabase class]);
}

-(id)objectForKey:(id)key
{
    NSRequestConcreteImplementation(self, _cmd, [WebDatabase class]);
    return nil;
}

-(unsigned)count
{
    return count;
}

@end


// implementation WebDatabase (WebDatabaseCreation) --------------------------------------------------------

@implementation WebDatabase (WebDatabaseCreation)

-(id)initWithPath:(NSString *)thePath
{
    if ((self = [super init])) {
    
        path = [[thePath stringByStandardizingPath] copy];
        isOpen = NO;
        sizeLimit = 0;
        usage = 0;
    
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


// implementation WebDatabase (WebDatabaseManagement) ------------------------------------------------------

@implementation WebDatabase (WebDatabaseManagement)

-(void)open
{
    NSRequestConcreteImplementation(self, _cmd, [WebDatabase class]);
}

-(void)close
{
    NSRequestConcreteImplementation(self, _cmd, [WebDatabase class]);
}

-(void)sync
{
    NSRequestConcreteImplementation(self, _cmd, [WebDatabase class]);
}

-(NSString *)path
{
    return path;
}

-(BOOL)isOpen
{
    return isOpen;
}

-(unsigned)count
{
    NSRequestConcreteImplementation(self, _cmd, [WebDatabase class]);
    return 0;
}

-(unsigned)sizeLimit
{
    return sizeLimit;
}

-(void)setSizeLimit:(unsigned)limit
{
    NSRequestConcreteImplementation(self, _cmd, [WebDatabase class]);
}

-(unsigned)usage
{
    return usage;
}

@end
