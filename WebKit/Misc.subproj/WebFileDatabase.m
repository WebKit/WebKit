/*	IFURLFileDatabase.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "IFURLFileDatabase.h"
#import "IFNSFileManagerExtensions.h"
#import "IFURLCacheLoaderConstantsPrivate.h"
#import "WebFoundationDebug.h"

static NSNumber *IFURLFileDirectoryPosixPermissions;
static NSNumber *IFURLFilePosixPermissions;

typedef enum
{
    IFURLFileDatabaseSetObjectOp,
    IFURLFileDatabaseRemoveObjectOp,
} IFURLFileDatabaseOpCode;

enum
{
    SYNC_INTERVAL = 5,
    SYNC_IDLE_THRESHOLD = 5,
};

// interface IFURLFileDatabaseOp -------------------------------------------------------------

@interface IFURLFileDatabaseOp : NSObject
{
    IFURLFileDatabaseOpCode opcode;
    id key;
    id object; 
}

+(id)opWithCode:(IFURLFileDatabaseOpCode)opcode key:(id)key object:(id)object;
-(id)initWithCode:(IFURLFileDatabaseOpCode)opcode key:(id)key object:(id)object;

-(IFURLFileDatabaseOpCode)opcode;
-(id)key;
-(id)object;
-(void)perform:(IFURLFileDatabase *)target;

@end


// implementation IFURLFileDatabaseOp -------------------------------------------------------------

@implementation IFURLFileDatabaseOp

+(id)opWithCode:(IFURLFileDatabaseOpCode)theOpcode key:(id)theKey object:(id)theObject
{
    return [[[IFURLFileDatabaseOp alloc] initWithCode:theOpcode key:theKey object:theObject] autorelease];
}

-(id)initWithCode:(IFURLFileDatabaseOpCode)theOpcode key:(id)theKey object:(id)theObject
{
    if ((self = [super init])) {
        
        opcode = theOpcode;
        key = [theKey retain];
        object = [theObject retain];
        
        return self;
    }
  
    [self release];
    return nil;
}

-(IFURLFileDatabaseOpCode)opcode
{
    return opcode;
}

-(id)key
{
    return key;
}

-(id)object
{
    return object;
}

-(void)perform:(IFURLFileDatabase *)target
{
    switch (opcode) {
        case IFURLFileDatabaseSetObjectOp:
            [target performSetObject:object forKey:key];
            break;
        case IFURLFileDatabaseRemoveObjectOp:
            [target performRemoveObjectForKey:key];
            break;
    }
}

-(void)dealloc
{
    [key release];
    [object release];
    
    [super dealloc];
}

@end


// implementation IFURLFileDatabase -------------------------------------------------------------

@implementation IFURLFileDatabase

// creation functions ---------------------------------------------------------------------------
#pragma mark creation functions

+(void)initialize
{
    // set file perms to owner read/write/execute only
    IFURLFileDirectoryPosixPermissions = [[NSNumber numberWithInt:(IF_UREAD | IF_UWRITE | IF_UEXEC)] retain];

    // set file perms to owner read/write only
    IFURLFilePosixPermissions = [[NSNumber numberWithInt:(IF_UREAD | IF_UWRITE)] retain];
}

-(id)initWithPath:(NSString *)thePath
{
    if ((self = [super initWithPath:thePath])) {
    
        ops = [[NSMutableArray alloc] init];
        setCache = [[NSMutableDictionary alloc] init];
        removeCache = [[NSMutableSet alloc] init];
        timer = nil;
        mutex = [[NSLock alloc] init];

        return self;
    }
    
    [self release];
    return nil;
}

-(void)dealloc
{
    [self close];
    [self sync];
    
    [setCache release];
    [removeCache release];
    [mutex release];

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

-(void)setTimer
{
    if (timer == nil) {
        timer = [[NSTimer scheduledTimerWithTimeInterval:SYNC_INTERVAL target:self selector:@selector(lazySync:) userInfo:nil repeats:YES] retain];
    }
}

// database functions ---------------------------------------------------------------------------
#pragma mark database functions

-(void)setObject:(id)object forKey:(id)key
{
    IFURLFileDatabaseOp *op;

    touch = CFAbsoluteTimeGetCurrent();
    
    [mutex lock];
    
    [setCache setObject:object forKey:key];
    op = [[IFURLFileDatabaseOp alloc] initWithCode:IFURLFileDatabaseSetObjectOp key:key object:object];
    [ops addObject:op];
    [self setTimer];
    
    [mutex unlock];
}

-(void)removeObjectForKey:(id)key
{
    IFURLFileDatabaseOp *op;

    touch = CFAbsoluteTimeGetCurrent();
    
    [mutex lock];
    
    [removeCache addObject:key];
    op = [[IFURLFileDatabaseOp alloc] initWithCode:IFURLFileDatabaseRemoveObjectOp key:key object:nil];
    [ops addObject:op];
    [self setTimer];
    
    [mutex unlock];
}

-(void)removeAllObjects
{
    touch = CFAbsoluteTimeGetCurrent();

    [mutex lock];
    [setCache removeAllObjects];
    [removeCache removeAllObjects];
    [self close];
    [[NSFileManager defaultManager] backgroundRemoveFileAtPath:path];
    [self open];
    [mutex unlock];

#ifdef WEBFOUNDATION_DEBUG
    WebFoundationLogAtLevel(WebFoundationLogDiskCacheActivity, @"- [WEBFOUNDATION_DEBUG] - removeAllObjects");
#endif
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
    data = nil;
    unarchiver = nil;

    touch = CFAbsoluteTimeGetCurrent();

    // check caches
    [mutex lock];
    if ([removeCache containsObject:key]) {
        [mutex unlock];
        return nil;
    }
    if ((result = [setCache objectForKey:key])) {
        [mutex unlock];
        return result;
    }
    [mutex unlock];

    // go to disk
    filePath = [NSString stringWithFormat:@"%@/%@", path, [IFURLFileDatabase uniqueFilePathForKey:key]];

    NS_DURING
        data = [[NSData alloc] initWithContentsOfMappedFile:filePath];
        if (!data) {
            data = [[NSData alloc] initWithContentsOfFile:filePath];
        }
        if (data) {
            unarchiver = [[NSUnarchiver alloc] initForReadingWithData:data];
        }
        if (unarchiver) {
            fileKey = [unarchiver decodeObject];
            object = [unarchiver decodeObject];
            if (object && [fileKey isEqual:key]) {
                // make sure this object stays around until client has had a chance at it
                result = [object retain];
                [result autorelease];
            }
        }
    NS_HANDLER
#ifdef WEBFOUNDATION_DEBUG
        WebFoundationLogAtLevel(WebFoundationLogDiskCacheActivity, @"- [WEBFOUNDATION_DEBUG] - cannot unarchive cache file - %@", key);
#endif
        result = nil;
    NS_ENDHANDLER

    [unarchiver release];
    [data release];

    return result;
}

-(NSEnumerator *)keys
{
    // FIXME: [kocienda] Radar 2859370 (IFURLFileDatabase needs to implement keys method)
    return nil;
}


-(void)performSetObject:(id)object forKey:(id)key
{
    BOOL result;
    NSString *filePath;
    NSMutableData *data;
    NSDictionary *attributes;
    NSDictionary *directoryAttributes;
    NSArchiver *archiver;
    NSFileManager *defaultManager;

#ifdef WEBFOUNDATION_DEBUG
    WebFoundationLogAtLevel(WebFoundationLogDiskCacheActivity, @"- [WEBFOUNDATION_DEBUG] - performSetObject - %@ - %@", key, [IFURLFileDatabase uniqueFilePathForKey:key]);
#endif

    result = NO;

    data = [NSMutableData data];
    archiver = [[NSArchiver alloc] initForWritingWithMutableData:data];
    [archiver encodeObject:key];
    [archiver encodeObject:object];
    
    attributes = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSDate date], @"NSFileModificationDate",
        NSUserName(), @"NSFileOwnerAccountName",
        IFURLFilePosixPermissions, @"NSFilePosixPermissions",
        NULL
    ];

    directoryAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSDate date], @"NSFileModificationDate",
        NSUserName(), @"NSFileOwnerAccountName",
        IFURLFileDirectoryPosixPermissions, @"NSFilePosixPermissions",
        NULL
    ];

    defaultManager = [NSFileManager defaultManager];

    filePath = [NSString stringWithFormat:@"%@/%@", path, [IFURLFileDatabase uniqueFilePathForKey:key]];

    result = [defaultManager createFileAtPath:filePath contents:data attributes:attributes];
    if (!result) {
        result = [defaultManager createFileAtPathWithIntermediateDirectories:filePath contents:data attributes:attributes directoryAttributes:directoryAttributes];
    }

    [archiver release];
}

-(void)performRemoveObjectForKey:(id)key
{
    NSString *filePath;


#ifdef WEBFOUNDATION_DEBUG
    WebFoundationLogAtLevel(WebFoundationLogDiskCacheActivity, @"- [WEBFOUNDATION_DEBUG] - performRemoveObjectForKey - %@", key);
#endif

    filePath = [NSString stringWithFormat:@"%@/%@", path, [IFURLFileDatabase uniqueFilePathForKey:key]];

    [[NSFileManager defaultManager] removeFileAtPath:filePath handler:nil];
}

// database management functions ---------------------------------------------------------------------------
#pragma mark database management functions

-(BOOL)open
{
    NSFileManager *manager;
    NSDictionary *attributes;
    BOOL isDir;
    
    if (!isOpen) {
        manager = [NSFileManager defaultManager];
        if ([manager fileExistsAtPath:path isDirectory:&isDir]) {
            if (isDir) {
                isOpen = YES;
            }
        }
        else {
            attributes = [NSDictionary dictionaryWithObjectsAndKeys:
                [NSDate date], @"NSFileModificationDate",
                NSUserName(), @"NSFileOwnerAccountName",
                IFURLFileDirectoryPosixPermissions, @"NSFilePosixPermissions",
                NULL
            ];
            
            // be optimistic that full subpath leading to directory exists
            isOpen = [manager createDirectoryAtPath:path attributes:attributes];
            if (!isOpen) {
                // perhaps the optimism did not pay off ...
                // try again, this time creating full subpath leading to directory
                isOpen = [manager createDirectoryAtPathWithIntermediateDirectories:path attributes:attributes];
            }
        }
    }
    
    return isOpen;
}

-(BOOL)close
{
    if (isOpen) {
        isOpen = NO;
    }
    
    return YES;
}

-(void)lazySync:(NSTimer *)theTimer
{
    IFURLFileDatabaseOp *op;

    while (touch + SYNC_IDLE_THRESHOLD < CFAbsoluteTimeGetCurrent() && [ops count] > 0) {
        [mutex lock];

        if (timer) {
            [timer invalidate];
            [timer autorelease];
            timer = nil;
        }
        
        op = [ops lastObject];
        if (op) {
            [ops removeLastObject];
            [op perform:self];
            [setCache removeObjectForKey:[op key]];
            [removeCache removeObject:[op key]];
            [op release];
        }

        [mutex unlock];
    }

    // come back later to finish the work...
    if ([ops count] > 0) {
        [mutex lock];
        [self setTimer];
        [mutex unlock];
    }
}

-(void)sync
{
    NSArray *array;
    int opCount;
    int i;
    IFURLFileDatabaseOp *op;

    touch = CFAbsoluteTimeGetCurrent();
    
    array = nil;

    [mutex lock];
    if ([ops count] > 0) {
        array = [NSArray arrayWithArray:ops];
        [ops removeAllObjects];
    }
    if (timer) {
        [timer invalidate];
        [timer autorelease];
        timer = nil;
    }
    [setCache removeAllObjects];
    [removeCache removeAllObjects];
    [mutex unlock];

    opCount = [array count];
    for (i = 0; i < opCount; i++) {
        op = [array objectAtIndex:i];
        [op perform:self];
    }
}

@end
