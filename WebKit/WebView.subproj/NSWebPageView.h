/*	NSWebPageView.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Public header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/NSWebPageDataSource.h>

@interface NSWebPageView : NSView
{
@private
    id _viewPrivate;
}

- initWithFrame: (NSRect)frame dataSource: (NSWebPageDataSource *)dataSource;

- (NSWebPageDataSource *)dataSource;

@end