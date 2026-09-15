//
//  MTLLinkedFunctions.h
//  Metal
//
//  Copyright © 2020 Apple, Inc. All rights reserved.
//

#import <Metal/MTLDefines.h>


#import <Foundation/Foundation.h>


@protocol MTLFunction;

/*!
 @class MTLLinkedFunctions
 @abstract A class to set functions to be linked.
 @discussion All functions set on this object must have unique names.
 */
MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0))
@interface MTLLinkedFunctions : NSObject <NSCopying>

/*!
 @method linkedFunctions
 @abstract Create an autoreleased MTLLinkedFunctions object.
 */
+ (nonnull MTLLinkedFunctions *)linkedFunctions;

/*!
* @property functions
* @abstract The array of functions to be AIR linked.
*/
@property (readwrite, nonatomic, copy, nullable) NSArray<id<MTLFunction>> *functions;

/*!
* @property binaryFunctions
* @abstract The array of functions compiled to binary to be linked.
*/
@property (readwrite, nonatomic, copy, nullable) NSArray<id<MTLFunction>> *binaryFunctions API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

/*!
* @property groups
* @abstract Groups of functions, grouped to match callsites in the shader code.
*/
@property (readwrite, nonatomic, copy, nullable) NSDictionary<NSString*, NSArray<id<MTLFunction>>*> *groups;

/*!
@property privateFunctions
@abstract The array of functions to be AIR linked.
@discussion These functions are not exported by the pipeline state as MTLFunctionHandle objects.
 Function pointer support is not required to link private functions.
*/
@property (readwrite, nonatomic, copy, nullable) NSArray<id<MTLFunction>> *privateFunctions API_AVAILABLE(macos(12.0), ios(15.0));

@end

