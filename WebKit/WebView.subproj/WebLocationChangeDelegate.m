/*	
        WebLocationChangeHandler.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebLocationChangeHandler.h"

@implementation WebLocationChangeHandler

- (void)locationChangeStartedForDataSource:(WebDataSource *)dataSource { }
- (void)locationChangeCommittedForDataSource:(WebDataSource *)dataSource { }
- (void)locationChangeDone:(WebError *)error forDataSource:(WebDataSource *)dataSource { }

- (void)receivedPageTitle:(NSString *)title forDataSource:(WebDataSource *)dataSource { }
- (void)receivedPageIcon:(NSImage *)image forDataSource:(WebDataSource *)dataSource { }

- (void)serverRedirectTo:(NSURL *)URL forDataSource:(WebDataSource *)dataSource { }

- (void)clientRedirectTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date forFrame:(WebFrame *)frame { }
- (void)clientRedirectCancelledForFrame:(WebFrame *)frame { }

@end
