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

#import <Foundation/Foundation.h>

class KHTMLPart;
class KHTMLView;

namespace khtml {
    class RenderPart;
}

@class IFWebDataSource; // temporary -- here only until I finish KWQKloader.mm

@interface WebCoreBridge : NSObject
{
    KHTMLPart *part;
}

- (KHTMLPart *)part;

- (WebCoreBridge *)parent;
- (NSArray *)children; // WebCoreBridge objects

- (WebCoreBridge *)mainFrame;
- (WebCoreBridge *)frameNamed:(NSString *)name; // always searches entire hierarchy starting with mainFrame

- (void)setTitle:(NSString *)title;

- (void)loadURL:(NSURL *)URL;
- (void)postWithURL:(NSURL *)URL data:(NSData *)data;

- (BOOL)createNewFrameNamed:(NSString *)frameName
    withURL:(NSURL *)URL renderPart:(khtml::RenderPart *)renderPart
    allowsScrolling:(BOOL)allowsScrolling marginWidth:(int)width marginHeight:(int)height;

- (void)openNewWindowWithURL:(NSURL *)URL;

- (KHTMLView *)widget;

- (IFWebDataSource *)dataSource; // temporary -- here only until I finish KWQKloader.mm

@end
