//
//  MTLParallelRenderCommandEncoder.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Metal/MTLDefines.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLRenderPass.h>
#import <Metal/MTLCommandEncoder.h>

NS_ASSUME_NONNULL_BEGIN
@protocol MTLDevice;
@protocol MTLRenderCommandEncoder;

/*!
 @protocol MTLParallelRenderCommandEncoder
 @discussion The MTLParallelRenderCommandEncoder protocol is designed to allow a single render to texture operation to be efficiently (and safely) broken up across multiple threads.
 */
API_AVAILABLE(macos(10.11), ios(8.0))
@protocol MTLParallelRenderCommandEncoder <MTLCommandEncoder>

/*!
 @method renderCommandEncoder
 @abstract Return a new autoreleased object that conforms to <MTLRenderCommandEncoder> that may be used to encode on a different thread.
 */
- (nullable id <MTLRenderCommandEncoder>)renderCommandEncoder;

/*!
 @method setColorStoreAction:atIndex:
 @brief If the the store action for a given color attachment was set to MTLStoreActionUnknown when the render command encoder was created,
 setColorStoreAction:atIndex: must be used to finalize the store action before endEncoding is called.
 @param storeAction The desired store action for the given color attachment.  This may be set to any value other than MTLStoreActionUnknown.
 @param colorAttachmentIndex The index of the color attachment
*/
- (void)setColorStoreAction:(MTLStoreAction)storeAction atIndex:(NSUInteger)colorAttachmentIndex API_AVAILABLE(macos(10.12), ios(10.0));

/*!
 @method setDepthStoreAction:
 @brief If the the store action for the depth attachment was set to MTLStoreActionUnknown when the render command encoder was created,
 setDepthStoreAction: must be used to finalize the store action before endEncoding is called.
*/
- (void)setDepthStoreAction:(MTLStoreAction)storeAction API_AVAILABLE(macos(10.12), ios(10.0));

/*!
 @method setStencilStoreAction:
 @brief If the the store action for the stencil attachment was set to MTLStoreActionUnknown when the render command encoder was created,
 setStencilStoreAction: must be used to finalize the store action before endEncoding is called.
*/
- (void)setStencilStoreAction:(MTLStoreAction)storeAction API_AVAILABLE(macos(10.12), ios(10.0));

/*!
 @method setColorStoreActionOptions:atIndex:
 @brief If the the store action for a given color attachment was set to MTLStoreActionUnknown when the render command encoder was created,
 setColorStoreActionOptions:atIndex: may be used to finalize the store action options before endEncoding is called.
 @param storeActionOptions The desired store action options for the given color attachment.
 @param colorAttachmentIndex The index of the color attachment
 */
- (void)setColorStoreActionOptions:(MTLStoreActionOptions)storeActionOptions atIndex:(NSUInteger)colorAttachmentIndex API_DEPRECATED("Store action options have no effect on Apple Silicon", macos(10.13, 27.0), ios(11.0, 27.0));

/*!
 @method setDepthStoreActionOptions:
 @brief If the the store action for the depth attachment was set to MTLStoreActionUnknown when the render command encoder was created,
 setDepthStoreActionOptions: may be used to finalize the store action options before endEncoding is called.
 */
- (void)setDepthStoreActionOptions:(MTLStoreActionOptions)storeActionOptions API_DEPRECATED("Store action options have no effect on Apple Silicon", macos(10.13, 27.0), ios(11.0, 27.0));

/*!
 @method setStencilStoreActionOptions:
 @brief If the the store action for the stencil attachment was set to MTLStoreActionUnknown when the render command encoder was created,
 setStencilStoreActionOptions: may be used to finalize the store action options before endEncoding is called.
 */
- (void)setStencilStoreActionOptions:(MTLStoreActionOptions)storeActionOptions API_DEPRECATED("Store action options have no effect on Apple Silicon", macos(10.13, 27.0), ios(11.0, 27.0));

@end
NS_ASSUME_NONNULL_END
