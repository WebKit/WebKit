/*
    WebNSFileManagerExtras.h
    Private (SPI) header
    Copyright 2005, Apple, Inc. All rights reserved.
 */

#import <Foundation/Foundation.h>

#define WEB_UREAD     (00400)   /* Read by owner */
#define WEB_UWRITE    (00200)   /* Write by owner */
#define WEB_UEXEC     (00100)   /* Execute/Search by owner */

@interface NSFileManager (WebNSFileManagerExtras)

- (BOOL)_webkit_createDirectoryAtPathWithIntermediateDirectories:(NSString *)path attributes:(NSDictionary *)attributes;
- (BOOL)_webkit_createFileAtPathWithIntermediateDirectories:(NSString *)path contents:(NSData *)contents attributes:(NSDictionary *)attributes directoryAttributes:(NSDictionary *)directoryAttributes;
- (void)_webkit_backgroundRemoveFileAtPath:(NSString *)path;
- (void)_webkit_backgroundRemoveLeftoverFiles:(NSString *)path;
- (BOOL)_webkit_removeFileOnlyAtPath:(NSString *)path;
- (NSString *)_webkit_startupVolumeName;
- (NSString *)_webkit_pathWithUniqueFilenameForPath:(NSString *)path;

@end

