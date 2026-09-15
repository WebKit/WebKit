//
//  MTLResourceStateCommandEncoder.h
//  Metal
//
//  Copyright © 2018 Apple, Inc. All rights reserved.


#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLCommandEncoder.h>
#import <Metal/MTLTexture.h>
#import <Metal/MTLFence.h>
#import <Metal/MTLResourceStatePass.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 @enum MTLSparseTextureMappingMode
 @abstract Type of mapping operation for sparse texture
 */
typedef NS_ENUM(NSUInteger, MTLSparseTextureMappingMode)
{
    MTLSparseTextureMappingModeMap   = 0,
    MTLSparseTextureMappingModeUnmap = 1,
} API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));

/*!
 @enum MTLMapIndirectArguments
 @abstract Structure describing indirect mapping region. This structure is used to populate a buffer for the method  'MTLResourceStateCommandEncoder updateTextureMapping:indirectBuffer:indirectBufferOffset:'
 @discussion The correct data format for the buffer used in 'MTLResourceStateCommandEncoder updateTextureMapping:indirectBuffer:indirectBufferOffset: is the following:
 
 struct MTLMapIndirectBufferFormat{
     uint32_t numMappings;
     MTLMapIndirectArguments mappings[numMappings];
 }
 */
typedef struct {
    uint32_t regionOriginX;
    uint32_t regionOriginY;
    uint32_t regionOriginZ;
    uint32_t regionSizeWidth;
    uint32_t regionSizeHeight;
    uint32_t regionSizeDepth;
    uint32_t mipMapLevel;
    uint32_t sliceId;
} MTLMapIndirectArguments;

API_AVAILABLE(macos(10.15), ios(13.0), tvos(16.0))
@protocol MTLResourceStateCommandEncoder <MTLCommandEncoder>


/*!
 @method updateTextureMappings:regions:mipLevels:slices:numRegions:mode:
 @abstract Updates multiple regions within a sparse texture.
 */
-(void) updateTextureMappings:(id<MTLTexture>) texture
                         mode:(const MTLSparseTextureMappingMode)mode
                      regions:(const MTLRegion[_Nonnull])regions
                    mipLevels:(const NSUInteger[_Nonnull])mipLevels
                       slices:(const NSUInteger[_Nonnull])slices
                   numRegions:(NSUInteger)numRegions API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));

/*!
 @method updateTextureMapping:region:mipLevel:slice:mode:
 @abstract Updates mapping for given sparse texture
 */
-(void) updateTextureMapping:(id<MTLTexture>) texture
                        mode:(const MTLSparseTextureMappingMode)mode
                      region:(const MTLRegion)region
                    mipLevel:(const NSUInteger)mipLevel
                       slice:(const NSUInteger)slice API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));

/*!
 @method updateTextureMapping:indirectBuffer:indirectBufferOffset:
 @abstract Updates mapping for given sparse texture. Updates are driven via a MTLBuffer with the structure format defined by MTLMapIndirectBufferFormat.
 
  struct MTLMapIndirectBufferFormat{
      uint32_t numMappings;
      MTLMapIndirectArguments mappings[numMappings]; 
  }
 */
-(void) updateTextureMapping:(id<MTLTexture>) texture
                        mode:(const MTLSparseTextureMappingMode)mode
              indirectBuffer:(id<MTLBuffer>)indirectBuffer
        indirectBufferOffset:(NSUInteger)indirectBufferOffset API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));

/*!
 @method updateFence:
 @abstract Update the fence to capture all GPU work so far enqueued by this encoder.
 @discussion The fence is updated at kernel submission to maintain global order and prevent deadlock.
 Drivers may delay fence updates until the end of the encoder. Drivers may also wait on fences at the beginning of an encoder. It is therefore illegal to wait on a fence after it has been updated in the same encoder.
 */
- (void)updateFence:(id <MTLFence>)fence API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));

/*!
 @method waitForFence:
 @abstract Prevent further GPU work until the fence is reached.
 @discussion The fence is evaluated at kernel submission to maintain global order and prevent deadlock.
 Drivers may delay fence updates until the end of the encoder. Drivers may also wait on fences at the beginning of an encoder. It is therefore illegal to wait on a fence after it has been updated in the same encoder.
 */
- (void)waitForFence:(id <MTLFence>)fence API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0));


/*!
 @method moveTextureMappingsFromTexture:sourceSlice:sourceLevel:sourceOrigin:sourceSize:toTexture:destinationSlice:destinationLevel:destinationOrigin:
 @abstract Move sparse page mappings from one sparse texture to another from the same heap.
 @discussion The tile mapping is moved from the source texture only if the destination texture tile is unmapped. The textures must also have matching a texture format,
 texture type, sample count, usage and resource options.
 */
-(void) moveTextureMappingsFromTexture:(id<MTLTexture>) sourceTexture
                          sourceSlice:(NSUInteger)sourceSlice
                          sourceLevel:(NSUInteger)sourceLevel
                         sourceOrigin:(MTLOrigin)sourceOrigin
                           sourceSize:(MTLSize)sourceSize
                            toTexture:(id<MTLTexture>) destinationTexture
                     destinationSlice:(NSUInteger)destinationSlice
                     destinationLevel:(NSUInteger)destinationLevel
                     destinationOrigin:(MTLOrigin)destinationOrigin API_AVAILABLE(macos(13.0), ios(16.0));



@end
NS_ASSUME_NONNULL_END

