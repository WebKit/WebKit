/*	
        WebPreferencesPrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebPreferences.h>

@interface WebPreferences (WebPrivate)

- (int)_pageCacheSize;
- (int)_objectCacheSize;
- (void)_postPreferencesChangesNotification;
+ (WebPreferences *)_getInstanceForIdentifier:(NSString *)identifier;
+ (void)_setInstance:(WebPreferences *)instance forIdentifier:(NSString *)identifier;
+ (void)_removeReferenceForIdentifier:(NSString *)identifier;
- (NSTimeInterval)_backForwardCacheExpirationInterval;

+ (void)_setIBCreatorID:(NSString *)string;
@end
