//
//  WebTextRendererFactory.h
//  WebKit
//
//  Created by Darin Adler on Thu May 02 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCore/WebCoreTextRendererFactory.h>

@class WebTextRenderer;

@interface WebTextRendererFactory : WebCoreTextRendererFactory
{
    NSMutableDictionary *cache;
}

+ (void)createSharedFactory;
+ (WebTextRendererFactory *)sharedFactory;
- init;

- (WebTextRenderer *)rendererWithFont:(NSFont *)font;

@end
