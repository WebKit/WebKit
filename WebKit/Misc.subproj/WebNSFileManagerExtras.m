/*
    WebNSFileManagerExtras.m
    Private (SPI) header
    Copyright 2003, Apple, Inc. All rights reserved.
 */

#import <WebKit/WebNSFileManagerExtras.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebKitNSStringExtras.h>

#import <sys/mount.h>

@implementation NSFileManager (WebNSFileManagerExtras)

- (BOOL)_webkit_fileExistsAtPath:(NSString *)path isDirectory:(BOOL *)isDirectory traverseLink:(BOOL)flag
{
    BOOL result;
    NSDictionary *attributes;

    result = NO;
    if (isDirectory) {
        *isDirectory = NO;
    }

    attributes = [self fileAttributesAtPath:path traverseLink:flag];

    if (attributes) {
        result = YES;
        if ([[attributes objectForKey:NSFileType] isEqualToString:NSFileTypeDirectory]) {
            if (isDirectory) {
                *isDirectory = YES;
            }
        }
    }

    return result;
}

- (BOOL)_webkit_createIntermediateDirectoriesForPath:(NSString *)path attributes:(NSDictionary *)attributes
{
    BOOL result;
    NSArray *pathComponents;
    BOOL isDir;
    unsigned count;
    unsigned i;
    NSString *checkPath;
    NSMutableString *subpath;
            
    if (!path || [path length] == 0 || ![path isAbsolutePath]) {
        return NO;
    }

    result = NO;  

    // check to see if the path to the file already exists        
    if ([self _webkit_fileExistsAtPath:[path stringByDeletingLastPathComponent] isDirectory:&isDir traverseLink:YES]) {
        if (isDir) {
            result = YES;
        }
        else {
            result = NO;
        }
    }
    else {
        // create the path to the file
        result = YES;  

        // assume that most of the path exists, look backwards until we find an existing subpath
        checkPath = path;
        while (![checkPath isEqualToString:@"/"]) {
            checkPath = [checkPath stringByDeletingLastPathComponent];
            if ([self _webkit_fileExistsAtPath:checkPath isDirectory:&isDir traverseLink:YES]) {
                if (isDir) {
                    break;
                }
                else {
                    // found a leaf node, can't continue
                    result = NO;
                    break;
                }
            }
        }

        if (result) {
            // now build up the path to the point where we found existing paths
            subpath = [[NSMutableString alloc] initWithCapacity:[path length]];
            pathComponents = [path componentsSeparatedByString:@"/"];    
            count = [pathComponents count];
            i = 0;
            while (i < count - 1 && ![subpath isEqualToString:checkPath]) {
                if (i > 0) {
                    [subpath appendString:@"/"];
                }
                [subpath appendString:[pathComponents objectAtIndex:i]];  
                i++;  
            }
            
            // now create the parts of the path that did not yet exist
            while (i < count - 1) {
                if ([(NSString *)[pathComponents objectAtIndex:i] length] == 0) {
                    continue;
                }
                if (i > 0) {
                    [subpath appendString:@"/"];
                }
                [subpath appendString:[pathComponents objectAtIndex:i]];
                
                // does this directory exist?
                if ([self _webkit_fileExistsAtPath:subpath isDirectory:&isDir traverseLink:YES]) {
                    if (!isDir) {
                        // ran into a leaf node of some sort
                        result = NO;
                        break;
                    }
                }
                else {
                    // subpath does not exist - create it
                    if (![self createDirectoryAtPath:subpath attributes:attributes]) {
                        // failed to create subpath
                        result = NO;
                        break;
                    }
                }
                i++; 
            }
            
            [subpath release];
        }
        
    }    
                            
    return result;
}

- (BOOL)_webkit_createDirectoryAtPathWithIntermediateDirectories:(NSString *)path attributes:(NSDictionary *)attributes
{
    // Be really optimistic - assume that in the common case, the directory exists.
    BOOL isDirectory;
    if ([self fileExistsAtPath:path isDirectory:&isDirectory] && isDirectory) {
	return YES;
    }

    // Assume the next most common case is that the parent directory already exists
    if ([self createDirectoryAtPath:path attributes:attributes]) {
	return YES;
    }

    // Do it the hard way 
    return [self _webkit_createIntermediateDirectoriesForPath:path attributes:attributes] && [self createDirectoryAtPath:path attributes:attributes];
}

- (BOOL)_webkit_createFileAtPathWithIntermediateDirectories:(NSString *)path contents:(NSData *)contents attributes:(NSDictionary *)attributes directoryAttributes:(NSDictionary *)directoryAttributes
{
    // Be optimistic - try just creating the file first, assuming intermediate directories exist. 
    if ([self createFileAtPath:path contents:contents attributes:attributes]) {
	return YES;
    }

    return ([self _webkit_createIntermediateDirectoriesForPath:path attributes:directoryAttributes] && [self createFileAtPath:path contents:contents attributes:attributes]);
}

