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

/*!
    @method setPrivateBrowsingEnabled:
    @param flag 
    @abstract If private browsing is enabled, WebKit will not store information
    about sites the user visits.
*/
- (void)setPrivateBrowsingEnabled:(BOOL)flag;

/*!
    @method privateBrowsingEnabled
 */
- (BOOL)privateBrowsingEnabled;

/*!
    @method setTabsToLinks:
    @param flag 
    @abstract If tabsToLinks is YES, the tab key will focus links and form controls. 
    The option key temporarily reverses this preference.
*/
- (void)setTabsToLinks:(BOOL)flag;

/*!
    @method tabsToLinks
*/
- (BOOL)tabsToLinks;

@end
