//
//  IFDownloadHandler.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Apr 11 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <WebKit/IFDownloadHandler.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/WebKitDebug.h>

@implementation IFDownloadHandler

- initWithDataSource:(IFWebDataSource *)dSource
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
    NSString *path = [dataSource downloadPath];
    NSFileManager *fileManager;
    NSWorkspace *workspace;
    
    if(!fileHandle){
        // FIXME: Should not replace existing file
        // FIXME: Handle errors
        fileManager = [NSFileManager defaultManager];
        [fileManager createFileAtPath:path contents:nil attributes:nil];
        fileHandle = [[NSFileHandle fileHandleForWritingAtPath:path] retain];
        
        workspace = [NSWorkspace sharedWorkspace];
        [workspace noteFileSystemChanged:path];
    }
    
    [fileHandle writeData:data];
}

- (void)finishedLoading
{
    NSString *path = [dataSource downloadPath];
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    
    [fileHandle closeFile];
    WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "Download complete. Saved to: %s", [path cString]);
    
    workspace = [NSWorkspace sharedWorkspace];
    [workspace noteFileSystemChanged:path];
    
    if([dataSource contentPolicy] == IFContentPolicyOpenExternally){
        [workspace openFile:path];
    }
}

- (void)cancel
{
    [fileHandle closeFile];
    // FIXME: Do something to mark it as resumable?
}


@end
