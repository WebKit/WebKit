/*	WKWebDataSourcePrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _dataSourcePrivate in
        NSWebPageDataSource.
*/
#import <WebKit/WKWebDataSourcePrivate.h>
#import <WebKit/WKException.h>




@implementation WKWebDataSourcePrivate 

- init
{
    parent = nil;
    children = nil;
    controller = nil;
    inputURL = nil;

    part = new KHTMLPart();
    
    return self;
}

- (void)dealloc
{
    // controller is not retained!  WKWebControllers maintain
    // a reference to their view and main data source.
    [parent release];
    [children release];
    
    //delete part;
}

@end

@implementation WKWebDataSource (WKPrivate)
- (void)_setController: (id <WKWebController>)controller
{
    if (((WKWebDataSourcePrivate *)_dataSourcePrivate)->parent != nil)
        [NSException raise:WKRuntimeError format:@"WKWebDataSource::_setController: called not called on main data source."];
    ((WKWebDataSourcePrivate *)_dataSourcePrivate)->controller = controller;
}


- (KHTMLPart *)_part
{
    return ((WKWebDataSourcePrivate *)_dataSourcePrivate)->part;
}


@end