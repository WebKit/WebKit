//
//  IFDownloadHandlerPrivate.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Apr 11 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <WebKit/IFDownloadHandlerPrivate.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Carbon/Carbon.h>

#import "IFMIMEHandler.h"
#import <WebFoundation/IFURLHandle.h>

@implementation IFDownloadHandlerPrivate


- init
{
    shouldOpen = NO;
    downloadCompleted = NO;
    return self;
}

- (void) dealloc
{
    if(!downloadCompleted){
        [self _cancelDownload];
    }
    [mimeHandler release];
    [urlHandle release];
    [path release];
}

- (void) _setMIMEHandler:(IFMIMEHandler *) mHandler
{
    mimeHandler = [mHandler retain];
}

- (void) _setURLHandle:(IFURLHandle *)uHandle
{
    urlHandle = [uHandle retain];
}

- (NSURL *) _url
{
    return [urlHandle url];
}

- (IFMIMEHandler *) _mimeHandler
{
    return mimeHandler;
}

- (NSString *) _suggestedFilename
{
    //FIXME: need a better way to get a file name out of a URL
    return [[[urlHandle url] absoluteString] lastPathComponent];
}

- (void) _cancelDownload
{
    if(!downloadCompleted)
        [urlHandle cancelLoadInBackground];
}

- (void) _storeAtPath:(NSString *)newPath
{
    path = [newPath retain];
    if(downloadCompleted)
        [self _saveFile];
}

- (void) _finishedDownload
{    
    downloadCompleted = YES;

    if(path)
        [self _saveFile];
}

- (void) _openAfterDownload:(BOOL)open
{
    shouldOpen = open;
    if(shouldOpen && path && downloadCompleted)
        [self _openFile];
}

- (void) _openFile
{
    CFURLRef pathURL;
    pathURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)path, kCFURLPOSIXPathStyle, FALSE);
    NSLog(@"Opening: %@", path);
    LSOpenCFURLRef(pathURL, NULL);
    CFRelease(pathURL);
}

- (void) _saveFile
{
    NSFileManager *fileManager;
    
    if(path){
        // FIXME: Should probably not replace existing file
        // FIXME: Should report error if there is one
        fileManager = [NSFileManager defaultManager];
        [fileManager createFileAtPath:path contents:[urlHandle resourceData] attributes:nil];
        NSLog(@"Download complete. Saved to: %@", path);
        
        // Send Finder notification
        NSLog(@"Notifying Finder");
        FNNotifyByPath([[path stringByDeletingLastPathComponent] cString], kFNDirectoryModifiedMessage, kNilOptions);
        
        if(shouldOpen)
            [self _openFile];
    }
}

@end

@implementation IFDownloadHandler (IFPrivate)

- _initWithURLHandle:(IFURLHandle *)uHandle mimeHandler:(IFMIMEHandler *)mHandler
{
    _downloadHandlerPrivate = [[IFDownloadHandlerPrivate alloc] init];
    [_downloadHandlerPrivate _setURLHandle:uHandle];
    [_downloadHandlerPrivate _setMIMEHandler:mHandler];
    
    NSLog(@"Downloading: %@", [uHandle url]);
    
    return self;
}

- (void) _receivedData:(NSData *)data
{

}

- (void) _finishedDownload
{
    [_downloadHandlerPrivate _finishedDownload];
}

@end