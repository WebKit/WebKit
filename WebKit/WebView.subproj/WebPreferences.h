/*	
        WebPreferences.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

/*!
    @class WebPreferences
*/
@interface WebPreferences: NSObject

/*!
    @method standardPreferences
*/
+ (WebPreferences *)standardPreferences;

/*!
    @method standardFontFamily
*/
- (NSString *)standardFontFamily;

/*!
    @method setStandardFontFamily:
    @param family
*/
- (void)setStandardFontFamily:(NSString *)family;

/*!
    @method fixedFontFamily
*/
- (NSString *)fixedFontFamily;

/*!
    @method setFixedFontFamily:
    @param family
*/
- (void)setFixedFontFamily:(NSString *)family;

/*!
    @method serifFontFamily
*/
- (NSString *)serifFontFamily;

/*!
    @method setSerifFontFamily:
    @param family
*/
- (void)setSerifFontFamily:(NSString *)family;

/*!
    @method sansSerifFontFamily
*/
- (NSString *)sansSerifFontFamily;

/*!
    @method setSansSerifFontFamily:
    @param family
*/
- (void)setSansSerifFontFamily:(NSString *)family;

/*!
    @method cursiveFontFamily
*/
- (NSString *)cursiveFontFamily;

/*!
    @method setCursiveFontFamily:
    @param family
*/
- (void)setCursiveFontFamily:(NSString *)family;

/*!
    @method fantasyFontFamily
*/
- (NSString *)fantasyFontFamily;

/*!
    @method setFantasyFontFamily:
    @param family
*/
- (void)setFantasyFontFamily:(NSString *)family;

/*!
    @method defaultFontSize
*/
- (int)defaultFontSize;

/*!
    @method setDefaultFontSize:
    @param size
*/
- (void)setDefaultFontSize:(int)size;

/*!
    @method defaultFixedFontSize
*/
- (int)defaultFixedFontSize;

/*!
    @method setDefaultFixedFontSize:
    @param size
*/
- (void)setDefaultFixedFontSize:(int)size;

/*!
    @method minimumFontSize
*/
- (int)minimumFontSize;

/*!
    @method setMinimumFontSize:
    @param size
*/
- (void)setMinimumFontSize:(int)size;

/*!
    @method defaultTextEncodingName
*/
- (NSString *)defaultTextEncodingName;

/*!
    @method setDefaultTextEncodingName:
    @param encoding
*/
- (void)setDefaultTextEncodingName:(NSString *)encoding;

/*!
    @method userStyleSheetEnabled
*/
- (BOOL)userStyleSheetEnabled;

/*!
    @method setUserStyleSheetEnabled:
    @param flag
*/
- (void)setUserStyleSheetEnabled:(BOOL)flag;

/*!
    @method userStyleSheetLocation
    @discussion The user style sheet is stored as a URL string, e.g. "file://<etc>"
*/
- (NSString *)userStyleSheetLocation;

/*!
    @method setUserStyleSheetLocation:
    @param string
*/
- (void)setUserStyleSheetLocation:(NSString *)string;

/*!
    @method JavaEnabled
*/
- (BOOL)JavaEnabled;

/*!
    @method setJavaEnabled:
    @param flag
*/
- (void)setJavaEnabled:(BOOL)flag;

/*!
    @method JavaScriptEnabled
*/
- (BOOL)JavaScriptEnabled;

/*!
    @method setJavaScriptEnabled:
    @param flag
*/
- (void)setJavaScriptEnabled:(BOOL)flag;

/*!
    @method JavaScriptCanOpenWindowsAutomatically
*/
- (BOOL)JavaScriptCanOpenWindowsAutomatically;

/*!
    @method setJavaScriptCanOpenWindowsAutomatically:
    @param flag
*/
- (void)setJavaScriptCanOpenWindowsAutomatically:(BOOL)flag;

/*!
    @method pluginsEnabled
*/
- (BOOL)pluginsEnabled;

/*!
    @method setJavaScriptCanOpenWindowsAutomatically:
    @param flag
*/
- (void)setPluginsEnabled:(BOOL)flag;

/*!
    @method allowAnimatedImages
*/
- (BOOL)allowAnimatedImages;

/*!
    @method setAllowAnimatedImages:
    @param flag
*/
- (void)setAllowAnimatedImages:(BOOL)flag;

/*!
    @method allowAnimatedImageLooping
*/
- (BOOL)allowAnimatedImageLooping;

/*!
    @method setAllowAnimatedImageLooping:
    @param flag
*/
- (void)setAllowAnimatedImageLooping: (BOOL)flag;

/*!
    @method setWillLoadImagesAutomatically:
    @param flag
*/
- (void)setWillLoadImagesAutomatically: (BOOL)flag;

/*!
    @method willLoadImagesAutomatically:
    @param flag
*/
- (BOOL)willLoadImagesAutomatically;

@end
