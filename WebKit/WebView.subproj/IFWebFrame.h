/*	
        IFWebFrame.h
	    
	    Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

@class IFWebDataSource;

@interface IFWebFrame : NSObject
{
    NSString *name;
    id view;
    IFWebDataSource *dataSource;
}
- initWithName: (NSString *)name view: view dataSource: (IFWebDataSource *)dataSource;
- (NSString *)name;
- (void)setView: view;
- view;
- (IFWebDataSource *)dataSource;
@end
