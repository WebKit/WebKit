/*	
        WebPreferences.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

@interface WebPreferences: NSObject

+ (WebPreferences *)standardPreferences;

- (NSString *)standardFontFamily;
- (void)setStandardFontFamily:(NSString *)family;

- (NSString *)fixedFontFamily;
- (void)setFixedFontFamily:(NSString *)family;

- (NSString *)serifFontFamily;
- (void)setSerifFontFamily:(NSString *)family;

- (NSString *)sansSerifFontFamily;
- (void)setSansSerifFontFamily:(NSString *)family;

- (NSString *)cursiveFontFamily;
- (void)setCursiveFontFamily:(NSString *)family;

- (NSString *)fantasyFontFamily;
- (void)setFantasyFontFamily:(NSString *)family;

- (int)defaultFontSize;
- (void)setDefaultFontSize:(int)size;

- (int)defaultFixedFontSize;
- (void)setDefaultFixedFontSize:(int)size;

- (int)minimumFontSize;
- (void)setMinimumFontSize:(int)size;

- (CFStringEncoding)defaultTextEncoding;
- (void)setDefaultTextEncoding:(CFStringEncoding)encoding;

- (BOOL)userStyleSheetEnabled;
- (void)setUserStyleSheetEnabled:(BOOL)flag;

// The user style sheet is stored as a URL string, e.g. "file://<etc>"
- (NSString *)userStyleSheetLocation;
- (void)setUserStyleSheetLocation:(NSString *)string;

- (BOOL)JavaEnabled;
- (void)setJavaEnabled:(BOOL)flag;

- (BOOL)JavaScriptEnabled;
- (void)setJavaScriptEnabled:(BOOL)flag;

- (BOOL)JavaScriptCanOpenWindowsAutomatically;
- (void)setJavaScriptCanOpenWindowsAutomatically:(BOOL)flag;

- (BOOL)pluginsEnabled;
- (void)setPluginsEnabled:(BOOL)flag;

- (BOOL)allowAnimatedImages;
- (void)setAllowAnimatedImages:(BOOL)flag;

- (BOOL)allowAnimatedImageLooping;
- (void)setAllowAnimatedImageLooping: (BOOL)flag;

- (void)setWillLoadImagesAutomatically: (BOOL)flag;
- (BOOL)willLoadImagesAutomatically;

@end

#ifdef READY_FOR_PRIMETIME
- setRefreshEnabled: (BOOL)flag;
- (BOOL)refreshEnabled;

/*
    Specify whether only local references ( stylesheets, images, scripts, subdocuments )
    should be loaded. ( default false - everything is loaded, if the more specific
    options allow )
    This is carried over from KDE.
*/
- (void)setOnlyLocalReferences: (BOOL)flag;
- (BOOL)onlyLocalReferences;

#endif
