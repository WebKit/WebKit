/*	
        IFPreferences.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/IFPreferences.h>

@interface IFPreferences (IFPrivate)

- (NSTimeInterval)_initialTimedLayoutDelay;
- (BOOL)_initialTimedLayoutEnabled;

@end

