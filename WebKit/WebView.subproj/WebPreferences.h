/*	
    WebPreferences.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
    
    Public header file.
*/

#import <Foundation/Foundation.h>

@class WebPreferencesPrivate;

extern NSString *WebPreferencesChangedNotification;

/*!
    @class WebPreferences
*/
@interface WebPreferences: NSObject <NSCoding>
{
@private
    WebPreferencesPrivate *_private;
}

/*!
    @method standardPreferences
*/
+ (WebPreferences *)standardPreferences;

/*!
    @method initWithIdentifier:
    @param anIdentifier A string used to identify the WebPreferences.
    @discussion WebViews can share instances of WebPreferences by using an instance of WebPreferences with
    the same identifier.  Typically, instance are not created directly.  Instead you set the preferences
    identifier on a WebView.  The identifier is used as a prefix that is added to the user defaults keys
    for the WebPreferences.
    @result Returns a new instance of WebPreferences or a previously allocated instance with the same identifier.
*/
- (id)initWithIdentifier:(NSString *)anIdentifier;

/*!
    @method identifier
    @result Returns the identifier for this WebPreferences.
*/
- (NSString *)identifier;

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
    @method minimumLogicalFontSize
*/
- (int)minimumLogicalFontSize;

/*!
    @method setMinimumLogicalFontSize:
    @param size
*/
- (void)setMinimumLogicalFontSize:(int)size;

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

/*!
    @method setAutosaves:
    @param flag 
    @discussion If autosave preferences is YES the settings represented by
    WebPreferences will be stored in the user defaults database.
*/
- (void)setAutosaves:(BOOL)flag;

/*!
    @method autosaves
    @result The value of the autosave preferences flag.
*/
- (BOOL)autosaves;

/*!
    @method setShouldPrintBackgrounds:
    @param flag
*/
- (void)setShouldPrintBackgrounds:(BOOL)flag;

/*!
    @method shouldPrintBackgrounds
    @result The value of the shouldPrintBackgrounds preferences flag
*/
- (BOOL)shouldPrintBackgrounds;

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
