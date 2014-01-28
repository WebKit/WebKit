/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <UIKit/UIKit.h>
#import <WebKit2/WKBase.h>
#import <WebKit2/WKBrowsingContextController.h>
#import <WebKit2/WKBrowsingContextGroup.h>
#import <WebKit2/WKProcessGroup.h>

@class WKContentView;
@class WKWebViewConfiguration;

typedef NS_ENUM(unsigned, WKContentType)
{
    Standard = 0,
    PlainText,
    Image
};

@protocol WKContentViewDelegate <NSObject>
@optional
- (void)contentView:(WKContentView *)contentView contentsSizeDidChange:(CGSize)newSize;
- (void)contentViewDidCommitLoadForMainFrame:(WKContentView *)contentView;
- (void)contentViewDidReceiveMobileDocType:(WKContentView *)contentView;
- (void)contentView:(WKContentView *)contentView didChangeViewportArgumentsSize:(CGSize)newSize initialScale:(float)initialScale minimumScale:(float)minimumScale maximumScale:(float)maximumScale allowsUserScaling:(float)allowsUserScaling;

@end

WK_API_CLASS
@interface WKContentView : UIView

@property (readonly, nonatomic) WKBrowsingContextController *browsingContextController;

@property (nonatomic, assign) id <WKContentViewDelegate> delegate;
@property (nonatomic, readonly) WKContentType contentType;

@property (readonly) WKPageRef _pageRef;

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration;

- (id)initWithFrame:(CGRect)frame contextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef;
- (id)initWithFrame:(CGRect)frame contextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef relatedToPage:(WKPageRef)relatedPage;
- (id)initWithFrame:(CGRect)frame processGroup:(WKProcessGroup *)processGroup browsingContextGroup:(WKBrowsingContextGroup *)browsingContextGroup;

- (void)setMinimumSize:(CGSize)size;
- (void)setViewportSize:(CGSize)size;

- (void)didFinishScrollTo:(CGPoint)contentOffset;
- (void)didScrollTo:(CGPoint)contentOffset;
- (void)didZoomToScale:(CGFloat)scale;

@end
