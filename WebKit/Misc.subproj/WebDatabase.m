/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
