//
//  WebImageRendererFactory.h
//  WebKit
//
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCore/WebCoreImageRendererFactory.h>

@class WebImageRenderer;

@interface WebImageRendererFactory : WebCoreImageRendererFactory <WebCoreImageRendererFactory>
{
}

+ (void)createSharedFactory;
+ (WebImageRendererFactory *)sharedFactory;
- (id <WebCoreImageRenderer>)imageRendererWithData:(NSData*)data MIMEType:(NSString *)MIMEType;

@end
