//
//  IFCachedTextRendererFactory.h
//  WebKit
//
//  Created by Darin Adler on Thu May 02 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <IFTextRendererFactory.h>

@interface IFCachedTextRendererFactory : IFTextRendererFactory
{
    NSMutableDictionary *cache;
}

+ (void)createSharedFactory;
- init;

@end
