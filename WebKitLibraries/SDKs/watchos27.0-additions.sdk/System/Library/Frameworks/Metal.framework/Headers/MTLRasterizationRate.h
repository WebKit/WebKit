//
//  MTLRasterizationRate.h
//  Metal
//
//  Copyright (c) 2018 Apple, Inc. All rights reserved.
//
//  Support for variable rasterization rate

#import <Metal/MTLDevice.h>
NS_ASSUME_NONNULL_BEGIN

/*!
 @interface MTLRasterizationRateSampleArray
 @abstract A helper object for convient access to samples stored in an array.
 */
MTL_EXPORT API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4), tvos(16.0))
@interface MTLRasterizationRateSampleArray : NSObject

/*!
 @method objectAtIndexedSubscript:
 @abstract Retrieves the sample value at the specified index.
 @return NSNumber instance describing the value of the sample at the specified index, or 0 if the index is out of range.
 */
- (NSNumber*)objectAtIndexedSubscript:(NSUInteger)index;

/*!
 @method setObject:atIndexedSubscript:
 @abstract Stores a sample value at the specified index.
 @discussion The value will be converted to a single precision floating point value.
 */
- (void)setObject:(NSNumber*)value atIndexedSubscript:(NSUInteger)index;
@end

/*!
 @interface MTLRasterizationRateLayerDescriptor
 @abstract Describes the minimum rasterization rate screen space using two piecewise linear functions.
 @discussion The two piecewise linear function (PLF) describe the desired rasterization quality on the horizontal and vertical axis separately.
 Each quality sample in the PLF is stored in an array as single precision floating point value between 0 (lowest quality) and 1 (highest quality).
 The first sample in the array describes the quality at the top (vertical) or left (horizontal) edge of screen space.
 The last sample in the array describes the quality at the bottom (vertical) or right (horizontal) edge of screen space.
 All other samples are spaced equidistant in screen space.
 MTLRasterizationRateLayerDescriptor instances will be stored inside a MTLRasterizationRateMapDescriptor which in turn is compiled by MTLDevice into a MTLRasterizationRateMap.
 Because MTLDevice may not support the requested granularity, the provided samples may be rounded up (towards higher quality) during compilation.
 */
MTL_EXPORT API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4), tvos(16.0))
@interface MTLRasterizationRateLayerDescriptor : NSObject <NSCopying>

/*!
 @method init
 @abstract Do not use, instead use initWithNumSamples:
 */
- (instancetype)init API_UNAVAILABLE(macos, ios);

/*!
 @method initWithSampleCount:
 @abstract Initialize a descriptor for a layer with the given number of quality samples on the horizontal and vertical axis.
 @param sampleCount The width and height components are the number of samples on the horizontal and vertical axis respectively. The depth component is ignored.
 @discussion All values are initialized to zero.
 */
- (instancetype)initWithSampleCount:(MTLSize)sampleCount NS_DESIGNATED_INITIALIZER;

/*!
 @method initWithSampleCount:horizontal:vertical:
 @abstract Initialize a descriptor for a layer with the given number of quality samples on the horizontal and vertical axis.
 @param sampleCount The width and height components are the number of samples on the horizontal and vertical axis respectively. The depth component is ignored.
 @param horizontal The initial sample values on the horizontal axis. Must point to an array of sampleCount.width elements, of which the values will be copied into the MTLRasterizationRateLayerDescriptor.
 @param vertical The initial sample values on the vertical axis. Must point to an array of sampleCount.height elements, of which the values will be copied into the MTLRasterizationRateLayerDescriptor.
 @discussion Use initWithSampleCount: to initialize with zeroes instead.
 */
- (instancetype)initWithSampleCount:(MTLSize)sampleCount horizontal:(const float *)horizontal vertical:(const float *)vertical;

/*!
 @property sampleCount
 @return The number of quality samples that this descriptor uses to describe its current function, for the horizontal and vertical axis. The depth component of the returned MTLSize is always 0.
 */
@property (readonly, nonatomic) MTLSize sampleCount API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4), tvos(16.0));

