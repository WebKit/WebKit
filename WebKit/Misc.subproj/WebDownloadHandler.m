//
//  WebDownloadHandler.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Apr 11 2002.
//  Copyright (c) 2002 Apple Computer, Inc.
//

#import <WebKit/WebDownloadHandler.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebKitDebug.h>

@implementation WebDownloadHandler

- initWithDataSource:(WebDataSource *)dSource
{
    [super init];
    
    dataSource = [dSource retain];
    WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "Download started for: %s", [[[dSource inputURL] absoluteString] cString]);
    return self;
}

- (void)dealloc
{
    [fileHandle release];
    [dataSource release];
    
    [super dealloc];
}

- (void)receivedData:(NSData *)data
{
    NSString *path = [[dataSource contentPolicy] path];
    NSString *pathWithoutExtension, *newPathWithoutExtension, *extension;
    NSFileManager *fileManager;
    NSWorkspace *workspace;
    unsigned i;
    
    if(!fileHandle){
        fileManager = [NSFileManager defaultManager];
        
        if([fileManager fileExistsAtPath:path]){
            pathWithoutExtension = [path stringByDeletingPathExtension];
            extension = [path pathExtension];
            
            for(i=1; 1; i++){
                newPathWithoutExtension = [NSString stringWithFormat:@"%@-%d", pathWithoutExtension, i];
                path = [newPathWithoutExtension stringByAppendingPathExtension:extension];
                if(![fileManager fileExistsAtPath:path]){
                    //[dataSource _setDownloadPath:path];
                    break;
                }
            }
        }
        
        if(![fileManager createFileAtPath:path contents:nil attributes:nil]){
            [dataSource stopLoading];
            // FIXME: send error
            return;
        }
        
        fileHandle = [[NSFileHandle fileHandleForWritingAtPath:path] retain];
        
        workspace = [NSWorkspace sharedWorkspace];
        [workspace noteFileSystemChanged:path];
    }
    
    [fileHandle writeData:data];
}

- (void)finishedLoading
{
    NSString *path = [[dataSource contentPolicy] path];
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    
    [fileHandle closeFile];
    WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "Download complete. Saved to: %s", [path cString]);
    
    if([[dataSource contentPolicy] policyAction] == WebContentPolicySaveAndOpenExternally){
        [workspace openFile:path];
    }
}

- (void)cancel
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSString *path = [[dataSource contentPolicy] path];
    
    [fileHandle closeFile];
    [fileManager removeFileAtPath:path handler:nil];
    [workspace noteFileSystemChanged:path];
}


@end
