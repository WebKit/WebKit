//
//  WebPluginPackage.h
//  WebKit
//
//  Created by Chris Blumenberg on Tue Oct 22 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <WebKit/WebBasePluginPackage.h>

@interface WebPluginPackage : WebBasePluginPackage
{
    NSBundle *bundle;
}

@end
