/*
 * Copyright (C) 2002 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import <WebCoreBridge.h>

#import <KWQKHTMLPartImpl.h>

@implementation WebCoreBridge

- init
{
    [super init];
    
    part = new KHTMLPart;
    part->impl->setBridge(self);
    
    return self;
}

- (void)dealloc
{
    part->deref();
    
    [super dealloc];
}

- (KHTMLPart *)part
{
    return part;
}

- (WebCoreBridge *)parent
{
    return nil;
}

- (NSArray *)children
{
    return nil;
}

- (void)loadURL:(NSURL *)URL
{
}

- (void)postWithURL:(NSURL *)URL data:(NSData *)data
{
}

- (BOOL)createNewFrameNamed:(NSString *)frameName
    withURL:(NSURL *)URL renderPart:(khtml::RenderPart *)renderPart
    allowsScrolling:(BOOL)allowsScrolling marginWidth:(int)width marginHeight:(int)height
{
    return NO;
}

- (void)openNewWindowWithURL:(NSURL *)URL
{
}

- (void)setTitle:(NSString *)title
{
}

- (WebCoreBridge *)mainFrame
{
    return nil;
}

- (WebCoreBridge *)frameNamed:(NSString *)name
{
    return nil;
}

- (KHTMLView *)widget
{
    return 0;
}

- (IFWebDataSource *)dataSource
{
    return nil;
}

@end
