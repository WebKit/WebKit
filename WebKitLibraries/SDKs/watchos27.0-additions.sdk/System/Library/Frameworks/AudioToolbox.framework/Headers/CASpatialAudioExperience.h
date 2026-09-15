#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/CASpatialAudioExperience.h>)
/*!
    @file        CASpatialAudioExperience.h
    @framework   AudioToolbox.framework
    @copyright   (c) 2025 Apple, Inc. All rights reserved.
    @abstract    API to express spatial experiences for audio playback APIs
*/

#ifndef CASpatialAudioExperience_h
#define CASpatialAudioExperience_h

#ifdef __OBJC2__

#import <Foundation/Foundation.h>
#import <os/availability.h>
#import <TargetConditionals.h>

NS_ASSUME_NONNULL_BEGIN

#pragma mark - Sound Stage Sizes

/// Configure the distribution of audio channels in 3D space.
///
/// The Objective-C version of the ``SpatialAudioExperiences.SoundStageSize`` Swift type.
API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios) API_UNAVAILABLE(watchos, tvos, macos)
typedef NS_ENUM(NSInteger, CASoundStageSize) {
    
    /// A system-defined sound stage size.
    CASoundStageSizeAutomatic,
    
    /// Places all of an audio stream's channels near the layout's front.
    CASoundStageSizeSmall,
    
    /// Pulls an audio stream's channels closer to the channel layout's front.
    CASoundStageSizeMedium,
    
    /// Spreads an audio stream's channels around the user according to the
    /// coordinates described in its channel layout.
    CASoundStageSizeLarge,
} NS_REFINED_FOR_SWIFT;

#pragma mark - Anchoring Strategies

/// The center of a head-tracked spatial experience.
///
/// The Objective-C version of the ``SpatialAudioExperiences.AnchoringStrategy`` Swift type.
NS_REFINED_FOR_SWIFT
API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios) API_UNAVAILABLE(watchos, tvos, macos)
@interface CAAnchoringStrategy : NSObject <NSSecureCoding, NSCopying>

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

/// A system-defined anchoring strategy.
NS_REFINED_FOR_SWIFT
API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios) API_UNAVAILABLE(watchos, tvos, macos)
@interface CAAutomaticAnchoringStrategy : CAAnchoringStrategy

- (instancetype)init;
+ (instancetype)new;

@end

/// Anchor to the front of the user's space.
NS_REFINED_FOR_SWIFT
API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios) API_UNAVAILABLE(watchos, tvos, macos)
@interface CAFrontAnchoringStrategy : CAAnchoringStrategy

- (instancetype)init;
+ (instancetype)new;

@end

/// Anchor to the visual center of a particular UIScene.
NS_REFINED_FOR_SWIFT
API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios) API_UNAVAILABLE(watchos, tvos, macos)
@interface CASceneAnchoringStrategy : CAAnchoringStrategy

- (instancetype)initWithSceneIdentifier:(NSString*)sceneIdentifier;
+ (instancetype)new NS_UNAVAILABLE;

@property (readonly) NSString *sceneIdentifier;

@end

#pragma mark - Spatial Experiences

/// Configure an audio stream for spatial computing.
///
/// The Objective-C version of the ``SpatialAudioExperience`` Swift type.
NS_REFINED_FOR_SWIFT
API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios) API_UNAVAILABLE(watchos, tvos, macos)
@interface CASpatialAudioExperience : NSObject <NSSecureCoding, NSCopying>

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

/// A spatial audio experience determined by the system.
///
/// The Objective-C version of the ``AutomaticSpatialAudio`` Swift type.
NS_REFINED_FOR_SWIFT
API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios) API_UNAVAILABLE(watchos, tvos, macos)
@interface CAAutomaticSpatialAudio : CASpatialAudioExperience

- (instancetype)init;
+ (instancetype)new;

@end

/// An experience in which the system does not apply spatial processing to the audio stream.
///
/// The Objective-C version of the ``BypassedSpatialAudio`` Swift type.
NS_REFINED_FOR_SWIFT
API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios) API_UNAVAILABLE(watchos, tvos, macos)
@interface CABypassedSpatialAudio : CASpatialAudioExperience

- (instancetype)init;
+ (instancetype)new;

@end

/// A spatial experience that does not take user motion into account.
///
/// The Objective-C version of the ``FixedSpatialAudio`` Swift type.
NS_REFINED_FOR_SWIFT
API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios) API_UNAVAILABLE(watchos, tvos, macos)
@interface CAFixedSpatialAudio : CASpatialAudioExperience

- (instancetype)initWithSoundStageSize:(CASoundStageSize)soundStageSize;
+ (instancetype)new NS_UNAVAILABLE;

/// The experience's sound stage size.
@property (readonly) CASoundStageSize soundStageSize;

@end

/// A spatial experience that takes user motion into account.
///
/// The Objective-C version of the ``HeadTrackedSpatialAudio`` Swift type.
NS_REFINED_FOR_SWIFT
API_AVAILABLE(visionos(26.0)) API_UNAVAILABLE(ios) API_UNAVAILABLE(watchos, tvos, macos)
@interface CAHeadTrackedSpatialAudio : CASpatialAudioExperience

- (instancetype)initWithSoundStageSize:(CASoundStageSize)soundStageSize
                     anchoringStrategy:(CAAnchoringStrategy*)anchoringStrategy;
+ (instancetype)new NS_UNAVAILABLE;

/// The experience's sound stage size.
@property (readonly) CASoundStageSize soundStageSize;

/// The experience's anchoring strategy.
@property (readonly) CAAnchoringStrategy *anchoringStrategy;

@end

NS_ASSUME_NONNULL_END

#endif /* __OBJC2__ */
#endif /* CASpatialAudioExperience_h */

#else
#include <AudioToolboxCore/CASpatialAudioExperience.h>
#endif
