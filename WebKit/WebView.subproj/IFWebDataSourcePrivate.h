/*	WKWebDataSourcePrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _dataSourcePrivate in
        NSWebPageDataSource.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/WKWebDataSource.h>

// includes from kde
#include <khtmlview.h>

@interface WKWebDataSourcePrivate : NSObject
{
    WKWebDataSource *parent;
    NSMutableArray *children;
    id <WKWebController>controller;
    NSURL *inputURL;
    KHTMLPart *part;
    NSString *frameName;
    NSMutableDictionary *frames;
}

- init;
- (void)dealloc;

@end

@interface WKWebDataSource (WKPrivate)
- (void)_setController: (id <WKWebController>)controller;
- (KHTMLPart *)_part;
- (void)_setFrameName: (NSString *)fName;
@end