/*	
        IFPreferences.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/IFPreferences.h>

@interface IFPreferences (IFPrivate)

- (NSTimeInterval)_initialTimedLayoutDelay;
- (int)_initialTimedLayoutSize;
- (BOOL)_initialTimedLayoutEnabled;
- (BOOL)_resourceTimedLayoutEnabled;
- (NSTimeInterval)_resourceTimedLayoutDelay;

@end

