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
    return [((IFDownloadHandlerPrivate *)_downloadHandlerPrivate) _url];
}

- (IFMIMEHandler *) mimeHandler
{
    return [((IFDownloadHandlerPrivate *)_downloadHandlerPrivate) _mimeHandler];
}

- (NSString *) suggestedFilename
{
    return [((IFDownloadHandlerPrivate *)_downloadHandlerPrivate) _suggestedFilename];
}

- (void) cancelDownload
{
    [((IFDownloadHandlerPrivate *)_downloadHandlerPrivate) _cancelDownload];
}

- (void) storeAtPath:(NSString *)path
{
    [((IFDownloadHandlerPrivate *)_downloadHandlerPrivate) _storeAtPath:path];
}

- (void) dealloc
{
    [_downloadHandlerPrivate release];
}

- (void) openAfterDownload:(BOOL)open
{
    [((IFDownloadHandlerPrivate *)_downloadHandlerPrivate) _openAfterDownload:open];
}

@end
