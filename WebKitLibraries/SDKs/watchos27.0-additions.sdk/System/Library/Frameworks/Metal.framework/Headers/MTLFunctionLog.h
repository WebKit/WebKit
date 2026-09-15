//
//  MTLFunctionLog.h
//  Metal
//
//  Copyright © 2020 Apple, Inc. All rights reserved.
//

#import <Metal/MTLTypes.h>

@protocol MTLFunction;


API_AVAILABLE(macos(11.0), ios(14.0))
typedef NS_ENUM(NSUInteger, MTLFunctionLogType) {
    MTLFunctionLogTypeValidation   = 0, /// validation
};

API_AVAILABLE(macos(11.0), ios(14.0))
@protocol MTLLogContainer <NSObject, NSFastEnumeration>
@end

API_AVAILABLE(macos(11.0), ios(14.0))
@protocol MTLFunctionLogDebugLocation <NSObject>
@property (readonly, nullable, nonatomic) NSString* functionName;  // faulting function
@property (readonly, nullable, nonatomic) NSURL* URL;              // source location
@property (readonly, nonatomic) NSUInteger line;                   // line number
@property (readonly, nonatomic) NSUInteger column;                 // column in line
@end

API_AVAILABLE(macos(11.0), ios(14.0))
@protocol MTLFunctionLog <NSObject>
@property (readonly, nonatomic) MTLFunctionLogType type;
@property (readonly, nullable, nonatomic) NSString* encoderLabel;
@property (readonly, nullable, nonatomic) id<MTLFunction> function;
@property (readonly, nullable, nonatomic) id<MTLFunctionLogDebugLocation> debugLocation;
@end

