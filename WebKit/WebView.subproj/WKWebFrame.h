/*	
        WKWebFrame.h
	    
	    Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

@class WKWebDataSource;

@interface WKWebFrame : NSObject
{
    NSString *name;
    id view;
    WKWebDataSource *dataSource;
}
- initWithName: (NSString *)name view: view dataSource: (WKWebDataSource *)dataSource;
- (NSString *)name;
- (void)setView: view;
- view;
- (WKWebDataSource *)dataSource;
@end