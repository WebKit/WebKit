//
//  WebImageRendererFactory.h
//  WebKit
//
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCore/WebCoreImageRendererFactory.h>

@class WebImageRenderer;

@interface WebImageRendererFactory : WebCoreImageRendererFactory
{
}

+ (void)createSharedFactory;
+ (WebImageRendererFactory *)sharedFactory;
- (id <WebCoreImageRenderer>)imageRendererWithBytes: (const void *)bytes length:(unsigned)length;

@end
