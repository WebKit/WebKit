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

- (void) setOpenAfterDownload:(BOOL)open
{
    [((IFDownloadHandlerPrivate *)_downloadHandlerPrivate) _setOpenAfterDownload:open];
}

@end
