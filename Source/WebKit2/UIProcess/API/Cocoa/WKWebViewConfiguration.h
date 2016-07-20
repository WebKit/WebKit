/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WKFoundation.h>

#if WK_API_ENABLED

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class WKPreferences;
@class WKProcessPool;
@class WKUserContentController;
@class WKWebsiteDataStore;

#if TARGET_OS_IPHONE

/*! @enum WKSelectionGranularity
 @abstract The granularity with which a selection can be created and modified interactively.
 @constant WKSelectionGranularityDynamic    Selection granularity varies automatically based on the selection.
 @constant WKSelectionGranularityCharacter  Selection endpoints can be placed at any character boundary.
 @discussion An example of how granularity may vary when WKSelectionGranularityDynamic is used is
 that when the selection is within a single block, the granularity may be single character, and when
 the selection is not confined to a single block, the selection granularity may be single block.
 */
typedef NS_ENUM(NSInteger, WKSelectionGranularity) {
    WKSelectionGranularityDynamic,
    WKSelectionGranularityCharacter,
} WK_API_AVAILABLE(ios(8.0));

/*! @enum WKDataDetectorTypes
 @abstract The type of data detected.
 @constant WKDataDetectorTypeNone No detection is performed.
 @constant WKDataDetectorTypePhoneNumber Phone numbers are detected and turned into links.
 @constant WKDataDetectorTypeLink URLs in text are detected and turned into links.
 @constant WKDataDetectorTypeAddress Addresses are detected and turned into links.
 @constant WKDataDetectorTypeCalendarEvent Dates and times that are in the future are detected and turned into links.
 @constant WKDataDetectorTypeAll All of the above data types are turned into links when detected. Choosing this value will
 automatically include any new detection type that is added.
 */
typedef NS_OPTIONS(NSUInteger, WKDataDetectorTypes) {
    WKDataDetectorTypeNone = 0,
    WKDataDetectorTypePhoneNumber = 1 << 0,
    WKDataDetectorTypeLink = 1 << 1,
    WKDataDetectorTypeAddress = 1 << 2,
    WKDataDetectorTypeCalendarEvent = 1 << 3,
    WKDataDetectorTypeTrackingNumber = 1 << 4,
    WKDataDetectorTypeFlightNumber = 1 << 5,
    WKDataDetectorTypeLookupSuggestion = 1 << 6,
    WKDataDetectorTypeAll = NSUIntegerMax,

    WKDataDetectorTypeSpotlightSuggestion WK_API_DEPRECATED_WITH_REPLACEMENT("WKDataDetectorTypeLookupSuggestion", ios(WK_IOS_TBA, WK_IOS_TBA)) = WKDataDetectorTypeLookupSuggestion,
} WK_API_AVAILABLE(ios(WK_IOS_TBA));

#else

/*! @enum WKUserInterfaceDirectionPolicy
 @abstract The policy used to determine the directionality of user interface elements inside a web view.
 @constant WKUserInterfaceDirectionPolicyContent User interface directionality follows CSS / HTML / XHTML
 specifications.
 @constant WKUserInterfaceDirectionPolicySystem User interface directionality follows the view's
 userInterfaceLayoutDirection property
 @discussion When WKUserInterfaceDirectionPolicyContent is specified, the directionality of user interface
 elements is affected by the "dir" attribute or the "direction" CSS property. When
 WKUserInterfaceDirectionPolicySystem is specified, the directionality of user interface elements is
 affected by the direction of the view.
*/
typedef NS_ENUM(NSInteger, WKUserInterfaceDirectionPolicy) {
    WKUserInterfaceDirectionPolicyContent,
    WKUserInterfaceDirectionPolicySystem,
} WK_API_AVAILABLE(macosx(WK_MAC_TBA));

#endif

/*! @enum WKAudiovisualMediaTypes
 @abstract The types of audiovisual media which will require a user gesture to begin playing.
 @constant WKAudiovisualMediaTypeNone No audiovisual media will require a user gesture to begin playing.
 @constant WKAudiovisualMediaTypeAudio Audiovisual media containing audio will require a user gesture to begin playing.
 @constant WKAudiovisualMediaTypeVideo Audiovisual media containing video will require a user gesture to begin playing.
 @constant WKAudiovisualMediaTypeAll All audiovisual media will require a user gesture to begin playing.
*/
typedef NS_OPTIONS(NSUInteger, WKAudiovisualMediaTypes) {
    WKAudiovisualMediaTypeNone = 0,
    WKAudiovisualMediaTypeAudio = 1 << 0,
    WKAudiovisualMediaTypeVideo = 1 << 1,
    WKAudiovisualMediaTypeAll = NSUIntegerMax
} WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));

/*! A WKWebViewConfiguration object is a collection of properties with
 which to initialize a web view.
 @helps Contains properties used to configure a @link WKWebView @/link.
 */
