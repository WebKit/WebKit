//
//  IFWebCoreFrame.h
//  WebKit
//
//  Created by Darin Adler on Sun Jun 16 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCore/WebCoreFrame.h>

@class IFWebDataSource;
@class IFWebFrame;

@interface IFWebCoreFrame : WebCoreFrame <WebCoreFrame>
{
    IFWebFrame *frame;
}

- initWithWebFrame:(IFWebFrame *)frame;

- (void)loadURL:(NSURL *)URL attributes:(NSDictionary *)attributes flags:(unsigned)flags withParent:(IFWebDataSource *)parent;

@end
