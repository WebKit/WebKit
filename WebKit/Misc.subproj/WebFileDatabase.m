/*	WebFileDatabase.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <string.h>
#import <fcntl.h>
#import <sys/stat.h>
#import <sys/types.h>
#import <sys/mman.h>
#import <pthread.h>
#import <fts.h>

#import "WebFileDatabase.h"
#import "WebNSFileManagerExtras.h"
#import "WebFoundationLogging.h"
#import "WebSystemBits.h"

#define SIZE_FILE_NAME @".size"
#define SIZE_FILE_NAME_CSTRING ".size"

#if ERROR_DISABLED
#define BEGIN_EXCEPTION_HANDLER
#define END_EXCEPTION_HANDLER
#else
#define BEGIN_EXCEPTION_HANDLER NS_DURING
#define END_EXCEPTION_HANDLER \
    NS_HANDLER \
        ERROR("Uncaught exception: %@ [%@] [%@]", [localException class], [localException reason], [localException userInfo]); \
    NS_ENDHANDLER
#endif

static pthread_once_t databaseInitControl = PTHREAD_ONCE_INIT;
static NSNumber *WebFileDirectoryPOSIXPermissions;
static NSNumber *WebFilePOSIXPermissions;
static NSRunLoop *syncRunLoop;

typedef enum
{
    WebFileDatabaseSetObjectOp,
    WebFileDatabaseRemoveObjectOp,
} WebFileDatabaseOpcode;

enum
{
    MAX_UNSIGNED_LENGTH = 20, // long enough to hold the string representation of a 64-bit unsigned number
    SYNC_IDLE_THRESHOLD = 3,
};

// support for expiring cache files using file system access times --------------------------------------------

typedef struct
{   
    NSString *path;
    time_t time;
} FileAccessTime;

static CFComparisonResult compare_atimes(const void *val1, const void *val2, void *context)
{
    int t1 = ((FileAccessTime *)val1)->time;
    int t2 = ((FileAccessTime *)val2)->time;
    
    if (t1 > t2) return kCFCompareGreaterThan;
    if (t1 < t2) return kCFCompareLessThan;
    return kCFCompareEqualTo;
}

static Boolean PointerEqual(const void *p1, const void *p2)
{
    return p1 == p2;  
}

static void FileAccessTimeRelease(CFAllocatorRef allocator, const void *value)
{
    FileAccessTime *spec;
    
    spec = (FileAccessTime *)value;
    [spec->path release];
    free(spec);
}

static CFArrayRef CreateArrayListingFilesSortedByAccessTime(const char *path)
{
    FTS *fts;
    FTSENT *ent;
    CFMutableArrayRef atimeArray;
    char *paths[2];
    NSFileManager *defaultManager;
    
    CFArrayCallBacks callBacks = {0, NULL, FileAccessTimeRelease, NULL, PointerEqual};
    
    defaultManager = [NSFileManager defaultManager];
    paths[0] = (char *)path;
    paths[1] = NULL;
    
    atimeArray = CFArrayCreateMutable(NULL, 0, &callBacks);
    fts = fts_open(paths, FTS_COMFOLLOW | FTS_LOGICAL, NULL);
    
    ent = fts_read(fts);
    while (ent) {
        if (ent->fts_statp->st_mode & S_IFREG && strcmp(ent->fts_name, SIZE_FILE_NAME_CSTRING) != 0) {
            FileAccessTime *spec = malloc(sizeof(FileAccessTime));
            spec->path = [[defaultManager stringWithFileSystemRepresentation:ent->fts_accpath length:strlen(ent->fts_accpath)] retain];
            spec->time = ent->fts_statp->st_atimespec.tv_sec;
            CFArrayAppendValue(atimeArray, spec);
        }
        ent = fts_read(fts);
    }

    CFArraySortValues(atimeArray, CFRangeMake(0, CFArrayGetCount(atimeArray)), compare_atimes, NULL);

    fts_close(fts);
    
    return atimeArray;
}

// interface WebFileReader -------------------------------------------------------------

@interface WebFileReader : NSObject
{
    NSData *data;
    caddr_t mappedBytes;
    size_t mappedLength;
}

- (id)initWithPath:(NSString *)path;
- (NSData *)data;

@end

// implementation WebFileReader -------------------------------------------------------------

static NSMutableSet *notMappableFileNameSet = nil;
static NSLock *mutex;

static void URLFileReaderInit(void)
{
    mutex = [[NSLock alloc] init];
    notMappableFileNameSet = [[NSMutableSet alloc] init];    
}

@implementation WebFileReader

- (id)initWithPath:(NSString *)path
{
    int fd;
    struct stat statInfo;
    const char *fileSystemPath;
    BOOL fileNotMappable;
    static pthread_once_t cacheFileReaderControl = PTHREAD_ONCE_INIT;

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
            LOG(DiskCacheActivity, "mmaped disk cache file - %@", path);
        }
        else {
            LOG(DiskCacheActivity, "fs read disk cache file - %@", path);
        }
        return self;
    }
    else {
        LOG(DiskCacheActivity, "no disk cache file - %@", path);
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

// interface WebFileDatabaseOp -------------------------------------------------------------

@interface WebFileDatabaseOp : NSObject
{
    WebFileDatabaseOpcode opcode;
    id key;
    id object; 
}

+(id)opWithCode:(WebFileDatabaseOpcode)opcode key:(id)key object:(id)object;
-(id)initWithCode:(WebFileDatabaseOpcode)opcode key:(id)key object:(id)object;

-(WebFileDatabaseOpcode)opcode;
-(id)key;
-(id)object;
-(void)perform:(WebFileDatabase *)target;

@end


// implementation WebFileDatabaseOp -------------------------------------------------------------

@implementation WebFileDatabaseOp

+(id)opWithCode:(WebFileDatabaseOpcode)theOpcode key:(id)theKey object:(id)theObject
{
    return [[[WebFileDatabaseOp alloc] initWithCode:theOpcode key:theKey object:theObject] autorelease];
}

-(id)initWithCode:(WebFileDatabaseOpcode)theOpcode key:(id)theKey object:(id)theObject
{
    ASSERT(theKey);

    if ((self = [super init])) {
        
        opcode = theOpcode;
        key = [theKey retain];
        object = [theObject retain];
        
        return self;
    }
  
    return nil;
}

-(WebFileDatabaseOpcode)opcode
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

-(void)perform:(WebFileDatabase *)target
{
    ASSERT(target);

    switch (opcode) {
        case WebFileDatabaseSetObjectOp:
            [target performSetObject:object forKey:key];
            break;
        case WebFileDatabaseRemoveObjectOp:
            [target performRemoveObjectForKey:key];
            break;
        default:
            ASSERT_NOT_REACHED();
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


// interface WebFileDatabasePrivate -----------------------------------------------------------

@interface WebFileDatabase (WebFileDatabasePrivate)

+(NSString *)uniqueFilePathForKey:(id)key;
-(void)writeSizeFile:(unsigned)value;
-(unsigned)readSizeFile;
-(void)_truncateToSizeLimit:(unsigned)size;

@end

// implementation WebFileDatabasePrivate ------------------------------------------------------

@implementation WebFileDatabase (WebFileDatabasePrivate)

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
    
    fd = open(sizeFilePath, O_WRONLY | O_CREAT, [WebFilePOSIXPermissions intValue]);
    if (fd > 0) {
        buf = calloc(1, MAX_UNSIGNED_LENGTH);
        sprintf(buf, "%d", value);
        write(fd, buf, MAX_UNSIGNED_LENGTH);
        free(buf);
        close(fd);
    }

    LOG(DiskCacheActivity, "writing size file - %u", value);
    
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

-(void)_truncateToSizeLimit:(unsigned)size
{
    NSFileManager *defaultManager;
    NSDictionary *attributes;
    NSNumber *fileSize;
    CFArrayRef atimeArray;
    unsigned aTimeArrayCount;
    unsigned i;
    FileAccessTime *spec;
    
    if (size > usage) {
        return;
    }

    if (size == 0) {
        [self removeAllObjects];
    }
    else {
        defaultManager = [NSFileManager defaultManager];
        [mutex lock];
        atimeArray = CreateArrayListingFilesSortedByAccessTime([defaultManager fileSystemRepresentationWithPath:path]);
        aTimeArrayCount = CFArrayGetCount(atimeArray);
        for (i = 0; i < aTimeArrayCount && usage > size; i++) {
            spec = (FileAccessTime *)CFArrayGetValueAtIndex(atimeArray, i);
            attributes = [defaultManager fileAttributesAtPath:spec->path traverseLink:YES];
            if (attributes) {
                fileSize = [attributes objectForKey:NSFileSize];
                if (fileSize) {
                    usage -= [fileSize unsignedIntValue];
                    LOG(DiskCacheActivity, "_truncateToSizeLimit - %u - %u - %u, %@", size, usage, [fileSize unsignedIntValue], spec->path);
                    [defaultManager removeFileAtPath:spec->path handler:nil];
                }
            }
        }
        CFRelease(atimeArray);
        [self writeSizeFile:usage];
        [mutex unlock];
    }
}

@end


// implementation WebFileDatabase -------------------------------------------------------------

@implementation WebFileDatabase

// creation functions ---------------------------------------------------------------------------
#pragma mark creation functions

+(void)_syncLoop:(id)arg
{
    WebSetThreadPriority(WebMinThreadPriority);

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSPort *placeholderPort;

    BEGIN_EXCEPTION_HANDLER

    syncRunLoop = [NSRunLoop currentRunLoop];

    while (YES) {
        BEGIN_EXCEPTION_HANDLER
        // we specifically use an NSRunLoop here to get autorelease pool support
        placeholderPort = [NSPort port];
        [syncRunLoop addPort:placeholderPort forMode:NSDefaultRunLoopMode];
        [syncRunLoop run];
        [syncRunLoop removePort:placeholderPort forMode:NSDefaultRunLoopMode];
        END_EXCEPTION_HANDLER
    }

    END_EXCEPTION_HANDLER

    [pool release];
}

static void databaseInit()
{
    // set file perms to owner read/write/execute only
    WebFileDirectoryPOSIXPermissions = [[NSNumber numberWithInt:(WEB_UREAD | WEB_UWRITE | WEB_UEXEC)] retain];

    // set file perms to owner read/write only
    WebFilePOSIXPermissions = [[NSNumber numberWithInt:(WEB_UREAD | WEB_UWRITE)] retain];

    [NSThread detachNewThreadSelector:@selector(_syncLoop:) toTarget:[WebFileDatabase class] withObject:nil];
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
    [mutex lock];
    // this locking gives time for writes that are happening now to finish
    [mutex unlock];
    [self close];
    [timer invalidate];
    [timer release];
    [ops release];
    [setCache release];
    [removeCache release];
    [mutex release];

    [super dealloc];
}

-(void)setTimer
{
    if (timer == nil) {
        timer = [[NSTimer timerWithTimeInterval:SYNC_IDLE_THRESHOLD target:self selector:@selector(lazySync:) userInfo:nil repeats:YES] retain];
        [syncRunLoop addTimer:timer forMode:NSDefaultRunLoopMode];
    }
}

// database functions ---------------------------------------------------------------------------
#pragma mark database functions

-(void)setObject:(id)object forKey:(id)key
{
    WebFileDatabaseOp *op;

    ASSERT(object);
    ASSERT(key);

    touch = CFAbsoluteTimeGetCurrent();

    LOG(DiskCacheActivity, "setObject - %p - %@", object, key);
    
    [mutex lock];
    
    [setCache setObject:object forKey:key];
    op = [[WebFileDatabaseOp alloc] initWithCode:WebFileDatabaseSetObjectOp key:key object:object];
    [ops addObject:op];
    [self setTimer];
    
    [mutex unlock];
}

-(void)removeObjectForKey:(id)key
{
    WebFileDatabaseOp *op;

    ASSERT(key);

    touch = CFAbsoluteTimeGetCurrent();
    
    [mutex lock];
    
    [removeCache addObject:key];
    op = [[WebFileDatabaseOp alloc] initWithCode:WebFileDatabaseRemoveObjectOp key:key object:nil];
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
    [ops removeAllObjects];
    [self close];
    [[NSFileManager defaultManager] _web_backgroundRemoveFileAtPath:path];
    [self open];
    [self writeSizeFile:0];
    usage = 0;
    [mutex unlock];

    LOG(DiskCacheActivity, "removeAllObjects");
}

-(id)objectForKey:(id)key
{
    volatile id result;
    
    ASSERT(key);

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
    NSString *filePath = [[NSString alloc] initWithFormat:@"%@/%@", path, [WebFileDatabase uniqueFilePathForKey:key]];
    WebFileReader *fileReader = [[WebFileReader alloc] initWithPath:filePath];
    
    NSData *data;
    NSUnarchiver * volatile unarchiver = nil;

    NS_DURING
        if (fileReader && (data = [fileReader data])) {
            unarchiver = [[NSUnarchiver alloc] initForReadingWithData:data];
            if (unarchiver) {
                id fileKey = [unarchiver decodeObject];
                if ([fileKey isEqual:key]) {
                    id object = [unarchiver decodeObject];
                    if (object) {
                        // Decoded objects go away when the unarchiver does, so we need to
                        // retain this so we can return it to our caller.
                        result = [[object retain] autorelease];
                    }
                }
            }
        }
    NS_HANDLER
        LOG(DiskCacheActivity, "cannot unarchive cache file - %@", key);
        result = nil;
    NS_ENDHANDLER

    [unarchiver release];
    [fileReader release];
    [filePath release];

    return result;
}

-(NSEnumerator *)keys
{
    // FIXME: [kocienda] Radar 2859370 (WebFileDatabase needs to implement keys method)
    return nil;
}


-(void)performSetObject:(id)object forKey:(id)key
{
    NSString *filePath;
    NSMutableData *data;
    NSDictionary *attributes;
    NSDictionary *directoryAttributes;
    NSArchiver *archiver;
    NSFileManager *defaultManager;
    NSNumber *oldSize;
    BOOL result;

    ASSERT(object);
    ASSERT(key);

    LOG(DiskCacheActivity, "performSetObject - %@ - %@", key, [WebFileDatabase uniqueFilePathForKey:key]);

    data = [NSMutableData data];
    archiver = [[NSArchiver alloc] initForWritingWithMutableData:data];
    [archiver encodeObject:key];
    [archiver encodeObject:object];
    
    attributes = [NSDictionary dictionaryWithObjectsAndKeys:
        NSUserName(), NSFileOwnerAccountName,
        WebFilePOSIXPermissions, NSFilePosixPermissions,
        NULL
    ];

    directoryAttributes = [NSDictionary dictionaryWithObjectsAndKeys:
        NSUserName(), NSFileOwnerAccountName,
        WebFileDirectoryPOSIXPermissions, NSFilePosixPermissions,
        NULL
    ];

    defaultManager = [NSFileManager defaultManager];

    filePath = [[NSString alloc] initWithFormat:@"%@/%@", path, [WebFileDatabase uniqueFilePathForKey:key]];
    attributes = [defaultManager fileAttributesAtPath:filePath traverseLink:YES];

    // update usage and truncate before writing file
    // this has the effect of _always_ keeping disk usage under sizeLimit by clearing away space in anticipation of the write.
    usage += [data length];
    [self _truncateToSizeLimit:[self sizeLimit]];

    result = [defaultManager _web_createFileAtPathWithIntermediateDirectories:filePath contents:data attributes:attributes directoryAttributes:directoryAttributes];

    if (result) {
        // we're going to write a new file
        // if there was an old file, we have to subtract the size of the old file before adding the new size
        if (attributes) {
            oldSize = [attributes objectForKey:NSFileSize];
            if (oldSize) {
                usage -= [oldSize unsignedIntValue];
            }
        }
        [self writeSizeFile:usage];
    }
    else {
        // we failed to write the file. don't charge this against usage.
        usage -= [data length];
    }

    [archiver release];
    [filePath release];    
}

-(void)performRemoveObjectForKey:(id)key
{
    NSString *filePath;
    NSDictionary *attributes;
    NSNumber *size;
    BOOL result;
    
    ASSERT(key);
    
    LOG(DiskCacheActivity, "performRemoveObjectForKey - %@", key);

    filePath = [[NSString alloc] initWithFormat:@"%@/%@", path, [WebFileDatabase uniqueFilePathForKey:key]];
    attributes = [[NSFileManager defaultManager] fileAttributesAtPath:filePath traverseLink:YES];
    result = [[NSFileManager defaultManager] removeFileAtPath:filePath handler:nil];
    if (result && attributes) {
        size = [attributes objectForKey:NSFileSize];
        if (size) {
            usage -= [size unsignedIntValue];
            [self writeSizeFile:usage];
        }
    }
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
                WebFileDirectoryPOSIXPermissions, @"NSFilePosixPermissions",
                NULL
            ];
            
	    isOpen = [manager _web_createDirectoryAtPathWithIntermediateDirectories:path attributes:attributes];
	}

        sizeFilePathString = [NSString stringWithFormat:@"%@/%@", path, SIZE_FILE_NAME];
        tmp = [[NSFileManager defaultManager] fileSystemRepresentationWithPath:sizeFilePathString];
        sizeFilePath = strdup(tmp);
        usage = [self readSizeFile];

        // remove any leftover turds
        [manager _web_deleteBackgroundRemoveLeftoverFiles:path];
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
    WebFileDatabaseOp *op;

    ASSERT(theTimer);

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

    touch = CFAbsoluteTimeGetCurrent();
    
    [mutex lock];
    array = [ops copy];
    [ops removeAllObjects];
    [timer invalidate];
    [timer autorelease];
    timer = nil;
    [setCache removeAllObjects];
    [removeCache removeAllObjects];
    [mutex unlock];

    [array makeObjectsPerformSelector:@selector(perform:) withObject:self];
    [array release];
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
        [self _truncateToSizeLimit:limit];
    }
}

@end
