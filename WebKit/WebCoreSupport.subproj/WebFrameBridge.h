//
//  WebFrameBridge.h
//  WebKit
//
//  Created by Darin Adler on Sun Jun 16 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCore/WebCoreFrameBridge.h>

@class WebDataSource;
@class WebFrame;

@interface WebFrameBridge : WebCoreFrameBridge <WebCoreFrameBridge>
{
    WebFrame *frame;
}

- initWithWebFrame:(WebFrame *)frame;

- (void)loadURL:(NSURL *)URL attributes:(NSDictionary *)attributes flags:(unsigned)flags withParent:(WebDataSource *)parent;

@end
