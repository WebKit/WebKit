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
    return [_private _url];
}

- (IFMIMEHandler *) mimeHandler
{
    return [_private _mimeHandler];
}

- (NSString *) suggestedFilename
{
    return [_private _suggestedFilename];
}

- (void) cancelDownload
{
    [_private _cancelDownload];
}

- (void) storeAtPath:(NSString *)path
{
    [_private _storeAtPath:path];
}

- (void) dealloc
{
    [_private release];
}

- (void) openAfterDownload:(BOOL)open
{
    [_private _openAfterDownload:open];
}

@end
