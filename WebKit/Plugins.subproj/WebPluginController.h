//
//  WebPluginController.h
//  WebKit
//
//  Created by Chris Blumenberg on Wed Oct 23 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <WebKit/WebPluginContainer.h>

@class WebFrame;

@interface WebPluginController : NSObject <WebPluginContainer>
{
    WebFrame *frame;
}

- initWithWebFrame:(WebFrame *)theFrame;

@end
