//
//  WebLocalizedStringFactory.h
//  WebKit
//
//  Created by Chris Blumenberg on Thu Nov 20 2003.
//  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
//

#import <WebCore/WebCoreLocalizedStringFactory.h>


@interface WebLocalizedStringFactory : WebCoreLocalizedStringFactory
{
    NSArray *keyGenerationMenuItemTitles;
}
+ (void)createSharedFactory;
@end
