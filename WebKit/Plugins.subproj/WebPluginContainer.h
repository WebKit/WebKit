//
//  WebPluginContainer.h
//
//  Created by Mike Hay <mhay@apple.com> on Tue Oct 15 2002.
//  Copyright (c) 2002 Apple Computer.  All rights reserved.
//

//
//  About WebPluginContainer
//
//    This protocol enables an applet to request that its
//    host application perform certain specific operations.
//

#import <Cocoa/Cocoa.h>


@protocol WebPluginContainer <NSObject>

//
//  showURL:inFrame:    plugin requests that the host app show the specified URL in the target frame.
//
//                      target may be nil.  if target is nil, the URL should be shown in a new window.
//
- (void)showURL:(NSURL *)URL inFrame:(NSString *)target;

//
//  showStatus:        plugin requests that the host app show the specified status message.
//
- (void)showStatus:(NSString *)message;
        
@end