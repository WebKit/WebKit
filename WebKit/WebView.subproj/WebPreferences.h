/*	
        IFPreferences.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

@interface IFPreferences: NSObject

+ (IFPreferences *)standardPreferences;

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

- (int)mediumFontSize;
- (void)setMediumFontSize:(int)size;

- (int)minimumFontSize;
- (void)setMinimumFontSize:(int)size;

- (BOOL)javaEnabled;
- (void)setJavaEnabled:(BOOL)flag;

- (BOOL)jScriptEnabled;
- (void)setJScriptEnabled:(BOOL)flag;

- (BOOL)pluginsEnabled;
- (void)setPluginsEnabled:(BOOL)flag;

- (BOOL)allowAnimatedImages;
- (void)setAllowAnimatedImages:(BOOL)flag;

- (BOOL)allowAnimatedImageLooping;
- (void)setAllowAnimatedImageLooping: (BOOL)flag;

@end

#ifdef READY_FOR_PRIMETIME

/*
   ============================================================================= 

    Here is some not-yet-implemented API that we might want to get around to.
    Someday we might have preferences on a per-URL basis.
*/

+ getPreferencesForURL: (NSURL *)url;

// Encoding that will be used in none specified on page? or in header?
+ setEncoding: (NSString *)encoding;
+ (NSString *)encoding;

// Document refreshes allowed
- setRefreshEnabled: (BOOL)flag;
- (BOOL)refreshEnabled;

// Should images be loaded.
- (void)setAutoloadImages: (BOOL)flag;
- (BOOL)autoloadImages;

/*
    Specify whether only local references ( stylesheets, images, scripts, subdocuments )
    should be loaded. ( default false - everything is loaded, if the more specific
    options allow )
    This is carried over from KDE.
*/
- (void)setOnlyLocalReferences: (BOOL)flag;
- (BOOL)onlyLocalReferences;

#endif
