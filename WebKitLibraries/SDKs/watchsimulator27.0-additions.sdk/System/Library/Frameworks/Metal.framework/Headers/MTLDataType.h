//
//  MTLDataType.h
//  Metal
//
//  Created by Vatsin Suchak on 3/20/25.
//  Copyright © 2025 Apple, Inc. All rights reserved.
//

#ifndef MTLDataType_h
#define MTLDataType_h

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>

/// An enumeration of the different data types in Metal.
typedef NS_ENUM(NSUInteger, MTLDataType) {

    /// Represents no data type.
    MTLDataTypeNone = 0,

    /// Represents a struct data type.
    MTLDataTypeStruct = 1,
    /// Represents an array data type.
    MTLDataTypeArray  = 2,

    /// Represents a data type consisting of a single floating-point value.
    MTLDataTypeFloat  = 3,
    /// Represents a data type consisting of a vector of two floating-point values.
    MTLDataTypeFloat2 = 4,
    /// Represents a data type consisting of a vector of three floating-point values.
    MTLDataTypeFloat3 = 5,
    /// Represents a data type consisting of a vector of four floating-point values.
    MTLDataTypeFloat4 = 6,

    /// Represents a data type consisting of a 2x2 floating-point matrix.
    MTLDataTypeFloat2x2 = 7,
    /// Represents a data type consisting of a 2x3 floating-point matrix.
    MTLDataTypeFloat2x3 = 8,
    /// Represents a data type consisting of a 2x4 floating-point matrix.
    MTLDataTypeFloat2x4 = 9,

    /// Represents a data type consisting of a 3x2 floating-point matrix.
    MTLDataTypeFloat3x2 = 10,
    /// Represents a data type consisting of a 3x3 floating-point matrix.
    MTLDataTypeFloat3x3 = 11,
    /// Represents a data type consisting of a 3x4 floating-point matrix.
    MTLDataTypeFloat3x4 = 12,

    /// Represents a data type consisting of a 4x2 floating-point matrix.
    MTLDataTypeFloat4x2 = 13,
    /// Represents a data type consisting of a 4x3 floating-point matrix.
    MTLDataTypeFloat4x3 = 14,
    /// Represents a data type consisting of a 4x4 floating-point matrix.
    MTLDataTypeFloat4x4 = 15,

    /// Represents a data type consisting of a half-precision floating-point value.
    MTLDataTypeHalf  = 16,
    /// Represents a data type consisting of a vector of two half-precision floating-point values.
    MTLDataTypeHalf2 = 17,
    /// Represents a data type consisting of a vector of three half-precision floating-point values.
    MTLDataTypeHalf3 = 18,
    /// Represents a data type consisting of a vector of four half-precision floating-point values.
    MTLDataTypeHalf4 = 19,

    /// Represents a data type consisting of a 2x2 half-precision floating-point matrix.
    MTLDataTypeHalf2x2 = 20,
    /// Represents a data type consisting of a 2x3 half-precision floating-point matrix.
    MTLDataTypeHalf2x3 = 21,
    /// Represents a data type consisting of a 2x4 half-precision floating-point matrix.
    MTLDataTypeHalf2x4 = 22,

    /// Represents a data type consisting of a 3x2 half-precision floating-point matrix.
    MTLDataTypeHalf3x2 = 23,
    /// Represents a data type consisting of a 3x3 half-precision floating-point matrix.
    MTLDataTypeHalf3x3 = 24,
    /// Represents a data type consisting of a 3x4 half-precision floating-point matrix.
    MTLDataTypeHalf3x4 = 25,

    /// Represents a data type consisting of a 4x2 half-precision floating-point matrix.
    MTLDataTypeHalf4x2 = 26,
    /// Represents a data type consisting of a 4x3 half-precision floating-point matrix.
    MTLDataTypeHalf4x3 = 27,
    /// Represents a data type consisting of a 4x4 half-precision floating-point matrix.
    MTLDataTypeHalf4x4 = 28,

    /// Represents a data type consisting of a single signed integer value.
    MTLDataTypeInt  = 29,
    /// Represents a data type consisting of a vector of two signed integer values.
    MTLDataTypeInt2 = 30,
    /// Represents a data type consisting of a vector of three signed integer values.
    MTLDataTypeInt3 = 31,
    /// Represents a data type consisting of a vector of four signed integer values.
    MTLDataTypeInt4 = 32,

    /// Represents a data type consisting of a single unsigned integer value.
    MTLDataTypeUInt  = 33,
    /// Represents a data type consisting of a vector of two unsigned integer values.
    MTLDataTypeUInt2 = 34,
    /// Represents a data type consisting of a vector of three unsigned integer values.
    MTLDataTypeUInt3 = 35,
    /// Represents a data type consisting of a vector of four unsigned integer values.
    MTLDataTypeUInt4 = 36,

    /// Represents a data type consisting of a single 16-bit signed integer value.
    MTLDataTypeShort  = 37,
    /// Represents a data type consisting of a vector of two 16-bit signed integer values.
    MTLDataTypeShort2 = 38,
    /// Represents a data type consisting of a vector of three 16-bit signed integer values.
    MTLDataTypeShort3 = 39,
    /// Represents a data type consisting of a vector of three 16-bit signed integer values.
    MTLDataTypeShort4 = 40,

    /// Represents a data type consisting of a single 16-bit unsigned integer value.
    MTLDataTypeUShort = 41,
    /// Represents a data type consisting of a vector of two 16-bit unsigned integer values.
    MTLDataTypeUShort2 = 42,
    /// Represents a data type consisting of a vector of three 16-bit unsigned integer values.
    MTLDataTypeUShort3 = 43,
    /// Represents a data type consisting of a vector of four 16-bit unsigned integer values.
    MTLDataTypeUShort4 = 44,

    /// Represents a data type consisting of a single signed character value.
    MTLDataTypeChar  = 45,
    /// Represents a data type consisting of a vector of two signed character values.
    MTLDataTypeChar2 = 46,
    /// Represents a data type consisting of a vector of three signed character values.
    MTLDataTypeChar3 = 47,
    /// Represents a data type consisting of a vector of four signed character values.
    MTLDataTypeChar4 = 48,

    /// Represents a data type consisting of a single unsigned character value.
    MTLDataTypeUChar  = 49,
    /// Represents a data type consisting of a vector of two unsigned character values.
    MTLDataTypeUChar2 = 50,
    /// Represents a data type consisting of a vector of three unsigned character values.
    MTLDataTypeUChar3 = 51,
    /// Represents a data type consisting of a vector of four unsigned character values.
    MTLDataTypeUChar4 = 52,

    /// Represents a data type consisting of a single boolean value.
    MTLDataTypeBool  = 53,
    /// Represents a data type consisting of a vector of two boolean values.
    MTLDataTypeBool2 = 54,
    /// Represents a data type consisting of a vector of three boolean values.
    MTLDataTypeBool3 = 55,
    /// Represents a data type consisting of a vector of four boolean values.
    MTLDataTypeBool4 = 56,

    /// Represents a data type corresponding to a texture object.
    MTLDataTypeTexture API_AVAILABLE(macos(10.13), ios(11.0)) = 58,
    /// Represents a data type corresponding to a sampler state object.
    MTLDataTypeSampler API_AVAILABLE(macos(10.13), ios(11.0)) = 59,
    /// Represents a data type corresponding to a pointer.
    MTLDataTypePointer API_AVAILABLE(macos(10.13), ios(11.0)) = 60,

    /// Represents an image block data type consisting of an unsigned 8-bit red channel normalized to the [0-1] range.
    MTLDataTypeR8Unorm         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 62,
    /// Represents an image block data type consisting of an signed 8-bit red channel normalized to the [0-1] range.
    MTLDataTypeR8Snorm         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 63,
    /// Represents an image block data type consisting of an unsigned 16-bit red channel normalized to the [0-1] range.
    MTLDataTypeR16Unorm        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 64,
    /// Represents an image block data type consisting of a signed 16-bit red channel normalized to the [0-1] range.
    MTLDataTypeR16Snorm        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 65,
    /// Represents an image block data type consisting of an unsigned 8-bit red channel and a unsigned 8-bit green channel, both normalized to the [0-1] range.
    MTLDataTypeRG8Unorm        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 66,
    /// Represents an image block data type consisting of a signed 8-bit red channel and a signed 8-bit green channel, both normalized to the [0-1] range.
    MTLDataTypeRG8Snorm        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 67,
    /// Represents an image block data type consisting of an unsigned 16-bit red channel and an unsigned 16-bit green channel, both normalized to the [0-1] range.
    MTLDataTypeRG16Unorm       API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 68,
    /// Represents an image block data type consisting of a signed 16-bit red channel and a signed 16-bit green channel, both normalized to the [0-1] range.
    MTLDataTypeRG16Snorm       API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 69,
    /// Represents an image block data type consisting of four unsigned 8-bit channels normalized to the [0-1] range.
    MTLDataTypeRGBA8Unorm      API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 70,
    /// Represents an image block data type consisting of four unsigned 8-bit channels normalized to the [0-1] range and subject to gamma-correction.
    MTLDataTypeRGBA8Unorm_sRGB API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 71,
    /// Represents an image block data type consisting of four signed 8-bit channels normalized to the [0-1] range.
    MTLDataTypeRGBA8Snorm      API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 72,
    /// Represents an image block data type consisting of four unsigned 16-bit channels normalized to the [0-1] range.
    MTLDataTypeRGBA16Unorm     API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 73,
    /// Represents an image block data type consisting of four signed 16-bit channels normalized to the [0-1] range.
    MTLDataTypeRGBA16Snorm     API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 74,
    /// Represents an image block data type consisting of three unsigned 10-bit channels and one 2-bit unsigned alpha channel, all normalized to the [0-1] range.
    MTLDataTypeRGB10A2Unorm    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 75,
    /// Represents an image block data type consisting of two 11-bit floating-point channels, and one 10-bit floating-point blue channel.
    MTLDataTypeRG11B10Float    API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 76,
    /// Represents an image block data type consisting of three 9-bit floating-point channels, and one 5-bit floating-point exponent.
    MTLDataTypeRGB9E5Float     API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5)) = 77,
    /// Represents a data type corresponding to a render pipeline state object.
    MTLDataTypeRenderPipeline  API_AVAILABLE(macos(10.14), ios(13.0)) = 78,
    /// Represents a data type corresponding to a compute pipeline state object.
    MTLDataTypeComputePipeline API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0)) = 79,
    /// Represents a data type corresponding to an indirect command buffer object.
    MTLDataTypeIndirectCommandBuffer   API_AVAILABLE(macos(10.14), ios(12.0)) = 80,
    /// Represents a data type consisting of a signed long integer value.
    MTLDataTypeLong  API_AVAILABLE(macos(12.0), ios(14.0)) = 81,
    /// Represents a data type consisting of a vector of two signed long integer values.
    MTLDataTypeLong2 API_AVAILABLE(macos(12.0), ios(14.0)) = 82,
    /// Represents a data type consisting of a vector of three signed long integer values.
    MTLDataTypeLong3 API_AVAILABLE(macos(12.0), ios(14.0)) = 83,
    /// Represents a data type consisting of a vector of four signed long integer values.
    MTLDataTypeLong4 API_AVAILABLE(macos(12.0), ios(14.0)) = 84,

    /// Represents a data type consisting of an unsigned long integer value.
    MTLDataTypeULong  API_AVAILABLE(macos(12.0), ios(14.0)) = 85,
    /// Represents a data type consisting of a vector two unsigned long integer values.
    MTLDataTypeULong2 API_AVAILABLE(macos(12.0), ios(14.0)) = 86,
    /// Represents a data type consisting of a vector three unsigned long integer values.
    MTLDataTypeULong3 API_AVAILABLE(macos(12.0), ios(14.0)) = 87,
    /// Represents a data type consisting of a vector four unsigned long integer values.
    MTLDataTypeULong4 API_AVAILABLE(macos(12.0), ios(14.0)) = 88,
    /// Represents a data type corresponding to a visible function table object.
    MTLDataTypeVisibleFunctionTable API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0)) = 115,
    /// Represents a data type corresponding to an intersection function table object.
    MTLDataTypeIntersectionFunctionTable API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0)) = 116,
    /// Represents a data type corresponding to a primitive acceleration structure.
    MTLDataTypePrimitiveAccelerationStructure API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0)) = 117,
    /// Represents a data type corresponding to an instance acceleration structure.
    MTLDataTypeInstanceAccelerationStructure API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0)) = 118,
    /// Represents a data type consisting of a single BFloat value.
    MTLDataTypeBFloat  API_AVAILABLE(macos(14.0), ios(17.0)) = 121,
    /// Represents a data type consisting of a vector two BFloat values.
    MTLDataTypeBFloat2 API_AVAILABLE(macos(14.0), ios(17.0)) = 122,
    /// Represents a data type consisting of a vector three BFloat values.
    MTLDataTypeBFloat3 API_AVAILABLE(macos(14.0), ios(17.0)) = 123,
    /// Represents a data type consisting of a vector four BFloat values.
    MTLDataTypeBFloat4 API_AVAILABLE(macos(14.0), ios(17.0)) = 124,
    
    /// Represents a data type corresponding to a depth-stencil state object.
    MTLDataTypeDepthStencilState API_AVAILABLE(macos(26.0), ios(26.0)) = 139,
    
    /// Represents a data type corresponding to a machine learning tensor.
    MTLDataTypeTensor API_AVAILABLE(macos(26.0), ios(26.0)) = 140,
} API_AVAILABLE(macos(10.11), ios(8.0));


#endif /* MTLDataType_h */
