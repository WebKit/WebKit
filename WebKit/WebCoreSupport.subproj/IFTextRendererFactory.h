//
//  IFTextRendererFactory.h
//  WebKit
//
//  Created by Darin Adler on Thu May 02 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCoreTextRendererFactory.h>

@class IFTextRenderer;

@interface IFTextRendererFactory : WebCoreTextRendererFactory
{
    NSMutableDictionary *cache;
}

+ (void)createSharedFactory;
+ (IFTextRendererFactory *)sharedFactory;
- init;

- (IFTextRenderer *)rendererWithFont:(NSFont *)font;

@end
