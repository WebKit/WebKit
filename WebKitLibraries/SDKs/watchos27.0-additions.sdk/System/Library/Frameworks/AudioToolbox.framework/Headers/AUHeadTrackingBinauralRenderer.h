#if (defined(__USE_PUBLIC_HEADERS__) && __USE_PUBLIC_HEADERS__) || (defined(USE_AUDIOTOOLBOX_PUBLIC_HEADERS) && USE_AUDIOTOOLBOX_PUBLIC_HEADERS) || !__has_include(<AudioToolboxCore/AUHeadTrackingBinauralRenderer.h>)
#ifndef AudioToolbox_AUHeadTrackingBinauralRenderer_h
#define AudioToolbox_AUHeadTrackingBinauralRenderer_h
#if __OBJC2__

#import <Foundation/Foundation.h>
#import <AudioToolbox/AUAudioUnit.h>

NS_ASSUME_NONNULL_BEGIN

/// A subclass of AUAudioUnit specifically for 3rd party spatial Audio Units.
///
/// This class adds spatial-audio-specific head tracking properties beyond the standard
/// AUAudioUnit interface.
///
/// When the user selects matching Bluetooth headphones for the current audio route and
/// the system has a 3rd Party Spatial Audio Extension installed that supports them, the
/// system automatically loads this AUAudioUnit subclass into the audio signal chain while
/// head tracking remains active on the host device.
///
/// Only the audio mix engine may use AUHeadTrackingBinauralRenderer Audio Units to
/// provide on demand Bluetooth head tracking support. See the 3rd Party Spatial Audio
/// Extension programming guide for more information.
API_AVAILABLE(ios(27.0)) API_UNAVAILABLE(macos, macCatalyst, tvos, watchos, visionos)
@interface AUHeadTrackingBinauralRenderer : AUAudioUnit

/// Indicates whether the host currently has enabled head tracking for this spatial Audio Unit.
///
/// The host sets this property when the user enables or disables head tracking.
/// When YES, the spatial Audio Unit should use head tracking data to adjust
/// spatialization. When NO, the host has disabled head tracking.
///
/// The Audio Unit should monitor this property to detect changes in head tracking state.
///
/// This property supports Key-Value Observing (KVO).
@property (nonatomic, readonly, getter=isHeadTracking) BOOL headTracking;

/// Indicates whether the host is bypassing the renderer due to poor performance.
///
/// The host may set this property when the Audio Unit exhibits problematic behavior
/// such as excessive CPU usage that impacts real-time performance or non-compliance with
/// API requirements.
///
/// When YES, the host bypasses the spatial Audio Unit in the audio signal chain and the
/// Audio Unit does not render audio. When NO, the Audio Unit receives input and must
/// render audio when it is matched a Bluetooth headphone device.
///
/// The Audio Unit should monitor this property to detect when the host disables the Audio Unit.
///
/// This property supports Key-Value Observing (KVO).
@property (nonatomic, readonly, getter=isDisabled) BOOL disabled;

/// The Unique Identifier (UID) of the Bluetooth headphone device providing IMU sensor data for head tracking.
///
/// The UID identifies which Bluetooth headphone device corresponds to this instance of the
/// Audio Unit. The host sets this property when it matches a device with this instance of
/// the Audio Unit.
///
/// The Audio Unit should monitor this property to detect when the host matches the Audio Unit with
/// Bluetooth headphones.
///
/// This property supports Key-Value Observing (KVO).
@property (nonatomic, readonly, nullable) NSString* deviceUID;

@end

NS_ASSUME_NONNULL_END

#endif // __OBJC2__
#endif // AudioToolbox_AUHeadTrackingBinauralRenderer_h
#else
#include <AudioToolboxCore/AUHeadTrackingBinauralRenderer.h>
#endif
