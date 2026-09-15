//
//  MTL4PipelineDataSetSerializer.h
//  Metal
//
//  Copyright © 2024 Apple, Inc. All rights reserved.
//

#ifndef MTL4PipelineDataSetSerializer_h
#define MTL4PipelineDataSetSerializer_h

#import <Metal/MTLDefines.h>

#import <Metal/MTLDevice.h>
NS_ASSUME_NONNULL_BEGIN

/// Configuration options for pipeline dataset serializer objects.
///
/// Use these options to enable different functionality in instances of ``MTL4PipelineDataSetSerializer``.
///
/// You can combine these values via a logical `OR` and set it to ``MTL4PipelineDataSetSerializerDescriptor/configuration``
/// to specify desired level of serialization support for instances of ``MTL4PipelineDataSetSerializer``.
typedef NS_OPTIONS(NSUInteger, MTL4PipelineDataSetSerializerConfiguration) {
    /// Enables serializing pipeline scripts.
    ///
    /// Set this mask to use ``MTL4PipelineDataSetSerializer.serializeAsPipelinesScriptWithError``.
    ///
    /// This for the default behavior.
    MTL4PipelineDataSetSerializerConfigurationCaptureDescriptors = (1 << 0),
    
    /// Enables serializing pipeline binary functions.
    ///
    /// Set this mask to use ``MTL4PipelineDataSetSerializer.serializeAsArchiveAndFlush(toURL:error:)``.
    MTL4PipelineDataSetSerializerConfigurationCaptureBinaries = (1 << 1),
} API_AVAILABLE(macos(26.0), ios(26.0));

/// Groups together properties to create a pipeline data set serializer.
MTL_EXPORT
API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4PipelineDataSetSerializerDescriptor : NSObject <NSCopying>

/// Specifies the configuration of the serialization process.
///
/// The configuration of the serialization process determines the mechanisms you use to serialize pipeline data sets.
///
/// When this configuration contains ``MTL4PipelineDataSetSerializerConfigurationCaptureDescriptors``, use
/// ``serializeAsPipelinesScriptWithError:`` to serialize pipeline scripts.
///
/// If this option contains ``MTL4PipelineDataSetSerializerConfigurationCaptureBinaries``, the serializer can additionally
/// serialize to a binary archive by calling ``serializeAsArchiveAndFlushToURL:error::``.
@property(readwrite, nonatomic) MTL4PipelineDataSetSerializerConfiguration configuration;

@end

/// A fast-addition container for collecting data during pipeline state creation.
///
/// Pipeline data serializer instances allow you to create binary archives and serialize pipeline scripts to use with
/// the offline Metal binary generator (`metal-tt`) <doc:compiling-binary-archives-from-a-custom-configuration-script.md>.
///
/// You capture and retain all relevant data for all pipelines a compiler instance creates by providing an instance of
/// this object to its ``MTL4CompilerDescriptor``.
///
/// After capturing data, you can serialize it to a binary archive to persist its contents offline by calling
/// ``serializeAsArchiveAndFlushToURL:error:``. You can also serialize a pipeline script suitable for the offline binary
/// generator (`metal-tt`) by calling ``serializeAsPipelinesScriptWithError:``
///
/// - Note: The objects ``MTL4PipelineDataSetSerializer`` contains are opaque and can't accelerate compilation for
/// compilers they are not attached to. Additionally, your program can't read data out of data set serializer instances.
API_AVAILABLE(macos(26.0), ios(26.0))
@protocol MTL4PipelineDataSetSerializer <NSObject>
/// Serializes a pipeline data set to an archive.
///
/// - Parameters:
///   - url: the URL used to serialize the serializer data set as an archive to.
///   - error: an optional parameter to store information in case of an error.
/// - Returns: a boolean indicating whether the operation was successful.
- (BOOL)serializeAsArchiveAndFlushToURL:(NSURL *)url error:(NSError **)error;

/// Serializes a serializer data set to a pipeline script as raw data.
///
/// - Parameters:
///   - error: an optional parameter to store information in case of an error.
/// - Returns: an `NSData` instance containing the pipeline script.
- (nullable NSData *)serializeAsPipelinesScriptWithError:(NSError **)error;

@end

NS_ASSUME_NONNULL_END
#endif // MTL4PipelineDataSetSerializer_h