- (BOOL)_webkit_removeFileOnlyAtPath:(NSString *)path
{
    struct statfs buf;
    BOOL result = unlink([path fileSystemRepresentation]) == 0;

    // For mysterious reasons, MNT_DOVOLFS is the flag for "supports resource fork"
    if ((statfs([path fileSystemRepresentation], &buf) == 0) && !(buf.f_flags & MNT_DOVOLFS)) {
	NSString *lastPathComponent = [path lastPathComponent];
	if ([lastPathComponent length] != 0 && ![lastPathComponent isEqualToString:@"/"]) {
	    NSString *resourcePath = [[path stringByDeletingLastPathComponent] stringByAppendingString:[@"._" stringByAppendingString:lastPathComponent]];
	    if (unlink([resourcePath fileSystemRepresentation]) != 0) {
		result = NO;
	    }
	}
    }

    return result;
}

- (void)_webkit_backgroundRemoveFileAtPath:(NSString *)path
{
    NSFileManager *manager;
    NSString *moveToSubpath;
    NSString *moveToPath;
    int i;
    
    manager = [NSFileManager defaultManager];
    
    i = 0;
    moveToSubpath = [path stringByDeletingLastPathComponent];
    do {
        moveToPath = [NSString stringWithFormat:@"%@/.tmp%d", moveToSubpath, i];
        i++;
    } while ([manager fileExistsAtPath:moveToPath]);

    if ([manager movePath:path toPath:moveToPath handler:nil]) {
        [NSThread detachNewThreadSelector:@selector(_performRemoveFileAtPath:) toTarget:self withObject:moveToPath];
    }

}

- (void)_webkit_backgroundRemoveLeftoverFiles:(NSString *)path
{
    NSFileManager *manager;
    NSString *leftoverSubpath;
    NSString *leftoverPath;
    int i;
    
    manager = [NSFileManager defaultManager];
    leftoverSubpath = [path stringByDeletingLastPathComponent];
    
    i = 0;
    while (1) {
        leftoverPath = [NSString stringWithFormat:@"%@/.tmp%d", leftoverSubpath, i];
        if (![manager fileExistsAtPath:leftoverPath]) {
            break;
        }
        [NSThread detachNewThreadSelector:@selector(_performRemoveFileAtPath:) toTarget:self withObject:leftoverPath];
        i++;
    }
}

- (NSString *)_webkit_carbonPathForPath:(NSString *)posixPath
{
    OSStatus error;
    FSRef ref, rootRef, parentRef;
    FSCatalogInfo info;
    NSMutableArray *carbonPathPieces;
    HFSUniStr255 nameString;

    // Make an FSRef.
    error = FSPathMakeRef((const UInt8 *)[posixPath fileSystemRepresentation], &ref, NULL);
    if (error != noErr) {
        return nil;
    }

    // Get volume refNum.
    error = FSGetCatalogInfo(&ref, kFSCatInfoVolume, &info, NULL, NULL, NULL);
    if (error != noErr) {
        return nil;
    }

    // Get root directory FSRef.
    error = FSGetVolumeInfo(info.volume, 0, NULL, kFSVolInfoNone, NULL, NULL, &rootRef);
    if (error != noErr) {
        return nil;
    }

    // Get the pieces of the path.
    carbonPathPieces = [NSMutableArray array];
    for (;;) {
        error = FSGetCatalogInfo(&ref, kFSCatInfoNone, NULL, &nameString, NULL, &parentRef);
        if (error != noErr) {
            return nil;
        }
        [carbonPathPieces insertObject:[NSString stringWithCharacters:nameString.unicode length:nameString.length] atIndex:0];
        if (FSCompareFSRefs(&ref, &rootRef) == noErr) {
            break;
        }
        ref = parentRef;
    }

    // Volume names need trailing : character.
    if ([carbonPathPieces count] == 1) {
        [carbonPathPieces addObject:@""];
    }

    return [carbonPathPieces componentsJoinedByString:@":"];
}

- (NSString *)_webkit_startupVolumeName
{
    NSString *path = [self _webkit_carbonPathForPath:@"/"];
    return [path substringToIndex:[path length]-1];
}

- (NSString *)_webkit_pathWithUniqueFilenameForPath:(NSString *)path
{
    // "Fix" the filename of the path.
    NSString *filename = [[path lastPathComponent] _webkit_filenameByFixingIllegalCharacters];
    path = [[path stringByDeletingLastPathComponent] stringByAppendingPathComponent:filename];

    NSFileManager *fileManager = [NSFileManager defaultManager];
    if ([fileManager fileExistsAtPath:path]) {
        // Don't overwrite existing file by appending "-n", "-n.ext" or "-n.ext.ext" to the filename.
        NSString *extensions = nil;
        NSString *pathWithoutExtensions;
        NSString *lastPathComponent = [path lastPathComponent];
        NSRange periodRange = [lastPathComponent rangeOfString:@"."];
        
        if (periodRange.location == NSNotFound) {
            pathWithoutExtensions = path;
        } else {
            extensions = [lastPathComponent substringFromIndex:periodRange.location + 1];
            lastPathComponent = [lastPathComponent substringToIndex:periodRange.location];
            pathWithoutExtensions = [[path stringByDeletingLastPathComponent] stringByAppendingPathComponent:lastPathComponent];
        }

        NSString *pathWithAppendedNumber;
        unsigned i;

        for (i = 1; 1; i++) {
            pathWithAppendedNumber = [NSString stringWithFormat:@"%@-%d", pathWithoutExtensions, i];
            path = [extensions length] ? [pathWithAppendedNumber stringByAppendingPathExtension:extensions] : pathWithAppendedNumber;
            if (![fileManager fileExistsAtPath:path]) {
                break;
            }
        }
    }

    return path;
}

@end