WK_CLASS_AVAILABLE(macosx(10.10), ios(8.0))
@interface WKWebViewConfiguration : NSObject <NSCoding, NSCopying>

/*! @abstract The process pool from which to obtain the view's web content
 process.
 @discussion When a web view is initialized, a new web content process
 will be created for it from the specified pool, or an existing process in
 that pool will be used.
*/
@property (nonatomic, strong) WKProcessPool *processPool;

/*! @abstract The preference settings to be used by the web view.
*/
@property (nonatomic, strong) WKPreferences *preferences;

/*! @abstract The user content controller to associate with the web view.
*/
@property (nonatomic, strong) WKUserContentController *userContentController;

/*! @abstract The website data store to be used by the web view.
 */
@property (nonatomic, strong) WKWebsiteDataStore *websiteDataStore WK_API_AVAILABLE(macosx(10.11), ios(9.0));

/*! @abstract A Boolean value indicating whether the web view suppresses
 content rendering until it is fully loaded into memory.
 @discussion The default value is NO.
 */
@property (nonatomic) BOOL suppressesIncrementalRendering;

/*! @abstract The name of the application as used in the user agent string.
*/
@property (nullable, nonatomic, copy) NSString *applicationNameForUserAgent WK_API_AVAILABLE(macosx(10.11), ios(9.0));

/*! @abstract A Boolean value indicating whether AirPlay is allowed.
 @discussion The default value is YES.
 */
@property (nonatomic) BOOL allowsAirPlayForMediaPlayback WK_API_AVAILABLE(macosx(10.11), ios(9.0));

@property (nonatomic) WKAudiovisualMediaTypes mediaTypesRequiringUserActionForPlayback WK_API_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA));

#if TARGET_OS_IPHONE
/*! @abstract A Boolean value indicating whether HTML5 videos play inline
 (YES) or use the native full-screen controller (NO).
 @discussion The default value is NO.
 */
@property (nonatomic) BOOL allowsInlineMediaPlayback;

/*! @abstract The level of granularity with which the user can interactively
 select content in the web view.
 @discussion Possible values are described in WKSelectionGranularity.
 The default value is WKSelectionGranularityDynamic.
 */
@property (nonatomic) WKSelectionGranularity selectionGranularity;

/*! @abstract A Boolean value indicating whether HTML5 videos may play
 picture-in-picture.
 @discussion The default value is YES.
 */
@property (nonatomic) BOOL allowsPictureInPictureMediaPlayback WK_API_AVAILABLE(ios(9_0));

/*! @abstract An enum value indicating the type of data detection desired.
 @discussion The default value is WKDataDetectorTypeNone.
 An example of how this property may affect the content loaded in the WKWebView is that content like
 'Visit apple.com on July 4th or call 1 800 555-5545' will be transformed to add links around 'apple.com', 'July 4th' and '1 800 555-5545'
 if the dataDetectorTypes property is set to WKDataDetectorTypePhoneNumber | WKDataDetectorTypeLink | WKDataDetectorTypeCalendarEvent.

 */
@property (nonatomic) WKDataDetectorTypes dataDetectorTypes WK_API_AVAILABLE(ios(WK_IOS_TBA));

/*! @abstract A Boolean value indicating whether the WKWebView should always allow scaling of the web page, regardless of author intent.
 @discussion This will override the user-scalable property.
 The default value is NO.
 */
@property (nonatomic) BOOL ignoresViewportScaleLimits WK_API_AVAILABLE(ios(WK_IOS_TBA));

#else

/*! @abstract The directionality of user interface elements.
 @discussion Possible values are described in WKUserInterfaceDirectionPolicy.
 The default value is WKUserInterfaceDirectionPolicyContent.
 */
@property (nonatomic) WKUserInterfaceDirectionPolicy userInterfaceDirectionPolicy WK_API_AVAILABLE(macosx(WK_MAC_TBA));

#endif

@end

@interface WKWebViewConfiguration (WKDeprecated)

#if TARGET_OS_IPHONE
@property (nonatomic) BOOL mediaPlaybackRequiresUserAction WK_API_DEPRECATED_WITH_REPLACEMENT("requiresUserActionForMediaPlayback", ios(8.0, 9.0));
@property (nonatomic) BOOL mediaPlaybackAllowsAirPlay WK_API_DEPRECATED_WITH_REPLACEMENT("allowsAirPlayForMediaPlayback", ios(8.0, 9.0));
@property (nonatomic) BOOL requiresUserActionForMediaPlayback WK_API_DEPRECATED_WITH_REPLACEMENT("mediaTypesRequiringUserActionForPlayback", ios(9.0, WK_IOS_TBA));
#endif

@end

NS_ASSUME_NONNULL_END

#endif
