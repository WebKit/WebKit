/*	IFWebDataSourcePrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _dataSourcePrivate in
        NSWebPageDataSource.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/IFWebDataSource.h>

// includes from kde
#include <khtmlview.h>

@interface IFWebDataSourcePrivate : NSObject
{
    IFWebDataSource *parent;
    NSMutableArray *children;
    id <IFWebController>controller;
    NSURL *inputURL;
    KHTMLPart *part;
    IFWebFrame *frame;
    // Child frames of this frame.
    NSMutableDictionary *frames;
}

- init;
- (void)dealloc;

@end

@interface IFWebDataSource (IFPrivate)
- (void)_setController: (id <IFWebController>)controller;
- (KHTMLPart *)_part;
- (void)_setParent: (IFWebDataSource *)p;
@end
