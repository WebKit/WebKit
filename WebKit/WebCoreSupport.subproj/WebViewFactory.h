//
//  WebViewFactory.h
//  WebKit
//
//  Created by Darin Adler on Tue May 07 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCore/WebCoreViewFactory.h>

@interface WebViewFactory : WebCoreViewFactory <WebCoreViewFactory>
{
}

+ (void)createSharedFactory;

@end
