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

#ifdef __cplusplus

class KHTMLPart;
class KHTMLView;

namespace khtml {
    class RenderPart;
}

typedef khtml::RenderPart KHTMLRenderPart;

#else

typedef struct KHTMLPart KHTMLPart;
typedef struct KHTMLView KHTMLView;
typedef struct KHTMLRenderPart KHTMLRenderPart;

#endif

@class IFError;
@class IFURLHandle;

@class WebCoreBridge;

@protocol WebCoreResourceLoader <NSObject>

- (void)addData:(NSData *)data;

// Either cancel or finish will be called before the loader is released, but never both.
- (void)cancel;
- (void)finish;

@end

// The WebCoreBridge class contains methods for use by the non-WebCore side of the bridge.

@interface WebCoreBridge : NSObject
{
    KHTMLPart *part;
}

- (KHTMLPart *)part;

- (void)openURL:(NSURL *)URL;
- (void)addData:(NSData *)data withEncoding:(NSString *)encoding;
- (void)closeURL;
- (void)end;

- (void)setURL:(NSURL *)URL;

- (void)scrollToBaseAnchor;

- (NSString *)documentTextFromDOM;

- (KHTMLView *)createKHTMLViewWithNSView:(NSView *)view
    width:(int)width height:(int)height
    marginWidth:(int)mw marginHeight:(int)mh;

@end

// The WebCoreBridge protocol contains methods for use by the WebCore side of the bridge.

@protocol WebCoreBridge

- (WebCoreBridge *)parentFrame;
- (NSArray *)childFrames; // WebCoreBridge objects
- (WebCoreBridge *)childFrameNamed:(NSString *)name;
- (WebCoreBridge *)descendantFrameNamed:(NSString *)name;

- (WebCoreBridge *)mainFrame;
- (WebCoreBridge *)frameNamed:(NSString *)name; // searches entire hierarchy starting with mainFrame

- (void)setTitle:(NSString *)title;

- (void)loadURL:(NSURL *)URL;
- (void)postWithURL:(NSURL *)URL data:(NSData *)data;

- (BOOL)createChildFrameNamed:(NSString *)frameName
    withURL:(NSURL *)URL renderPart:(KHTMLRenderPart *)renderPart
    allowsScrolling:(BOOL)allowsScrolling marginWidth:(int)width marginHeight:(int)height;

- (void)openNewWindowWithURL:(NSURL *)URL;

- (KHTMLView *)widget;

- (IFURLHandle *)startLoadingResource:(id <WebCoreResourceLoader>)resourceLoader withURL:(NSURL *)URL;

@end

@interface WebCoreBridge (SubclassResponsibility) <WebCoreBridge>
@end
