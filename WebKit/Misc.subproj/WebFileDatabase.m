/*	IFURLFileDatabase.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <fcntl.h>
#import <sys/stat.h>
#import <sys/types.h>
#import <sys/mman.h>
#import <pthread.h>

#import "IFURLFileDatabase.h"
#import "IFNSFileManagerExtensions.h"
#import "IFURLCacheLoaderConstantsPrivate.h"
#import "WebFoundationDebug.h"

#define SIZE_FILE_NAME @".size"

// The next line is a workaround for Radar 2905545. Once that's fixed, it can use PTHREAD_ONCE_INIT.
static void databaseInit(void);
static pthread_once_t databaseInitControl = {_PTHREAD_ONCE_SIG_init, {}};
static NSNumber *IFURLFileDirectoryPosixPermissions;
static NSNumber *IFURLFilePosixPermissions;

typedef enum
{
    IFURLFileDatabaseSetObjectOp,
    IFURLFileDatabaseRemoveObjectOp,
} IFURLFileDatabaseOpCode;

enum
{
    MAX_UNSIGNED_LENGTH = 20, // long enough to hold the string representation of a 64-bit unsigned number
    SYNC_INTERVAL = 5,
    SYNC_IDLE_THRESHOLD = 5,
};

// interface IFURLFileReader -------------------------------------------------------------

@interface IFURLFileReader : NSObject
{
    NSData *data;
    caddr_t mappedBytes;
    size_t mappedLength;
}

- (id)initWithPath:(NSString *)path;
- (NSData *)data;

@end

// implementation IFURLFileReader -------------------------------------------------------------

static NSMutableSet *notMappableFileNameSet = nil;
static NSLock *mutex;

// The next line is a workaround for Radar 2905545. Once that's fixed, it can use PTHREAD_ONCE_INIT.
static pthread_once_t cacheFileReaderControl = {_PTHREAD_ONCE_SIG_init, {}};

static void URLFileReaderInit(void)
{
    mutex = [[NSLock alloc] init];
    notMappableFileNameSet = [[NSMutableSet alloc] init];    
}

@implementation IFURLFileReader

- (id)initWithPath:(NSString *)path
{
    int fd;
    struct stat statInfo;
    const char *fileSystemPath;
    BOOL fileNotMappable;

    pthread_once(&cacheFileReaderControl, URLFileReaderInit);

    [super init];

    if (self == nil || path == nil) {
        [self dealloc];
        return nil;
    }
    
    data = nil;
    mappedBytes = NULL;
    mappedLength = 0;

    NS_DURING
        fileSystemPath = [path fileSystemRepresentation];
    NS_HANDLER
        fileSystemPath = NULL;
    NS_ENDHANDLER

    [mutex lock];
    fileNotMappable = [notMappableFileNameSet containsObject:path];
    [mutex unlock];

    if (fileNotMappable) {
        data = [[NSData alloc] initWithContentsOfFile:path];
    }
    else if (fileSystemPath && (fd = open(fileSystemPath, O_RDONLY, 0)) >= 0) {
        // File exists. Retrieve the file size.
        if (fstat(fd, &statInfo) == 0) {
            // Map the file into a read-only memory region.
            mappedBytes = mmap(NULL, statInfo.st_size, PROT_READ, 0, fd, 0);
            if (mappedBytes == MAP_FAILED) {
                // map has failed but file exists
                // add file to set of paths known not to be mappable
                // then, read file from file system
                [mutex lock];
                [notMappableFileNameSet addObject:path];
                [mutex unlock];
                
                mappedBytes = NULL;
                data = [[NSData alloc] initWithContentsOfFile:path];
            }
            else {
                // On success, create data object using mapped bytes.
                mappedLength = statInfo.st_size;
                data = [[NSData alloc] initWithBytesNoCopy:mappedBytes length:mappedLength freeWhenDone:NO];
                // ok data creation failed but we know file exists
                // be stubborn....try to read bytes again
                if (!data) {
                    munmap(mappedBytes, mappedLength);    
                    data = [[NSData alloc] initWithContentsOfFile:path];
                }
            }
        }
        close(fd);
    }
    
    if (data) {
        if (mappedBytes) {
            WEBFOUNDATIONDEBUGLEVEL(WebFoundationLogDiskCacheActivity, "mmaped disk cache file - %s", [path lossyCString]);
        }
        else {
            WEBFOUNDATIONDEBUGLEVEL(WebFoundationLogDiskCacheActivity, "fs read disk cache file - %s", [path lossyCString]);
        }
        return self;
    }
    else {
        WEBFOUNDATIONDEBUGLEVEL(WebFoundationLogDiskCacheActivity, "no disk cache file - %s", [path lossyCString]);
        [self dealloc];
        return nil;
    }
}

- (NSData *)data
{
    return data;
}

- (void)dealloc
{
    if (mappedBytes) {
        munmap(mappedBytes, mappedLength); 
    }
    
    [data release];
    
    [super dealloc];
}

@end

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
    WEBFOUNDATION_ASSERT_NOT_NIL(theKey);

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
    WEBFOUNDATION_ASSERT_NOT_NIL(target);

    switch (opcode) {
        case IFURLFileDatabaseSetObjectOp:
            [target performSetObject:object forKey:key];
            break;
        case IFURLFileDatabaseRemoveObjectOp:
            [target performRemoveObjectForKey:key];
            break;
        default:
            WEBFOUNDATION_ASSERT_NOT_NIL(nil);
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


// interface IFURLFileDatabasePrivate -----------------------------------------------------------

@interface IFURLFileDatabase (IFURLFileDatabasePrivate)

+(NSString *)uniqueFilePathForKey:(id)key;
-(void)writeSizeFile:(unsigned)value;
-(unsigned)readSizeFile;
-(void)truncateToSizeLimit:(unsigned)size;

@end

// implementation IFURLFileDatabasePrivate ------------------------------------------------------

@implementation IFURLFileDatabase (IFURLFileDatabasePrivate)

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


-(void)writeSizeFile:(unsigned)value
{
    void *buf;
    int fd;
    
    [mutex lock];
    
    fd = open(sizeFilePath, O_WRONLY | O_CREAT, [IFURLFilePosixPermissions intValue]);
    if (fd > 0) {
        buf = calloc(1, MAX_UNSIGNED_LENGTH);
        sprintf(buf, "%d", value);
        write(fd, buf, MAX_UNSIGNED_LENGTH);
        free(buf);
        close(fd);
    }
    
    [mutex unlock];
}

-(unsigned)readSizeFile
{
    unsigned result;
    void *buf;
    int fd;
    
    result = 0;

    [mutex lock];
    
    fd = open(sizeFilePath, O_RDONLY, 0);
    if (fd > 0) {
        buf = calloc(1, MAX_UNSIGNED_LENGTH);
        read(fd, buf, MAX_UNSIGNED_LENGTH);
        result = strtol(buf, NULL, 10);
        free(buf);
        close(fd);
    }
    
    [mutex unlock];

    return result;
}

-(void)truncateToSizeLimit:(unsigned)size
{
    NSFileManager *defaultManager;
    NSDirectoryEnumerator *enumerator;
    NSString *filePath;
    NSString *fullFilePath;
    NSDictionary *attributes;
    NSNumber *fileSize;
    
    if (size > usage) {
        return;
    }

    if (size == 0) {
        [self removeAllObjects];
    }
    else {
        defaultManager = [NSFileManager defaultManager];
        [mutex lock];
        enumerator = [defaultManager enumeratorAtPath:path];
        while (usage > size) {
            filePath = [enumerator nextObject];
            if (filePath == nil) {
                break;
            }
            if ([filePath isEqualToString:SIZE_FILE_NAME]) {
                continue;
            }
            
            fullFilePath = [[NSString alloc] initWithFormat:@"%@/%@", path, filePath];
            attributes = [defaultManager fileAttributesAtPath:fullFilePath traverseLink:YES];
            if (attributes) {
                if ([[attributes objectForKey:NSFileType] isEqualToString:NSFileTypeRegular]) {
                    fileSize = [attributes objectForKey:NSFileSize];
                    if (fileSize) {
                        usage -= [fileSize unsignedIntValue];
                        WEBFOUNDATIONDEBUGLEVEL(WebFoundationLogDiskCacheActivity, "truncateToSizeLimit - %u - %u - %u, %s", size, usage, [fileSize unsignedIntValue], DEBUG_OBJECT(fullFilePath));
                        [defaultManager removeFileAtPath:fullFilePath handler:nil];
                    }
                }
            }
            [fullFilePath release];
        }
        [self writeSizeFile:usage];
        [mutex unlock];
    }
}

@end


// implementation IFURLFileDatabase -------------------------------------------------------------

@implementation IFURLFileDatabase

// creation functions ---------------------------------------------------------------------------
#pragma mark creation functions

static void databaseInit()
{
    // set file perms to owner read/write/execute only
    IFURLFileDirectoryPosixPermissions = [[NSNumber numberWithInt:(IF_UREAD | IF_UWRITE | IF_UEXEC)] retain];

    // set file perms to owner read/write only
    IFURLFilePosixPermissions = [[NSNumber numberWithInt:(IF_UREAD | IF_UWRITE)] retain];
}

-(id)initWithPath:(NSString *)thePath
{
    pthread_once(&databaseInitControl, databaseInit);

    [super initWithPath:thePath];
    
    if (self == nil || thePath == nil) {
        [self dealloc];
        return nil;
    }

    ops = [[NSMutableArray alloc] init];
    setCache = [[NSMutableDictionary alloc] init];
    removeCache = [[NSMutableSet alloc] init];
    timer = nil;
    mutex = [[NSRecursiveLock alloc] init];
    sizeFilePath = NULL;
    
    return self;
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

    WEBFOUNDATION_ASSERT_NOT_NIL(object);
    WEBFOUNDATION_ASSERT_NOT_NIL(key);

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

    WEBFOUNDATION_ASSERT_NOT_NIL(key);

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
    [[NSFileManager defaultManager] _IF_backgroundRemoveFileAtPath:path];
    [self open];
    [self writeSizeFile:0];
    [mutex unlock];

    WEBFOUNDATIONDEBUGLEVEL(WebFoundationLogDiskCacheActivity, "removeAllObjects");
}

-(id)objectForKey:(id)key
{
    volatile id result;
    id fileKey;
    id object;
    NSString *filePath;
    IFURLFileReader * volatile fileReader;
    NSData *data;
    NSUnarchiver * volatile unarchiver;
        
    fileKey = nil;
    fileReader = nil;
    data = nil;
    unarchiver = nil;

    WEBFOUNDATION_ASSERT_NOT_NIL(key);

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
    filePath = [[NSString alloc] initWithFormat:@"%@/%@", path, [IFURLFileDatabase uniqueFilePathForKey:key]];
    fileReader = [[IFURLFileReader alloc] initWithPath:filePath];
    if (fileReader && (data = [fileReader data])) {
        unarchiver = [[NSUnarchiver alloc] initForReadingWithData:data];
    }
    
    NS_DURING
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
        WEBFOUNDATIONDEBUGLEVEL(WebFoundationLogDiskCacheActivity, "cannot unarchive cache file - %s", DEBUG_OBJECT(key));
        result = nil;
    NS_ENDHANDLER

    [unarchiver release];
    [fileReader release];
    [filePath release];

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

    WEBFOUNDATION_ASSERT_NOT_NIL(object);
    WEBFOUNDATION_ASSERT_NOT_NIL(key);

    WEBFOUNDATIONDEBUGLEVEL(WebFoundationLogDiskCacheActivity, "performSetObject - %s - %s",
        DEBUG_OBJECT(key), DEBUG_OBJECT([IFURLFileDatabase uniqueFilePathForKey:key]));

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

    filePath = [[NSString alloc] initWithFormat:@"%@/%@", path, [IFURLFileDatabase uniqueFilePathForKey:key]];

    result = [defaultManager createFileAtPath:filePath contents:data attributes:attributes];
    if (!result) {
        result = [defaultManager _IF_createFileAtPathWithIntermediateDirectories:filePath contents:data attributes:attributes directoryAttributes:directoryAttributes];
    }

    [archiver release];
    [filePath release];
    
    usage += [data length];
    [self writeSizeFile:usage];
    [self truncateToSizeLimit:[self sizeLimit]];
}

-(void)performRemoveObjectForKey:(id)key
{
    NSString *filePath;
    NSDictionary *attributes;
    NSNumber *size;
    
    WEBFOUNDATION_ASSERT_NOT_NIL(key);
    
    WEBFOUNDATIONDEBUGLEVEL(WebFoundationLogDiskCacheActivity, "performRemoveObjectForKey - %s", DEBUG_OBJECT(key));

    filePath = [[NSString alloc] initWithFormat:@"%@/%@", path, [IFURLFileDatabase uniqueFilePathForKey:key]];
    attributes = [[NSFileManager defaultManager] fileAttributesAtPath:filePath traverseLink:YES];
    if (attributes) {
        size = [attributes objectForKey:NSFileSize];
        if (size) {
            usage -= [size unsignedIntValue];
            [self writeSizeFile:usage];
        }
    }
    [[NSFileManager defaultManager] removeFileAtPath:filePath handler:nil];
    [filePath release];
}

// database management functions ---------------------------------------------------------------------------
#pragma mark database management functions

-(BOOL)open
{
    NSFileManager *manager;
    NSDictionary *attributes;
    BOOL isDir;
    const char *tmp;
    NSString *sizeFilePathString;
    
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
                isOpen = [manager _IF_createDirectoryAtPathWithIntermediateDirectories:path attributes:attributes];
            }
        }

        sizeFilePathString = [NSString stringWithFormat:@"%@/%@", path, SIZE_FILE_NAME];
        tmp = [[NSFileManager defaultManager] fileSystemRepresentationWithPath:sizeFilePathString];
        sizeFilePath = strdup(tmp);
        usage = [self readSizeFile];
    }
    
    return isOpen;
}

-(BOOL)close
{
    if (isOpen) {
        isOpen = NO;

        if (sizeFilePath) {
            free(sizeFilePath);
            sizeFilePath = NULL;
        }
    }
    
    return YES;
}

-(void)lazySync:(NSTimer *)theTimer
{
    IFURLFileDatabaseOp *op;

    WEBFOUNDATION_ASSERT_NOT_NIL(theTimer);

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

-(unsigned)count
{
    // FIXME: [kocienda] Radar 2922874 (On-disk cache does not know how many elements it is storing)
    return 0;
}

-(void)setSizeLimit:(unsigned)limit
{
    sizeLimit = limit;
    if (limit < usage) {
        [self truncateToSizeLimit:limit];
    }
}

@end
