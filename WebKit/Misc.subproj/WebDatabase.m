/*	
    WebDatabase.m
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <WebKit/WebDatabase.h>
#import <WebKit/WebAssertions.h>

// implementation WebDatabase ------------------------------------------------------------------------

@implementation WebDatabase

-(void)setObject:(id)object forKey:(id)key
{
    ASSERT_NOT_REACHED();
}

-(void)removeObjectForKey:(id)key
{
    ASSERT_NOT_REACHED();
}

-(void)removeAllObjects
{
    ASSERT_NOT_REACHED();
}

-(id)objectForKey:(id)key
{
    ASSERT_NOT_REACHED();
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
    ASSERT_NOT_REACHED();
}

-(void)close
{
    ASSERT_NOT_REACHED();
}

-(void)sync
{
    ASSERT_NOT_REACHED();
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
    ASSERT_NOT_REACHED();
    return 0;
}

-(unsigned)sizeLimit
{
    return sizeLimit;
}

-(void)setSizeLimit:(unsigned)limit
{
    ASSERT_NOT_REACHED();
}

-(unsigned)usage
{
    return usage;
}

@end
