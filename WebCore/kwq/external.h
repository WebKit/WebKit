/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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
 
#ifndef __EXTERNAL_H
#define __EXTERNAL_H_

#import <WebFoundation/WebFoundation.h>


@class IFWebDataSource;
@class IFWebView;
@class IFWebFrame;
@class IFError;
@class IFURLHandle;
@class IFLoadProgress;

@protocol IFWebController
- (IFWebFrame *)createFrameNamed: (NSString *)name for: (IFWebDataSource *)dataSource inParent: (IFWebDataSource *)dataSource inScrollView: (BOOL)inScrollView;
- (IFWebFrame *)frameNamed: (NSString *)name;
@end

@interface IFBaseWebController
- (void)_receivedProgress: (IFLoadProgress *)progress forResource: (NSString *)resourceDescription fromDataSource: (IFWebDataSource *)dataSource;
- (void)_receivedError: (IFError *)error forResource: (NSString *)resourceDescription partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource;
- (void)_mainReceivedProgress: (IFLoadProgress *)progress forResource: (NSString *)resourceDescription fromDataSource: (IFWebDataSource *)dataSource;
- (void)_mainReceivedError: (IFError *)error forResource: (NSString *)resourceDescription partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource;
@end

@protocol IFLocationChangeHandler
- (void)serverRedirectTo: (NSURL *)url forDataSource: (IFWebDataSource *)dataSource;
@end

@interface IFWebDataSource : NSObject
- initWithURL: (NSURL *)url;
- (void)setFrame: (IFWebFrame *)fName;
- (IFWebFrame *)webFrame;
- (id <IFWebController>)controller;
- frameNamed: (NSString *)f;
- (BOOL)frameExists: (NSString *)f;
- (void)_setParent: (IFWebDataSource *)p;
- (IFWebDataSource *)parent;
- (void)_addURLHandle: (IFURLHandle *)handle;
- (void)_removeURLHandle: (IFURLHandle *)handle;
- controller;
- (KHTMLPart *)_part;
- (void)_setTitle: (NSString *)title;
- (void)_setFinalURL: (NSURL *)url;
- (id <IFLocationChangeHandler>)_locationChangeHandler;
@end

// This should not be allowed here.  data source should not reference view
// API.
@interface IFWebView: NSObject
- (QWidget *)_widget;
- (QWidget *)_provisionalWidget;
@end

@interface IFWebFrame: NSObject
- initWithName: (NSString *)n view: v dataSource: (IFWebDataSource *)d;
- view;
- (IFWebDataSource *)dataSource;
- (void)setProvisionalDataSource: (IFWebDataSource *)ds;
- (void)_setRenderFramePart: (void *)p;
- (void *)_renderFramePart;
- (void)startLoading;
@end

typedef enum {
    IF_LOAD_TYPE_CSS    = 1,
    IF_LOAD_TYPE_IMAGE  = 2,
    IF_LOAD_TYPE_SCRIPT = 3,
    IF_LOAD_TYPE_HTML   = 4
} IF_LOAD_TYPE;


@interface IFLoadProgress : NSObject
{
    int bytesSoFar;	// 0 if this is the start of load
    int totalToLoad;	// -1 if this is not known.
                        // bytesSoFar == totalLoaded when complete
    IF_LOAD_TYPE type;	// load types, either image, css, or jscript
}
- init;
@end

@protocol  IFLoadHandler
- (void)receivedProgress: (IFLoadProgress *)progress forResource: (NSString *)resourceDescription fromDataSource: (IFWebDataSource *)dataSource;

- (void)receivedError: (IFError *)error forResource: (NSString *)resourceDescription partialProgress: (IFLoadProgress *)progress fromDataSource: (IFWebDataSource *)dataSource;

@end


@interface URLLoadClient : NSObject <IFURLHandleClient>
{
    @public
    khtml::Loader *m_loader;
    id m_dataSource;
}

-(id)initWithLoader:(khtml::Loader *)loader dataSource: dataSource;

@end

@interface WCURLHandle
-(int)contentLength;
@end

#endif
