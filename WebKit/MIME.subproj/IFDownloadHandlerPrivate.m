//
//  IFDownloadHandlerPrivate.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Apr 11 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <WebKit/IFDownloadHandlerPrivate.h>
#import <ApplicationServices/ApplicationServices.h>

@implementation IFDownloadHandlerPrivate


- init
{
    //FIXME: we should not open files by default
    openAfterDownload = YES;
    return self;
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

- (void) _cancelDownload
{
    NSFileManager *fileManager;
    
    [urlHandle cancelLoadInBackground];
    if(path){
        fileManager = [NSFileManager defaultManager];
        [fileManager removeFileAtPath:path handler:nil];
    }
}

- (void) _storeAtPath:(NSString *)newPath
{
    NSFileManager *fileManager=nil;
    
    if(path){
        [fileManager movePath:path toPath:newPath handler:nil];
        [path release];
    }
    path = [newPath retain];
}

- (void) _openDownloadedFile
{
    CFURLRef pathURL;
    pathURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, (CFStringRef)path, kCFURLPOSIXPathStyle, FALSE);
    NSLog(@"Opening: %@", path);
    LSOpenCFURLRef(pathURL, NULL);
    CFRelease(pathURL);
}

- (void) _finishedDownload
{
    NSFileManager *fileManager;
    NSString *filename;
    
    if(!path){
        //FIXME: need a better way to get a file name out of a URL
        //FIXME: need to save temp file in file cache or somewhere else
        filename = [[[urlHandle url] absoluteString] lastPathComponent];
        path = [NSHomeDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"Desktop/%@", filename]];
        [path retain];
    }
    fileManager = [NSFileManager defaultManager];
    [fileManager createFileAtPath:path contents:[urlHandle resourceData] attributes:nil];
    
    //FIXME: need to send FNNotify here
    NSLog(@"Downloaded completed. Saved to: %@", path);
    
    if(openAfterDownload)
        [self _openDownloadedFile];
}

- (void) _setOpenAfterDownload:(BOOL)open
{
    openAfterDownload = open;
    if(path){
        [self _openDownloadedFile];
    }
}

- (void) dealloc
{
    [mimeHandler release];
    [urlHandle release];
    [path release];
}

@end

@implementation IFDownloadHandler (IFPrivate)

- _initWithURLHandle:(IFURLHandle *)uHandle mimeHandler:(IFMIMEHandler *)mHandler
{
    ((IFDownloadHandlerPrivate *)_downloadHandlerPrivate) = [[IFDownloadHandlerPrivate alloc] init];
    [((IFDownloadHandlerPrivate *)_downloadHandlerPrivate) _setURLHandle:uHandle];
    [((IFDownloadHandlerPrivate *)_downloadHandlerPrivate) _setMIMEHandler:mHandler];
    return self;
}

- (void) _receivedData:(NSData *)data
{

}

- (void) _finishedDownload
{
    [((IFDownloadHandlerPrivate *)_downloadHandlerPrivate) _finishedDownload];
}

@end