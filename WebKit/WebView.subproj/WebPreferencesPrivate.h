/*	
        WebPreferencesPrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebPreferences.h>

@interface WebPreferences (WebPrivate)

- (NSTimeInterval)_initialTimedLayoutDelay;
- (int)_initialTimedLayoutSize;
- (BOOL)_initialTimedLayoutEnabled;
- (BOOL)_resourceTimedLayoutEnabled;
- (NSTimeInterval)_resourceTimedLayoutDelay;
- (int)_pageCacheSize;
- (int)_objectCacheSize;
- (void)_postPreferencesChangesNotification;
+ (WebPreferences *)_getInstanceForIdentifier:(NSString *)identifier;
+ (void)_setInstance:(WebPreferences *)instance forIdentifier:(NSString *)identifier;
+ (void)_removeReferenceForIdentifier:(NSString *)identifier;

@end