/*!
 @property maxSampleCount
 @return The maximum number of quality samples that this descriptor can use to describe its function, for the horizontal and vertical axis, this is the sampleCount that the descriptor was initialized with. The depth component of the returned MTLSize is always 0.
 */
@property (readonly, nonatomic) MTLSize maxSampleCount API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @property horizontalSampleStorage
 @abstract Provide direct access to the quality samples stored in the descriptor.
 @return Pointer to the (mutable) storage array for samples on the horizontal axis.
 @discussion The returned pointer points to the first element of an array of sampleCount.width elements.
 */
@property (readonly, nonatomic) float* horizontalSampleStorage;

/*!
 @property verticalSampleStorage
 @abstract Provide direct access to the quality samples stored in the descriptor.
 @return Pointer to the (mutable) storage array for samples on the vertical axis.
 @discussion The returned pointer points to the first element of an array of sampleCount.height elements.
 */
@property (readonly, nonatomic) float* verticalSampleStorage;

/*!
 @property horizontal
 @abstract Provide convenient bounds-checked access to the quality samples stored in the descriptor.
 @return Returns a syntactic sugar helper to get or set sample values on the horizontal axis.
 */
@property (readonly, nonatomic) MTLRasterizationRateSampleArray* horizontal;

/*!
 @property vertical
 @abstract Provide convenient bounds-checked access to the quality samples stored in the descriptor.
 @return Returns a syntactic sugar helper to get or set sample values on the vertical axis.
 */
@property (readonly, nonatomic) MTLRasterizationRateSampleArray* vertical;
@end

@interface MTLRasterizationRateLayerDescriptor()
/*!
 @property sampleCount
 @abstract This declaration adds a setter to the previously (prior to macOS 12.0 and iOS 15.0) read-only property.
 @discussion Setting a value (must be <= maxSampleCount) to allow MTLRasterizationRateLayerDescriptor to be re-used with a different number of samples without having to be recreated.
 */
@property (readwrite, nonatomic) MTLSize sampleCount API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));
@end

/*!
 @interface MTLRasterizationRateLayerArray
 @abstract Mutable array of MTLRasterizationRateLayerDescriptor
 */
MTL_EXPORT API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4), tvos(16.0))
@interface MTLRasterizationRateLayerArray : NSObject

/*!
 @method objectAtIndexedSubscript:
 @return The MTLRasterizationRateLayerDescriptor instance for the given layerIndex, or nil if no instance hasn't been set for this index.
 @discussion Use setObject:atIndexedSubscript: to set the layer
 */
- (MTLRasterizationRateLayerDescriptor* _Nullable)objectAtIndexedSubscript:(NSUInteger)layerIndex;

/*!
 @method setObject:atIndexedSubscript:
 @abstract Sets the MTLRasterizationRateLayerDescriptor instance for the given layerIndex.
 @discussion The previous instance at this index will be overwritten.
 */
- (void)setObject:(MTLRasterizationRateLayerDescriptor* _Nullable)layer atIndexedSubscript:(NSUInteger)layerIndex;

@end

/*!
 @interface MTLRasterizationRateMapDescriptor
 @abstract Describes a MTLRasterizationRateMap containing an arbitrary number of MTLRasterizationRateLayerDescriptor instances.
 @discussion An MTLRasterizationRateMapDescriptor is compiled into an MTLRasterizationRateMap using MTLDevice.
 */
MTL_EXPORT API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4), tvos(16.0))
@interface MTLRasterizationRateMapDescriptor : NSObject <NSCopying>

/*!
 @method rasterizationRateMapDescriptorWithScreenSize:
 @abstract Convenience descriptor creation function without layers
 @param screenSize The dimensions, in screen space pixels, of the region where variable rasterization is applied. The depth component of MTLSize is ignored.
 @return A descriptor containing no layers. Add or remove layers using setObject:atIndexedSubscript:.
 */
+ (MTLRasterizationRateMapDescriptor*)rasterizationRateMapDescriptorWithScreenSize:(MTLSize)screenSize;

