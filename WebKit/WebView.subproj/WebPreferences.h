/*	
    WebPreferences.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
    
    Public header file.
*/

#import <Foundation/Foundation.h>

extern NSString *WebPreferencesChangedNotification;

/*!
    @class WebPreferences
*/
@interface WebPreferences: NSObject
{
    NSMutableDictionary *values;
}

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
    @discussion The location of the user style sheet.
*/
- (NSURL *)userStyleSheetLocation;

/*!
    @method setUserStyleSheetLocation:
    @param URL The location of the user style sheet.
*/
- (void)setUserStyleSheetLocation:(NSURL *)URL;

/*!
    @method isJavaEnabled
*/
- (BOOL)isJavaEnabled;

/*!
    @method setJavaEnabled:
    @param flag
*/
- (void)setJavaEnabled:(BOOL)flag;

/*!
    @method isJavaScriptEnabled
*/
- (BOOL)isJavaScriptEnabled;

/*!
    @method setJavaScriptEnabled:
    @param flag
*/
- (void)setJavaScriptEnabled:(BOOL)flag;

/*!
    @method JavaScriptCanOpenWindowsAutomatically
*/
- (BOOL)javaScriptCanOpenWindowsAutomatically;

/*!
    @method setJavaScriptCanOpenWindowsAutomatically:
    @param flag
*/
- (void)setJavaScriptCanOpenWindowsAutomatically:(BOOL)flag;

/*!
    @method arePlugInsEnabled
*/
- (BOOL)arePlugInsEnabled;

/*!
    @method setPlugInsEnabled:
    @param flag
*/
- (void)setPlugInsEnabled:(BOOL)flag;

/*!
    @method allowAnimatedImages
*/
- (BOOL)allowsAnimatedImages;

/*!
    @method setAllowAnimatedImages:
    @param flag
*/
- (void)setAllowsAnimatedImages:(BOOL)flag;

/*!
    @method allowAnimatedImageLooping
*/
- (BOOL)allowsAnimatedImageLooping;

/*!
    @method setAllowAnimatedImageLooping:
    @param flag
*/
- (void)setAllowsAnimatedImageLooping: (BOOL)flag;

/*!
    @method setWillLoadImagesAutomatically:
    @param flag
*/
- (void)setLoadsImagesAutomatically: (BOOL)flag;

/*!
    @method willLoadImagesAutomatically
*/
- (BOOL)loadsImagesAutomatically;

@end
