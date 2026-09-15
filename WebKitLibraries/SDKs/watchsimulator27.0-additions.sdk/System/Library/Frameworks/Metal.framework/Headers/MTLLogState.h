//
//  MTLLogState.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#import <Metal/MTLDefines.h>

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN
/*!
 * @enum MTLLogLevel
 *
 * @abstract
 * The level of the log entry.
 */

typedef NS_ENUM(NSInteger, MTLLogLevel)
{
    MTLLogLevelUndefined,
    MTLLogLevelDebug,  // A log level that captures diagnostic information.
    MTLLogLevelInfo,   // The log level that captures additional information.
    MTLLogLevelNotice, // The log level that captures notifications.
    MTLLogLevelError,  // The log level that captures errors.
    MTLLogLevelFault,  // The log level that captures fault information.
}
API_AVAILABLE(macos(15.0), ios(18.0));

API_AVAILABLE(macos(15.0), ios(18.0)) NS_SWIFT_SENDABLE
@protocol MTLLogState <NSObject>
/*!
@method addLogHandler
@abstract Add a function block to handle log message output.
In the absence of any handlers, log messages go through the default handler.
*/
- (void)addLogHandler:(void(^ NS_SWIFT_SENDABLE)(NSString* __nullable subSystem, NSString* __nullable category, MTLLogLevel logLevel, NSString* message))block;
@end

MTL_EXPORT API_AVAILABLE(macos(15.0), ios(18.0))
@interface MTLLogStateDescriptor : NSObject <NSCopying>
/*!
@abstract level indicates the minimum level of the logs that will be printed.
@discussion All the logs with level less than given level will be skipped on the GPU Side.
 */
@property (assign, readwrite) MTLLogLevel level;

/*!
 * @abstract bufferSize indicates the size of the buffer where GPU will store the logging content from shaders. Minimum value is 1KB
 */ 
@property (assign, readwrite) NSInteger bufferSize;
@end

API_AVAILABLE(macos(15.0), ios(18.0))
MTL_EXTERN NSErrorDomain const MTLLogStateErrorDomain;
/*!
 @enum 
 @abstract NSErrors raised when creating a logstate.
 */
typedef NS_ENUM(NSUInteger, MTLLogStateError) {
    MTLLogStateErrorInvalidSize      = 1,
    MTLLogStateErrorInvalid          = 2
} API_AVAILABLE(macos(15.0), ios(18.0));
NS_ASSUME_NONNULL_END