/*!
 @method rasterizationRateMapDescriptorWithScreenSize:layer:
 @abstract Convenience descriptor creation function for a single layer.
 @param screenSize The dimensions, in screen space pixels, of the region where variable rasterization is applied. The depth component of MTLSize is ignored.
 @param layer The single layer describing how the rasterization rate varies in screen space
 @return A descriptor containing a single layer. Add or remove layers using setObject:atIndexedSubscript:.
 */
+ (MTLRasterizationRateMapDescriptor*)rasterizationRateMapDescriptorWithScreenSize:(MTLSize)screenSize layer:(MTLRasterizationRateLayerDescriptor * _Nonnull)layer;

/*!
 @method rasterizationRateMapDescriptorWithScreenSize:layerCount:layers:
 @abstract Convenience descriptor creation function for an arbitrary amount of layers stored in a C-array.
 @param screenSize The dimensions, in screen space pixels, of the region where variable rasterization is applied. The depth component of MTLSize is ignored.
 @param layerCount The number of layers in the descriptor.
 @param layers An array of pointers to layer descriptors. The array must contain layerCount non-null pointers to MTLRasterizationRateLayerDescriptor instances.
 @return A descriptor containing all the specified layers. Add or remove layers using setObject:atIndexedSubscript:.
 @discussion The function copies the array of pointers internally, the caller need not keep the array alive after creating the descriptor.
 */
+ (MTLRasterizationRateMapDescriptor*)rasterizationRateMapDescriptorWithScreenSize:(MTLSize)screenSize layerCount:(NSUInteger)layerCount layers:(MTLRasterizationRateLayerDescriptor * const  _Nonnull * _Nonnull)layers;

/*!
 @method layerAtIndex:
 @return The MTLRasterizationRateLayerDescriptor instance for the given layerIndex, or nil if no instance hasn't been set for this index.
 @discussion Use setLayer:atIndex: to add or set the layer.
 Identical to "layers[layerIndex]".
 */
- (MTLRasterizationRateLayerDescriptor* _Nullable)layerAtIndex:(NSUInteger)layerIndex;

/*!
 @method setLayer:atIndex:
 @abstract Sets the MTLRasterizationRateLayerDescriptor instance for the given layerIndex.
 @discussion The previous instance at the index, if any, will be overwritten.
 Set nil to an index to remove the layer at that index from the descriptor.
 Identical to "layers[layerIndex] = layer".
 */
- (void)setLayer:(MTLRasterizationRateLayerDescriptor* _Nullable)layer atIndex:(NSUInteger)layerIndex;

/*!
 @property layers
 @return A modifiable array of layers
 @discussion Accesses the layers currently stored in the descriptor.
 Syntactic sugar around "layerAtIndex:" and "setLayer:atIndex:"
 */
@property (nonatomic, readonly) MTLRasterizationRateLayerArray* layers;

/*!
 @property screenSize
 @return The dimensions, in screen space pixels, of the region where variable rasterization is applied.
 @discussion The region always has its origin at [0, 0].
 The depth component of MTLSize is ignored.
 */
@property (nonatomic) MTLSize screenSize;

/*!
 @property label
 @abstract A string to help identify this object.
 @discussion The default value is nil.
 */
@property (nullable, copy, nonatomic) NSString* label;

/*!
 @property layerCount
 @return The number of subsequent non-nil layer instances stored in the descriptor, starting at index 0.
 @discussion This property is modified by setting new layer instances using setLayer:atIndex: or assigning to layers[X]
 */
@property (nonatomic, readonly) NSUInteger layerCount;

@end

