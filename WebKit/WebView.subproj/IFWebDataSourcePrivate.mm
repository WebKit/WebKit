/*	IFWebDataSourcePrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _dataSourcePrivate in
        NSWebPageDataSource.
*/
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFException.h>




@implementation WKWebDataSourcePrivate 

- init
{
    parent = nil;
    frames = nil;
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
    [frames release];
    
    delete part;
}

@end

@implementation WKWebDataSource (WKPrivate)
- (void)_setController: (id <WKWebController>)controller
{
    if (((WKWebDataSourcePrivate *)_dataSourcePrivate)->parent != nil)
        [NSException raise:WKRuntimeError format:@"WKWebDataSource::_setController: called not called on main data source."];
    ((WKWebDataSourcePrivate *)_dataSourcePrivate)->controller = controller;
    ((WKWebDataSourcePrivate *)_dataSourcePrivate)->part->setDataSource (self);
}


- (KHTMLPart *)_part
{
    return ((WKWebDataSourcePrivate *)_dataSourcePrivate)->part;
}

- (void)_setFrameName: (NSString *)fname
{
    ((WKWebDataSourcePrivate *)_dataSourcePrivate)->frameName = [fname retain];
}

@end
