//
//  WebNSPrintOperationExtras.h
//  WebKit
//
//  Created by John Sullivan on Wed Jan 28 2004.
//  Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface NSPrintOperation (WebKitExtras)

- (float)_web_pageSetupScaleFactor;

@end
