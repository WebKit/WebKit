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

#import <JavaScriptCore/Assertions.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebLRUFileList.h>

#import <stdlib.h>
#import <sys/types.h>
#import <sys/stat.h>
#import <fts.h>

#define RemoveFromHeap (TRUE)
#define DontRemoveFromHeap (FALSE)

struct WebLRUFileList
{
    char *rootDirectory;
    CFBinaryHeapRef heap;
    CFMutableDictionaryRef dict;
    unsigned int count;
    unsigned int totalSize;
};

typedef struct NSLRUFileData NSLRUFileData;

struct NSLRUFileData
{
    unsigned long fileSize;
    CFTimeInterval heapTime;
    CFTimeInterval updatedTime;
    unsigned int markRemoved;
    char path[1]; // variable size, big enough to hold path string and trailing nul
};

static CFComparisonResult compareTimes(const void *val1, const void *val2, void *context);
static Boolean cStringEqual(const void *val1, const void *val2);
static CFHashCode cStringHash(const void *val);
static Boolean NSLRUFileDataEqual(const void *val1, const void *val2);

static NSLRUFileData *WebLRUFileListGetOldestFileData(WebLRUFileList *list, Boolean removeFromHeap);

static void NSLRUFileDataReleaseApplierFunction(const void *key, const void *value, void *context);
static void NSLRUFileDataRelease(CFAllocatorRef allocator, const void *value);


WebLRUFileList *WebLRUFileListCreate(void)
{
    CFBinaryHeapCallBacks heapCallbacks = {0, NULL, NULL, NULL, compareTimes};
    CFBinaryHeapCompareContext heapCompareContext = {0, NULL, NULL, NULL, NULL}; 
    CFDictionaryKeyCallBacks dictKeyCallbacks = {0, NULL, NULL, NULL, cStringEqual, cStringHash};    
    CFDictionaryValueCallBacks dictValCallbacks = {0, NULL, NULL, NULL, NSLRUFileDataEqual};
    
    WebLRUFileList *list = malloc(sizeof(WebLRUFileList));
    
    list->rootDirectory = NULL;
    list->heap = CFBinaryHeapCreate(NULL, 0, &heapCallbacks, &heapCompareContext);
    list->dict = CFDictionaryCreateMutable(NULL, 0, &dictKeyCallbacks, &dictValCallbacks);    
    list->count = 0;
    list->totalSize = 0;
    
    return list;
}

void WebLRUFileListRelease(WebLRUFileList *list)
{
    ASSERT(list);
    free(list->rootDirectory);
    WebLRUFileListRemoveAllFilesFromList(list);
    CFRelease(list->heap);
    CFRelease(list->dict);
    free(list);
}

int WebLRUFileListRebuildFileDataUsingRootDirectory(WebLRUFileList *list, const char *path)
{
    ASSERT(list);
    ASSERT(path);

    FTS *fts;
    FTSENT *ent;
    char *paths[2];
    struct stat statInfo;
    
    if (stat(path, &statInfo) == -1) {
        return errno;
    }
    if ((statInfo.st_mode & S_IFDIR) == 0) {
        return ENOTDIR;
    }

    WebLRUFileListRemoveAllFilesFromList(list);
    free(list->rootDirectory);
    list->rootDirectory = strdup(path);
    int pathLength = strlen(path);
    
    paths[0] = (char *)path;
    paths[1] = NULL;
    
    fts = fts_open(paths, FTS_COMFOLLOW | FTS_LOGICAL, NULL);
    
    while ((ent = fts_read(fts))) {
        if (ent->fts_statp->st_mode & S_IFREG) {
            const char *filePath = ent->fts_accpath + pathLength + 1;
            WebLRUFileListSetFileData(list, filePath, ent->fts_statp->st_size, ent->fts_statp->st_ctimespec.tv_sec - kCFAbsoluteTimeIntervalSince1970);
        }
    }

    fts_close(fts);
    
    return 0;
}

unsigned int WebLRUFileListRemoveFileWithPath(WebLRUFileList *list, const char *path)
{
    ASSERT(list);
    ASSERT(path);

    unsigned int removedFileSize = 0;

    NSLRUFileData *data = (NSLRUFileData *)CFDictionaryGetValue(list->dict, path);
    if (data) {
        CFDictionaryRemoveValue(list->dict, path);
        data->markRemoved = 1;
        removedFileSize = data->fileSize;
        list->totalSize -= data->fileSize;
        ASSERT(list->count > 0);
        list->count--;
    }
    
    return removedFileSize;
}

void WebLRUFileListTouchFileWithPath(WebLRUFileList *list, const char *path)
{
    ASSERT(list);
    ASSERT(path);

    NSLRUFileData *data = (NSLRUFileData *)CFDictionaryGetValue(list->dict, path);
    if (data) {
        // This is not the same as the "real" mod time of the file on disk
        // but it is close enough for our purposes
        data->updatedTime = CFAbsoluteTimeGetCurrent();
    }
}

