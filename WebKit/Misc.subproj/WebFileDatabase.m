/*	IFURLFileDatabase.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "IFURLFileDatabase.h"
#import "IFNSFileManagerExtensions.h"

static NSNumber *IFURLFileDirectoryPosixPermissions;
static NSNumber *IFURLFilePosixPermissions;

// implementation IFURLFileDatabase -------------------------------------------------------------

@implementation IFURLFileDatabase

// creation functions ---------------------------------------------------------------------------
#pragma mark creation functions

+(void)initialize
{
    // set file perms to owner read/write/execute only
    IFURLFileDirectoryPosixPermissions = [[NSNumber numberWithInt:700] retain];

    // set file perms to owner read/write only
    IFURLFilePosixPermissions = [[NSNumber numberWithInt:600] retain];
}

-(id)initWithPath:(NSString *)thePath
{
    if ((self = [super initWithPath:thePath])) {
        return self;
    }
    
    return nil;
}

-(void)dealloc
{
    [self close];

    [super dealloc];
}

+(NSString *)uniqueFilePathForKey:(id)key
{
    const char *s;
    UInt32 hash1;
    UInt32 hash2;
    CFIndex len;
    CFIndex cnt;

    s = [[[[key description] lowercaseString] stringByStandardizingPath] lossyCString];
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

    // create the path and return it
    return [NSString stringWithFormat:@"%.2u/%.2u/%.10u-%.10u.cache", ((hash1 & 0xff) >> 4), ((hash2 & 0xff) >> 4), hash1, hash2];
}

// database functions ---------------------------------------------------------------------------
#pragma mark database functions


-(void)setObject:(id)object forKey:(id)key
{
    BOOL result;
    NSString *filePath;
    NSMutableData *data;
    NSDictionary *attributes;
    NSDictionary *directoryAttributes;
    NSArchiver *archiver;

    result = NO;

    data = [NSMutableData data];
    archiver = [[NSArchiver alloc] initForWritingWithMutableData:data];
    [archiver encodeRootObject:key];
    [archiver encodeRootObject:object];
    
    // FIXME: [kocienda] Radar 2859368 (IFURLFileDatabase must set correct permissions when creating files)
    attributes = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSDate date], @"NSFileModificationDate",
        NSUserName(), @"NSFileOwnerAccountName",
        NULL
    ];

    directoryAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSDate date], @"NSFileModificationDate",
        NSUserName(), @"NSFileOwnerAccountName",
        NULL
    ];

//        IFURLFilePosixPermissions, @"NSFilePosixPermissions",
    filePath = [NSString stringWithFormat:@"%@/%@", path, [IFURLFileDatabase uniqueFilePathForKey:key]];
    result = [[NSFileManager defaultManager] createFileAtPathWithIntermediateDirectories:filePath contents:data attributes:attributes directoryAttributes:directoryAttributes];

    [archiver release];
}

-(void)removeObjectForKey:(id)key
{
    NSString *filePath;

    filePath = [NSString stringWithFormat:@"%@/%@", path, [IFURLFileDatabase uniqueFilePathForKey:key]];
    [[NSFileManager defaultManager] removeFileAtPath:filePath handler:nil];
}

// FIXME: [kocienda] Radar 2861446 (Implement removeAllObjects on concrete IFDatabase classes)
-(void)removeAllObjects
{
    [self close];
    [[NSFileManager defaultManager] removeFileAtPath:path handler:nil];
    [self open];
}

-(id)objectForKey:(id)key
{
    id result;
    id fileKey;
    id object;
    NSString *filePath;
    NSData *data;
    NSUnarchiver *unarchiver;
        
    result = nil;
    fileKey = nil;

    filePath = [NSString stringWithFormat:@"%@/%@", path, [IFURLFileDatabase uniqueFilePathForKey:key]];
    
    data = [[NSFileManager defaultManager] contentsAtPath:filePath];
    if (data) {
        unarchiver = [[NSUnarchiver alloc] initForReadingWithData:data];
        fileKey = [unarchiver decodeObject];
        object = [unarchiver decodeObject];
        if ([fileKey isEqual:key]) {
            // make sure this object stays around until client has had a chance at it
            result = [object retain];
            [result autorelease];
        }
        [unarchiver release];
    }

    return result;
}

-(NSEnumerator *)keys
{
    // FIXME: [kocienda] Radar 2859370 (IFURLFileDatabase needs to implement keys method)
    return nil;
}


// database management functions ---------------------------------------------------------------------------
#pragma mark database management functions

-(BOOL)open
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
        else {
            isOpen = [manager createDirectoryAtPathWithIntermediateDirectories:path attributes:[NSDictionary dictionaryWithObjectsAndKeys:
                [NSDate date], @"NSFileModificationDate",
                NSUserName(), @"NSFileOwnerAccountName",
                NULL
            ]];
        }
    }
    
    return isOpen;
}
//                IFURLFileDirectoryPosixPermissions, @"NSFilePosixPermissions",


-(BOOL)close
{
    if (isOpen) {
        isOpen = NO;
    }
    
    return YES;
}

-(void)sync
{
    // no-op for this kind of database
}

@end
