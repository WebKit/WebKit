//
//  KWQLogging.m
//  WebCore
//
//  Created by Darin Adler on Sun Sep 08 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "KWQLogging.h"

KWQLogChannel KWQLogNotYetImplemented = { 0x00000001, "WebCoreLogLevel", KWQLogChannelUninitialized };

KWQLogChannel KWQLogFrames =            { 0x00000010, "WebCoreLogLevel", KWQLogChannelUninitialized };
KWQLogChannel KWQLogLoading =           { 0x00000020, "WebCoreLogLevel", KWQLogChannelUninitialized };

KWQLogChannel KWQLogPopupBlocking =     { 0x00000040, "WebCoreLogLevel", KWQLogChannelUninitialized };
