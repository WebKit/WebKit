//
//  WebPluginViewPrivate.h
//  WebKit
//
//  Created by Darin Adler on Tue Aug 27 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebPluginView.h>

@interface WebNetscapePluginView (WebNPPCallbacks)

- (NPError)getURLNotify:(const char *)URL target:(const char *)target notifyData:(void *)notifyData;
- (NPError)getURL:(const char *)URL target:(const char *)target;
- (NPError)postURLNotify:(const char *)URL target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file notifyData:(void *)notifyData;
- (NPError)postURL:(const char *)URL target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file;
- (NPError)newStream:(NPMIMEType)type target:(const char *)target stream:(NPStream**)stream;
- (NPError)write:(NPStream*)stream len:(SInt32)len buffer:(void *)buffer;
- (NPError)destroyStream:(NPStream*)stream reason:(NPReason)reason;
- (void)status:(const char *)message;
- (NPError)getValue:(NPNVariable)variable value:(void *)value;
- (NPError)setValue:(NPPVariable)variable value:(void *)value;
- (void)invalidateRect:(NPRect *)invalidRect;
- (void)invalidateRegion:(NPRegion)invalidateRegion;
- (void)forceRedraw;

@end
