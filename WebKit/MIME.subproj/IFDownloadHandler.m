//
//  IFDownloadHandler.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Apr 11 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <WebKit/IFDownloadHandler.h>
#import <WebKit/IFDownloadHandlerPrivate.h>

@implementation IFDownloadHandler

- (NSURL *) url
{
    return [_downloadHandlerPrivate _url];
}

- (IFMIMEHandler *) mimeHandler
{
    return [_downloadHandlerPrivate _mimeHandler];
}

- (NSString *) suggestedFilename
{
    return [_downloadHandlerPrivate _suggestedFilename];
}

- (void) cancelDownload
{
    [_downloadHandlerPrivate _cancelDownload];
}

- (void) storeAtPath:(NSString *)path
{
    [_downloadHandlerPrivate _storeAtPath:path];
}

- (void) dealloc
{
    [_downloadHandlerPrivate release];
}

- (void) openAfterDownload:(BOOL)open
{
    [_downloadHandlerPrivate _openAfterDownload:open];
}

@end
