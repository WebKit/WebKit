//
//  WebDownloadHandler.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Apr 11 2002.
//  Copyright (c) 2002 Apple Computer, Inc.
//

#import <WebKit/WebControllerPolicyDelegatePrivate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebDownloadHandler.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitLogging.h>

#import <WebFoundation/WebFoundation.h>

@implementation WebDownloadHandler

- initWithDataSource:(WebDataSource *)dSource
{
    [super init];
    
    dataSource = [dSource retain];
    LOG(Download, "Download started for: %s", [[[[dSource request] URL] absoluteString] cString]);
    return self;
}

- (void)dealloc
{
    [fileHandle release];
    [dataSource release];
    
    [super dealloc];
}

- (WebError *)errorWithCode:(int)code
{
    return [WebError errorWithCode:code inDomain:WebErrorDomainWebKit failingURL:[[dataSource URL] absoluteString]];
}

- (WebError *)receivedData:(NSData *)data
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
                    [[dataSource contentPolicy] _setPath:path];
                    break;
                }
            }
        }
        
        if(![fileManager createFileAtPath:path contents:nil attributes:nil]){
            return [self errorWithCode:WebErrorCannotCreateFile];
        }
        
        fileHandle = [[NSFileHandle fileHandleForWritingAtPath:path] retain];
        if(!fileHandle){
            return [self errorWithCode:WebErrorCannotOpenFile];
        }
        
        workspace = [NSWorkspace sharedWorkspace];
        [workspace noteFileSystemChanged:path];
    }
    
    [fileHandle writeData:data];

    return nil;
}

- (WebError *)finishedLoading
{
    NSString *path = [[dataSource contentPolicy] path];
    
    [fileHandle closeFile];

    LOG(Download, "Download complete. Saved to: %s", [path cString]);
    
    if([[dataSource contentPolicy] policyAction] == WebContentPolicySaveAndOpenExternally){
        if(![[NSWorkspace sharedWorkspace] openFile:path]){
            return [self errorWithCode:WebErrorCannotFindApplicationForFile];
        }
    }

    return nil;
}

- (WebError *)cancel
{
    NSString *path = [[dataSource contentPolicy] path];
    
    [fileHandle closeFile];
    if(![[NSFileManager defaultManager] removeFileAtPath:path handler:nil]){
        return [self errorWithCode:WebErrorCannotRemoveFile];
    }else{
        [[NSWorkspace sharedWorkspace] noteFileSystemChanged:path];
    }

    return nil;
}


@end