void WebLRUFileListSetFileData(WebLRUFileList *list, const char *path, unsigned long fileSize, CFTimeInterval time)
{
    ASSERT(list);
    ASSERT(path);

    NSLRUFileData *data = (NSLRUFileData *)CFDictionaryGetValue(list->dict, path);
    if (data) {
        // update existing data
        list->totalSize -= data->fileSize;
        list->totalSize += fileSize;
        data->fileSize = fileSize;
        data->updatedTime = time;
    }
    else {
        // create new data object
        // malloc a block that can accommodate the string
        // account for the size of the string when
        // allocating the correct size block.
        size_t mallocSize = offsetof(NSLRUFileData, path) + strlen(path) + 1;
        data = malloc(mallocSize);
        data->fileSize = fileSize;
        data->heapTime = data->updatedTime = time;
        data->markRemoved = 0;
        strcpy(data->path, path);
        list->totalSize += fileSize;
        list->count++;
        CFBinaryHeapAddValue(list->heap, data);
        CFDictionarySetValue(list->dict, data->path, data);
    }
}

Boolean WebLRUFileListGetPathOfOldestFile(WebLRUFileList *list, char *buffer, unsigned int length)
{
    ASSERT(list);

    NSLRUFileData *data = WebLRUFileListGetOldestFileData(list, DontRemoveFromHeap);

    if (data) {
        unsigned pathLength = strlen(data->path);
        if (length - 1 >= pathLength) {
            strcpy(buffer, data->path);
            return TRUE;
        }
    }
    
    return FALSE;
}

unsigned int WebLRUFileListRemoveOldestFileFromList(WebLRUFileList *list)
{
    ASSERT(list);

    if (list->count == 0) {
        return 0;
    }

    NSLRUFileData *data = WebLRUFileListGetOldestFileData(list, RemoveFromHeap);

    if (!data) {
        LOG_ERROR("list->count > 0, but no data returned from WebLRUFileListGetOldestFileData");
    }
    
    // no need to remove from heap explicitly
    // WebLRUFileListGetOldestFileData with RemoveFromHeap call does that
    CFDictionaryRemoveValue(list->dict, data->path);
    ASSERT(list->count > 0);
    list->count--;
    unsigned int removedFileSize = data->fileSize;
    list->totalSize -= removedFileSize;
    LOG(FileDatabaseActivity, "\n\t%s : %ld : %.0f : %.0f\n", data->path, data->fileSize, data->heapTime, data->updatedTime);
    NSLRUFileDataRelease(NULL, data);
    return removedFileSize;
}

Boolean WebLRUFileListContainsItem(WebLRUFileList *list, const char *path)
{
    ASSERT(list);
    ASSERT(path);

    if (list->count == 0) {
        return FALSE;
    }
    
    NSLRUFileData *data = (NSLRUFileData *)CFDictionaryGetValue(list->dict, path);
    if (!data) {
        return FALSE;
    }

    return TRUE;
}

unsigned int WebLRUFileListGetFileSize(WebLRUFileList *list, const char *path)
{
    ASSERT(list);
    ASSERT(path);

    unsigned int result = 0;

    if (list->count == 0) {
        return result;
    }
    
    NSLRUFileData *data = (NSLRUFileData *)CFDictionaryGetValue(list->dict, path);
    if (!data) {
        LOG_ERROR("list->count > 0, but no data returned from CFDictionaryGetValue with path: %s", path);
    }

    result = data->fileSize;
    
    return result;
}

unsigned int WebLRUFileListCountItems(WebLRUFileList *list)
{
    ASSERT(list);
    return list->count;
}

unsigned int WebLRUFileListGetTotalSize(WebLRUFileList *list)
{
    ASSERT(list);
    return list->totalSize;
}

void WebLRUFileListRemoveAllFilesFromList(WebLRUFileList *list)
{
    ASSERT(list);

    // use dictionary applier function to free all NSLRUFileData objects
    CFDictionaryApplyFunction(list->dict, NSLRUFileDataReleaseApplierFunction, NULL);
    
    CFDictionaryRemoveAllValues(list->dict);
    CFBinaryHeapRemoveAllValues(list->heap);
    list->count = 0;
    list->totalSize = 0;
}

static CFComparisonResult compareTimes(const void *val1, const void *val2, void *context)
{
    ASSERT(val1);
    ASSERT(val2);

    CFTimeInterval t1 = ((NSLRUFileData *)val1)->heapTime;
    CFTimeInterval t2 = ((NSLRUFileData *)val2)->heapTime;
    
    if (t1 > t2) return kCFCompareGreaterThan;
    if (t1 < t2) return kCFCompareLessThan;
    return kCFCompareEqualTo;
}

static Boolean cStringEqual(const void *val1, const void *val2)
{
    ASSERT(val1);
    ASSERT(val2);

    const char *s1 = (const char *)val1;
    const char *s2 = (const char *)val2;
    return strcmp(s1, s2) == 0;
}

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
// or anything like that.
static const unsigned PHI = 0x9e3779b9U;

