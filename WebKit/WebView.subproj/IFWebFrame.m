/*	
        IFWebFrame.h
	    
	    Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebDataSource.h>

@implementation WKWebFrame

- initWithName: (NSString *)n view: v dataSource: (WKWebDataSource *)d
{
    [super init];
    name = [n retain];
    view = [v retain];
    dataSource = [d retain];
    return self;
}

- (void)dealloc
{
    [name release];
    [view release];
    [dataSource release];
}

- (NSString *)name
{
    return name;
}


- (void)setView: v
{
    if (view)
        [view autorelease];
    view = [v retain];
}

- view
{
    return view;
}


- (WKWebDataSource *)dataSource
{
    return dataSource;
}


@end
