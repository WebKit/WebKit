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
                    [dataSource _setDownloadPath:path];
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
    NSString *path = [dataSource downloadPath];
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    
    [fileHandle closeFile];
    WEBKITDEBUGLEVEL(WEBKIT_LOG_DOWNLOAD, "Download complete. Saved to: %s", [path cString]);
    
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