// This hash algorithm comes from:
// http://burtleburtle.net/bob/hash/hashfaq.html
// http://burtleburtle.net/bob/hash/doobs.html
static CFHashCode cStringHash(const void *val)
{
    ASSERT(val);

    const char *s = (const char *)val;
    int length = strlen(s);
    int prefixLength = length < 8 ? length : 8;
    int suffixPosition = length < 16 ? 8 : length - 8;
    int i;

    unsigned h = PHI;
    h += length;
    h += (h << 10); 
    h ^= (h << 6); 

    for (i = 0; i < prefixLength; i++) {
        h += (unsigned char)s[i];
        h += (h << 10); 
        h ^= (h << 6); 
    }
    for (i = suffixPosition; i < length; i++) {
        h += (unsigned char)s[i];
        h += (h << 10); 
        h ^= (h << 6); 
    }

    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);
 
    return h;
}

static Boolean NSLRUFileDataEqual(const void *val1, const void *val2)
{
    ASSERT(val1);
    ASSERT(val2);

    NSLRUFileData *d1 = (NSLRUFileData *)val1;
    NSLRUFileData *d2 = (NSLRUFileData *)val2;
    return (d1->fileSize == d2->fileSize &&
        d1->heapTime == d2->heapTime && 
        d1->updatedTime == d2->updatedTime && 
        d1->markRemoved == d2->markRemoved &&
        strcmp(d1->path, d2->path) == 0);
}

static NSLRUFileData *WebLRUFileListGetOldestFileData(WebLRUFileList *list, Boolean removeFromHeap)
{
    ASSERT(list);

    // Use the heap count directory to account for the need to do lazy removal of heap objects 
    // that may have been left over by calls to WebLRUFileListRemoveFileWithPath
    // since whenever WebLRUFileListRemoveFileWithPath is called, 
    // list->count and the heap's count may no longer be equal
    unsigned int heapCount = CFBinaryHeapGetCount(list->heap);
    if (heapCount == 0) {
        return NULL;
    }

    unsigned i;
    NSLRUFileData *data;

    for (i = 0; i < heapCount; i++) {
        data = (NSLRUFileData *)CFBinaryHeapGetMinimum(list->heap);
        if (data->markRemoved) {
            // clean up this object that was marked for removal earlier
            CFBinaryHeapRemoveMinimumValue(list->heap);
            NSLRUFileDataRelease(NULL, data);        
        }
        else if (data->heapTime != data->updatedTime) {
            // update this object
            // reinsert into heap
            CFBinaryHeapRemoveMinimumValue(list->heap);
            data->heapTime = data->updatedTime;
            CFBinaryHeapAddValue(list->heap, data);    
        }
        else {
            // found an object that was neither updated nor marked for removal
            // return it
            if (removeFromHeap) {
                CFBinaryHeapRemoveMinimumValue(list->heap);
            }
            return data;
        }
    }        
    
    // we "wrapped around"
    // all nodes were touched after they were added, and we are back at the start
    // just pop off the object that is on top now.
    data = (NSLRUFileData *)CFBinaryHeapGetMinimum(list->heap);
    if (data && removeFromHeap) {
        ASSERT(data->heapTime == data->updatedTime);
        CFBinaryHeapRemoveMinimumValue(list->heap);
    }
    return data;
}

static void NSLRUFileDataReleaseApplierFunction(const void *key, const void *value, void *context)
{
    ASSERT(value);
    free((NSLRUFileData *)value);
}

static void NSLRUFileDataRelease(CFAllocatorRef allocator, const void *value)
{
    ASSERT(value);
    free((NSLRUFileData *)value);
}

// -----------------------------------------------------
// debugging

static void NSLRUFileDataBinaryHeapDumpApplierFunction(const void *value, void *context)
{
    NSMutableString *string = (NSMutableString *)context;
    NSLRUFileData *data = (NSLRUFileData *)value;
    [string appendFormat:@"   %s : %6ld : %.0f : %.0f\n", data->path, data->fileSize, data->heapTime, data->updatedTime];
}

static void NSLRUFileDataDictDumpApplierFunction(const void *key, const void *value, void *context)
{
    NSMutableString *string = (NSMutableString *)context;
    NSLRUFileData *data = (NSLRUFileData *)value;
    [string appendFormat:@"   %s -> %6ld : %.0f : %.0f\n", (const char *)key, data->fileSize, data->heapTime, data->updatedTime];
}

NSString *WebLRUFileListDescription(WebLRUFileList *list)
{
    NSMutableString *string = [NSMutableString string];

    [string appendFormat:@"\nList root path: %s\n", list->rootDirectory];
    [string appendFormat:@"List count: %d items\n", list->count];
    [string appendFormat:@"List total size: %d bytes\n", list->totalSize];

    [string appendFormat:@"\nHeap data: %ld items\n", CFBinaryHeapGetCount(list->heap)];
    CFBinaryHeapApplyFunction(list->heap, NSLRUFileDataBinaryHeapDumpApplierFunction, string);

    [string appendFormat:@"\nDict data: %ld items\n", CFDictionaryGetCount(list->dict)];
    CFDictionaryApplyFunction(list->dict, NSLRUFileDataDictDumpApplierFunction, string);
    
    return string;
}
