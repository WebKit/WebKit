/*	IFWebDataSourcePrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _dataSourcePrivate in
        NSWebPageDataSource.
*/
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFException.h>




@implementation IFWebDataSourcePrivate 

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
    // controller is not retained!  IFWebControllers maintain
    // a reference to their view and main data source.
    [parent release];
    [frames release];
    [frame release];
    [inputURL release];
    
    delete part;

    [super dealloc];
}

@end

@implementation IFWebDataSource (IFPrivate)
- (void)_setController: (id <IFWebController>)controller
{
    //if (((IFWebDataSourcePrivate *)_dataSourcePrivate)->parent != nil)
        //[NSException raise:IFRuntimeError format:@"IFWebDataSource::_setController: called not called on main data source."];
    ((IFWebDataSourcePrivate *)_dataSourcePrivate)->controller = controller;
    ((IFWebDataSourcePrivate *)_dataSourcePrivate)->part->setDataSource (self);
}


- (KHTMLPart *)_part
{
    return ((IFWebDataSourcePrivate *)_dataSourcePrivate)->part;
}

- (void)_setParent: (IFWebDataSource *)p
{
    ((IFWebDataSourcePrivate *)_dataSourcePrivate)->parent = [p retain];
}

- (void)_startLoading: (BOOL)forceRefresh initiatedByMouseEvent: (BOOL)byMouseEvent
{
    KURL url = [[[self inputURL] absoluteString] cString];
    
    [self _part]->openURL (url);
    
    [[self controller] locationChangeStartedForFrame: [self frame] initiatedByMouseEvent: byMouseEvent];
}

@end
