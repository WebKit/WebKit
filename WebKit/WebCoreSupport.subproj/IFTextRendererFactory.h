//
//  IFTextRendererFactory.h
//  WebKit
//
//  Created by Darin Adler on Thu May 02 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCoreTextRendererFactory.h>

@interface IFTextRendererFactory : WebCoreTextRendererFactory
{
    NSMutableDictionary *cache;
}

+ (void)createSharedFactory;
- init;
- (id <WebCoreTextRenderer>)rendererWithFont:(NSFont *)font;

@end
