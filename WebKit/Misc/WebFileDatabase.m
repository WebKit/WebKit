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

#import <WebKit/WebFileDatabase.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNSFileManagerExtras.h>

#import <fcntl.h>
#import <fts.h>
#import <string.h>
#import <sys/stat.h>
#import <sys/types.h>
#import <sys/mman.h>

#if ERROR_DISABLED
#define BEGIN_EXCEPTION_HANDLER
#define END_EXCEPTION_HANDLER
#else
#define BEGIN_EXCEPTION_HANDLER NS_DURING
#define END_EXCEPTION_HANDLER \
    NS_HANDLER \
        LOG_ERROR("Uncaught exception: %@ [%@] [%@]", [localException class], [localException reason], [localException userInfo]); \
    NS_ENDHANDLER
#endif

#define UniqueFilePathSize (34)

static void UniqueFilePathForKey(id key, char *buffer)
{
    const char *s;
    UInt32 hash1;
    UInt32 hash2;
    CFIndex len;
    CFIndex cnt;
    
    s = [[[[key description] lowercaseString] stringByStandardizingPath] UTF8String];
    len = strlen(s);

    // compute first hash    
    hash1 = len;
    for (cnt = 0; cnt < len; cnt++) {
        hash1 += (hash1 << 8) + s[cnt];
    }
    hash1 += (hash1 << (len & 31));

    // compute second hash    
    hash2 = len;
    for (cnt = 0; cnt < len; cnt++) {
        hash2 = (37 * hash2) ^ s[cnt];
    }

#ifdef __LP64__
    snprintf(buffer, UniqueFilePathSize, "%.2u/%.2u/%.10u-%.10u.cache", ((hash1 & 0xff) >> 4), ((hash2 & 0xff) >> 4), hash1, hash2);
#else
    snprintf(buffer, UniqueFilePathSize, "%.2lu/%.2lu/%.10lu-%.10lu.cache", ((hash1 & 0xff) >> 4), ((hash2 & 0xff) >> 4), hash1, hash2);
#endif
}

// implementation WebFileDatabase -------------------------------------------------------------

@implementation WebFileDatabase

// creation functions ---------------------------------------------------------------------------
#pragma mark creation functions

-(id)initWithPath:(NSString *)thePath
{
    if (!(self = [super init])) 
        return nil;
        
    path = [[thePath stringByStandardizingPath] copy];
    if (thePath == nil) {
        [self release];
        return nil;
    }
    
    isOpen = NO;
    sizeLimit = 0;
    
    return self;
}

-(void)dealloc
{
    [path release];
    [super dealloc];
}

// database functions ---------------------------------------------------------------------------
#pragma mark database functions

-(id)objectForKey:(id)key
{
    volatile id result;
    
    ASSERT(key);

    // go to disk
    char uniqueKey[UniqueFilePathSize];
    UniqueFilePathForKey(key, uniqueKey);
    NSString *filePath = [[NSString alloc] initWithFormat:@"%@/%s", path, uniqueKey];
    NSData *data = [[NSData alloc] initWithContentsOfFile:filePath];
    NSUnarchiver * volatile unarchiver = nil;

    NS_DURING
        if (data) {
            unarchiver = [[NSUnarchiver alloc] initForReadingWithData:data];
            if (unarchiver) {
                id fileKey = [unarchiver decodeObject];
                if ([fileKey isEqual:key]) {
                    id object = [unarchiver decodeObject];
                    if (object) {
                        // Decoded objects go away when the unarchiver does, so we need to
                        // retain this so we can return it to our caller.
                        result = [[object retain] autorelease];
                        LOG(FileDatabaseActivity, "read disk cache file - %@", key);
                    }
                }
            }
        }
    NS_HANDLER
        LOG(FileDatabaseActivity, "cannot unarchive cache file - %@", key);
        result = nil;
    NS_ENDHANDLER

    [unarchiver release];
    [data release];
    [filePath release];
    
    return result;
}

// database management functions ---------------------------------------------------------------------------
#pragma mark database management functions

-(void)open
{
    NSFileManager *manager;
    BOOL isDir;
    
    if (!isOpen) {
        manager = [NSFileManager defaultManager];
        if ([manager fileExistsAtPath:path isDirectory:&isDir]) {
            if (isDir) {
                isOpen = YES;
            }
        }
        // remove any leftover turds
        [manager _webkit_backgroundRemoveLeftoverFiles:path];
    }
}

-(void)close
{
    isOpen = NO;
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