/*!
 @protocol MTLRasterizationRateMap
 @abstract Compiled read-only object that determines how variable rasterization rate is applied when rendering.
 @discussion A variable rasterization rate map is compiled by MTLDevice from a MTLRasterizationRateMapDescriptor containing one or more MTLRasterizationRateLayerDescriptor.
 During compilation, the quality samples provided in the MTLRasterizationRateLayerDescriptor may be rounded up to the nearest supported value or granularity, depending on hardware support.
 However, the compilation will never round values down, so the actual rasterization will always happen at a quality level matching or exceeding the provided quality samples.
 During rasterization using the MTLRasterizationRateMap the screen space rendering is stored in a smaller area of the framebuffer, such that lower quality regions will not occupy as many texels as higher quality regions.
 The quality will never exceed 1:1 in any region of screen space.
 Because a smaller area of the framebuffer is populated, less fragment shader invocations are required to render content, and less bandwidth is consumed to store the shaded values.
 Use a rasterization rate map to reduce rendering quality in less-important or less-sampled regions of the framebuffer, such as the periphery of a VR/AR display or a far-away cascade of a shadow map.
 */
API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4), tvos(16.0)) NS_SWIFT_SENDABLE
@protocol MTLRasterizationRateMap <NSObject>

/*!
 @property device
 @return The device on which the rasterization rate map was created
 */
@property (readonly) id<MTLDevice> device;

/*!
 @property label
 @abstract A string to help identify this object.
 */
@property (nullable, readonly) NSString *label;

/*!
 @property screenSize
 @return The dimensions, in screen space pixels, of the region where variable rasterization is applied.
 @discussion The region always has its origin at [0, 0].
 The depth component of the returned MTLSize is always 0.
 */
@property (readonly) MTLSize screenSize;

/*!
 @property physicalGranularity
 @return The granularity, in physical pixels, at which variable rasterization rate varies.
 @discussion Rendering algorithms that use binning or tiling in screen space may want to determine the screen space bin size using this value.
 The depth component of the returned MTLSize is always 0.
 */
@property (readonly) MTLSize physicalGranularity;

/*!
 @property layerCount
 @return The number of different configured layers in the rasterization map.
 @discussion Different render-target layers may target different variable rasterization configurations.
 The rasterization rate layer for a primitive is selected on the [[render_target_layer_index]].
 */
@property (readonly) NSUInteger layerCount;

/*!
 @property parameterBufferSizeAndAlign
 @abstract Returns the size and alignment requirements of the parameter buffer for this rate map.
 @discussion The parameter data can be copied into a buffer with this size and alignment using copyParameterDataToBuffer:offset:
 */
@property (readonly) MTLSizeAndAlign parameterBufferSizeAndAlign;

/*!
 @method copyParameterDataToBuffer:offset:
 @abstract Copy the parameter data into the provided buffer at the provided offset.
 @discussion The buffer must have storageMode MTLStorageModeShared, and a size of at least parameterBufferSizeAndAlign.size + offset.
 The specified offset must be a multiple of parameterBufferSize.align.
 The buffer can be bound to a shader stage to map screen space to physical fragment space, or vice versa.
 */
- (void)copyParameterDataToBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset;

/*!
 @method getPhysicalSizeForLayer:
 @abstract The dimensions, in physical fragments, of the area in the render target where variable rasterization is applied
 @discussion Different configured layers may have a different rasterization rate and may have different size after rendering.
 The rasterization rate layer for a primitive is selected on the [[render_target_layer_index]].
 */
- (MTLSize)physicalSizeForLayer:(NSUInteger)layerIndex;

/*!
 @method mapScreenToPhysicalCoordinates:forLayer:
 @abstract Computes where an offset relative to the top-left of screen space, in screen space pixels, would end up in the framebuffer, in physical fragments.
 The returned value is less-or-equal the input value because the rasterization quality never exceeds 1:1 in any region.
 */
- (MTLCoordinate2D)mapScreenToPhysicalCoordinates:(MTLCoordinate2D)screenCoordinates
                                         forLayer:(NSUInteger)layerIndex;

/*!
 @method mapPhysicalToScreenCoordinates:forLayer:
 @abstract Computes where an offset relative to the top-left of the framebuffer, in physical pixels, would end up in screen space, in screen space pixels.
 The returned value is greater-or-equal the input value because the rasterization quality never exceeds 1:1 in any region.
 */
- (MTLCoordinate2D)mapPhysicalToScreenCoordinates:(MTLCoordinate2D)physicalCoordinates
                                         forLayer:(NSUInteger)layerIndex;
@end

NS_ASSUME_NONNULL_END
