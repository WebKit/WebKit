//
//  IFMIMEDatabase.m
//  WebKit
//
//  Created by Chris Blumenberg on Fri Mar 22 2002.
//  Copyright (c) 2002 Apple Computer Inc.. All rights reserved.
//

#import "IFMIMEDatabase.h"


@implementation IFMIMEDatabase

+ (IFMIMEDatabase *)sharedMIMEDatabase
{
    return nil;
}


- (IFMIMEHandler *)MIMEHandlerForMIMEType:(NSString *)mimeType
{
    return nil;
}


- (IFMIMEHandler *)MIMEHandlerForURL:(NSURL *)url
{
    return nil;
}

@end
