//
//  IFImageRendererFactory.h
//  WebKit
//
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCoreImageRendererFactory.h>

@class IFImageRenderer;

@interface IFImageRendererFactory : WebCoreImageRendererFactory
{
}

+ (void)createSharedFactory;
+ (IFImageRendererFactory *)sharedFactory;
- (id <WebCoreImageRenderer>)imageRendererWithBytes: (const void *)bytes length:(unsigned)length;

@end
