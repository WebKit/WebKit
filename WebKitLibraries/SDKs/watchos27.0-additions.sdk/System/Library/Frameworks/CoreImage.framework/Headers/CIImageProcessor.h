/* 
   CoreImage - CIImageProcessor.h

   Copyright (c) 2016 Apple Inc.
   All rights reserved. 
*/

#ifndef CIIMAGEPROCESSOR_H
#define CIIMAGEPROCESSOR_H

#ifdef __OBJC__

#import <CoreImage/CIImage.h>
#import <CoreImage/CIVector.h>

#if TARGET_OS_OSX
#import <IOSurface/IOSurface.h>
#elif !TARGET_OS_SIMULATOR
#import <IOSurface/IOSurfaceRef.h>
#endif
#import <IOSurface/IOSurfaceObjC.h>

NS_ASSUME_NONNULL_BEGIN

@protocol MTLTexture, MTLCommandBuffer;

@protocol CIImageProcessorInput;
@protocol CIImageProcessorOutput;

/// The abstract class you extend to create custom image processors that can integrate with Core Image workflows.
///
/// Unlike the ``CIKernel`` class and its other subclasses that allow you to create new image-processing effects 
/// with the Core Image Kernel Language, the `CIImageProcessorKernel` class provides direct access to the underlying 
/// bitmap image data for a step in the Core Image processing pipeline. As such, you can create subclasses of this 
/// class to integrate other image-processing technologies—such as Metal compute shaders, Metal Performance Shaders, 
/// Accelerate vImage operations, or your own CPU-based image-processing routines—with a Core Image filter chain.
/// 
/// Your custom image processing operation is invoked by your subclassed image processor kernel's 
/// ``processWithInputs:arguments:output:error:`` method. The method can accept zero, one or more `input` objects. 
/// Processors  that generate imagery (such as a noise or pattern generator) need no inputs, while kernels that 
/// composite source images together require multiple inputs. The `arguments` dictionary allows the caller to pass in 
/// additional parameter values (such as the radius of a blur) and the `output` contains the destination for your 
/// image processing code to write to.
///
/// The following code shows how you can subclass `CIImageProcessorKernel` to apply the Metal Performance Shader 
/// <doc://com.apple.documentation/documentation/metalperformanceshaders/mpsimagethresholdbinary> kernel to a ``CIImage``:
///
/// ```swift
/// class ThresholdImageProcessorKernel: CIImageProcessorKernel {
/// override class func process(with inputs: [CIImageProcessorInput]?, arguments: [String : Any]?, output: CIImageProcessorOutput) throws {
///     guard
///         let commandBuffer = output.metalCommandBuffer,
///         let input = inputs?.first,
///         let sourceTexture = input.metalTexture,
///         let destinationTexture = output.metalTexture,
///         let thresholdValue = arguments?["thresholdValue"] as? Float else {
///             return
///         }
///     
///     let threshold = MPSImageThresholdBinary(
///         device: commandBuffer.device,
///         thresholdValue: thresholdValue,        
///         maximumValue: 1.0,
///         linearGrayColorTransform: nil)
///     
///     threshold.encode(
///         commandBuffer: commandBuffer,
///         sourceTexture: sourceTexture,
///         destinationTexture: destinationTexture)
///     }
/// }
/// ```
///
/// To apply to kernel to an image, the calling side invokes the image processor's `apply(withExtent:inputs:arguments:)` 
/// method. The following code generates a new ``CIImage`` object named `result` which contains a thresholded version of 
/// the source image, `inputImage`.
///
/// ```swift
/// let result = try? ThresholdImageProcessorKernel.apply( 
///     withExtent: inputImage.extent,            
///     inputs: [inputImage],            
///     arguments: ["thresholdValue": 0.25])
/// ```
/// 
/// > Important: Core Image will concatenate kernels in a render into as fewer programs as possible, avoiding the creation
/// of intermediate buffers. However, it is unable to do this with image processor kernels. To get the best performance, 
/// you should use `CIImageProcessorKernel` objects only when your algorithms can't be expressed as a ``CIKernel``.
///
/// ## Subclassing Notes
/// 
/// The `CIImageProcessorKernel` class is abstract; to create a custom image processor, you define a subclass of this class.
/// 
/// You do not directly create instances of a custom `CIImageProcessorKernel` subclass. Image processors must not carry or 
/// use state specific to any single invocation of the processor, so all methods (and accessors for readonly properties) 
/// of an image processor kernel class are class methods.
/// 
/// Your subclass should override at least the ``processWithInputs:arguments:output:error:`` method to perform its
/// image processing.
/// 
/// If your image processor needs to work with a larger or smaller region of interest in the input image than each 
/// corresponding region of the output image (for example, a blur filter, which samples several input pixels for 
/// each output pixel), you should also override the ``roiForInput:arguments:outputRect:`` method.
/// 
/// You can also override the formatForInputAtIndex: method and outputFormat property getter to customize the input 
/// and output pixel formats for your processor (for example, as part of a multi-step workflow where you extract a 
/// single channel from an RGBA image, apply an effect to that channel only, then recombine the channels).
/// 
/// ## Using a Custom Image Processor
///
/// To apply your custom image processor class to create a ``CIImage`` object, call the 
/// ``applyWithExtent:inputs:arguments:error:`` class method. (Do not override this method.)
/// 
NS_CLASS_AVAILABLE(10_12, 10_0)
@interface CIImageProcessorKernel : NSObject

/// Override this class method to implement your Core Image Processor Kernel subclass.
/// 
/// When a `CIImage` containing your `CIImageProcessorKernel` class is rendered, your class' implementation of 
/// this method will be called as needed for that render.  The method may be called more than once if Core Image 
/// needs to tile to limit memory usage.
/// 
/// When your implementation of this class method is called, use the provided `inputs` and `arguments` objects
/// to return processed pixel data to Core Image via `output`.
///
/// > Important: this is a class method so that you cannot use or capture any state by accident.
/// All the parameters that affect the output results must be passed to
/// ``applyWithExtent:inputs:arguments:error:``.
/// 
/// - Parameters:
///    - inputs: An array of `id<CIImageProcessorInput>` that the class consumes to produce its output.
///              The `input.region` may be larger than the rect returned by ``roiForInput:arguments:outputRect:``.
///    - arguments: the arguments dictionary that was passed to ``applyWithExtent:inputs:arguments:error:``.
///    - output: The `id<CIImageProcessorOutput>` that the `CIImageProcessorKernel` must provide results to.
///    - error: Pointer to the `NSError` object into which processing errors will be written.
/// - Returns:
///    Returns YES if processing succeeded, and NO if processing failed.
/// 
+ (BOOL)processWithInputs:(nullable NSArray<id<CIImageProcessorInput>> *)inputs
                arguments:(nullable NSDictionary<NSString*,id> *)arguments
                   output:(id <CIImageProcessorOutput>)output
                    error:(NSError **)error;

/// Override this class method to implement your processor’s ROI callback.
/// 
/// This will be called one or more times per render to determine what portion
/// of the input images are needed to render a given 'outputRect' of the output.
/// This will not be called if processor has no input images.
/// 
/// The default implementation would return outputRect.
///
/// > Important: this is a class method so that you cannot use or capture any state by accident.
/// All the parameters that affect the output results must be passed to
/// ``applyWithExtent:inputs:arguments:error:``.
/// 
/// - Parameters:
///    - inputIndex: the index that tells you which processor input for which to return the ROI rectangle.
///    - arguments: the arguments dictionary that was passed to ``applyWithExtent:inputs:arguments:error:``.
///    - outputRect: the output `CGRect` that processor will be asked to output. 
/// - Returns:
///    The `CGRect` of the `inputIndex`th input that is required for the above `outputRect`
///
+ (CGRect)roiForInput:(int)inputIndex
            arguments:(nullable NSDictionary<NSString*,id> *)arguments
           outputRect:(CGRect)outputRect;

/// Override this class method to implement your processor’s tiled ROI callback.
/// 
/// This will be called one or more times per render to determine what tiles
/// of the input images are needed to render a given `outputRect` of the output.
///
/// If the processor implements this method, then when rendered;
///  * as CoreImage prepares for a render, this method will be called for each input to return an ROI tile array.
///  * as CoreImage performs the render, the method ``processWithInputs:arguments:output:error:`` will be called once for each tile.
///
/// > Important: this is a class method so that you cannot use or capture any state by accident.
/// All the parameters that affect the output results must be passed to
/// ``applyWithExtent:inputs:arguments:error:``.
/// 
/// - Parameters:
///    - inputIndex: the index that tells you which processor input for which to return the array of ROI rectangles
///    - arguments: the arguments dictionary that was passed to ``applyWithExtent:inputs:arguments:error:``.
///    - outputRect: the output `CGRect` that processor will be asked to output. 
/// - Returns:
///    An array of ``CIVector`` that specify tile regions of the `inputIndex`'th input that is required for the above `outputRect`
///    Each region tile in the array is a created by calling ``/CIVector/vectorWithCGRect:``
///    The tiles may overlap but should fully cover the area of 'input' that is needed.
///    If a processor has multiple inputs, then each input should return the same number of region tiles.
///
+ (NSArray<CIVector*>*) roiTileArrayForInput:(int)inputIndex
                                   arguments:(nullable NSDictionary<NSString*,id> *)arguments
                                  outputRect:(CGRect)outputRect NS_AVAILABLE(14_0, 17_0);

/// Override this class method if you want your any of the inputs to be in a specific pixel format.
/// 
/// The format must be one of `kCIFormatBGRA8`, `kCIFormatRGBAh`, `kCIFormatRGBAf` or `kCIFormatR8`.
/// On iOS 12 and macOS 10.14, the formats `kCIFormatRh` and `kCIFormatRf` are also supported.
/// 
/// If the requested inputFormat is `0`, then the input will be a supported format that best
/// matches the rendering context's ``/CIContext/workingFormat``.
///
/// If a processor wants data in a colorspace other than the context's working color space,
/// then call ``/CIImage/imageByColorMatchingWorkingSpaceToColorSpace:`` on the processor input.
/// If a processor wants it input as alpha-unpremultiplied RGBA data, then call
/// ``/CIImage/imageByUnpremultiplyingAlpha`` on the processor input.
///
+ (CIFormat)formatForInputAtIndex:(int)inputIndex;

/// Override this class property if you want your processor's output to be in a specific pixel format.
/// 
/// The format must be one of `kCIFormatBGRA8`, `kCIFormatRGBAh`, `kCIFormatRGBAf` or `kCIFormatR8`.
/// On iOS 12 and macOS 10.14, the formats `kCIFormatRh` and `kCIFormatRf` are also supported.
/// 
/// If the outputFormat is `0`, then the output will be a supported format that best
/// matches the rendering context's ``/CIContext/workingFormat``.
///
/// If a processor returns data in a color space other than the context working color space,
/// then call ``/CIImage/imageByColorMatchingColorSpaceToWorkingSpace:`` on the processor output.
/// If a processor returns data as alpha-unpremultiplied RGBA data, then call,
/// ``/CIImage/imageByPremultiplyingAlpha`` on the processor output.
///
@property (class, readonly) CIFormat outputFormat;

/// Override this class property if your processor's output stores 1.0 into the
/// alpha channel of all pixels within the output extent.
/// 
/// If not overridden, false is returned.
///
@property (class, readonly) bool outputIsOpaque NS_AVAILABLE(10_13, 11_0);

/// Override this class property to return false if you want your processor to be given
/// input objects that have not been synchronized for CPU access.
///
/// Generally, if your subclass uses the GPU your should override this method to return false.
/// If not overridden, true is returned.
///
@property (class, readonly) bool synchronizeInputs;


/// Call this method on your Core Image Processor Kernel subclass to create a new image of the specified extent.
///
/// The inputs and arguments will be retained so that your subclass can be called when the image is drawn.
///
/// This method will return `nil` and an error if:
/// * calling ``outputFormat`` on your subclass returns an unsupported format.
/// * calling ``formatForInputAtIndex:`` on your subclass returns an unsupported format.
/// * your subclass does not implement ``processWithInputs:arguments:output:error:``
///
/// - Parameters:
///    - extent: The bounding `CGRect` of pixels that the `CIImageProcessorKernel` can produce.
///              This method will return ``/CIImage/emptyImage`` if extent is empty.
///    - inputs: An array of ``CIImage`` objects to use as input.
///    - arguments: This dictionary contains any additional parameters that the processor needs to
///                 produce its output. The argument objects can be of any type but in order for 
///                 CoreImage to cache intermediates, they must be of the following immutable types:
///                 `NSArray`, `NSDictionary`, `NSNumber`, `NSValue`, `NSData`, `NSString`, `NSNull`,
///                 ``CIVector``, ``CIColor``, `CGImage`, `CGColorSpace`, or `MLModel`.
///    - error: Pointer to the `NSError` object into which processing errors will be written.
/// - Returns:
///     An autoreleased ``CIImage``
///
+ (nullable CIImage *)applyWithExtent:(CGRect)extent
                               inputs:(nullable NSArray<CIImage*> *)inputs
                            arguments:(nullable NSDictionary<NSString*,id> *)arguments
                                error:(NSError **)error;

@end

@interface CIImageProcessorKernel (MultipleOutputSupport)

/// Override this class method of your Core Image Processor Kernel subclass if it needs to produce multiple outputs.
///
/// This supports 0, 1, 2 or more input images and 2 or more output images.
///
/// When a `CIImage` containing your `CIImageProcessorKernel` class is rendered, your class' implementation of 
/// this method will be called as needed for that render.  The method may be called more than once if Core Image 
/// needs to tile to limit memory usage.
/// 
/// When your implementation of this class method is called, use the provided `inputs` and `arguments` objects
/// to return processed pixel data to Core Image via multiple `outputs`.
///
/// > Important: this is a class method so that you cannot use or capture any state by accident.
/// All the parameters that affect the output results must be passed to
/// ``applyWithExtent:inputs:arguments:error:``.
/// 
/// - Parameters:
///    - inputs: An array of `id<CIImageProcessorInput>` that the class consumes to produce its output.
///              The `input.region` may be larger than the rect returned by ``roiForInput:arguments:outputRect:``.
///    - arguments: the arguments dictionary that was passed to ``applyWithExtent:inputs:arguments:error:``.
///    - outputs: An array `id<CIImageProcessorOutput>` that the `CIImageProcessorKernel` must provide results to.
///    - error: Pointer to the `NSError` object into which processing errors will be written.
/// - Returns:
///    Returns YES if processing succeeded, and NO if processing failed.
/// 
+ (BOOL)processWithInputs:(nullable NSArray<id<CIImageProcessorInput>> *)inputs
                arguments:(nullable NSDictionary<NSString*,id> *)arguments
                  outputs:(NSArray<id <CIImageProcessorOutput>> *)outputs
                    error:(NSError **)error NS_AVAILABLE(26_0, 26_0);

/// Override this class method if your processor has more than one output and 
/// you want your processor's output to be in a specific supported `CIPixelFormat`.
/// 
/// The format must be one of `kCIFormatBGRA8`, `kCIFormatRGBAh`, `kCIFormatRGBAf` or `kCIFormatR8`.
/// On iOS 12 and macOS 10.14, the formats `kCIFormatRh` and `kCIFormatRf` are also supported.
/// 
/// If the outputFormat is `0`, then the output will be a supported format that best
/// matches the rendering context's ``/CIContext/workingFormat``.
///
/// - Parameters:
///    - outputIndex: the index that tells you which processor output for which to return the desired `CIPixelFormat`
///    - arguments: the arguments dictionary that was passed to ``applyWithExtent:inputs:arguments:error:``.
/// - Returns:
///    Return the desired `CIPixelFormat`
///
+ (CIFormat)outputFormatAtIndex:(int)outputIndex
                      arguments:(nullable NSDictionary<NSString*,id> *)arguments
NS_AVAILABLE(26_0, 26_0);

/// Call this method on your multiple-output Core Image Processor Kernel subclass 
/// to create an array of new image objects given the specified array of extents.
///
/// The inputs and arguments will be retained so that your subclass can be called when the image is drawn.
///
/// This method will return `nil` and an error if:
/// * calling ``outputFormatAtIndex:arguments:`` on your subclass returns an unsupported format.
/// * calling ``formatForInputAtIndex:`` on your subclass returns an unsupported format.
/// * your subclass does not implement ``processWithInputs:arguments:output:error:``
///
/// - Parameters:
///    - extents: The array of bounding rectangles that the `CIImageProcessorKernel` can produce.
///               Each rectangle in the array is an object created using ``/CIVector/vectorWithCGRect:`` 
///               This method will return `CIImage.emptyImage` if a rectangle in the array is empty.
///    - inputs: An array of ``CIImage`` objects to use as input.
///    - arguments: This dictionary contains any additional parameters that the processor needs to
///                 produce its output. The argument objects can be of any type but in order for 
///                 CoreImage to cache intermediates, they must be of the following immutable types:
///                 `NSArray`, `NSDictionary`, `NSNumber`, `NSValue`, `NSData`, `NSString`, `NSNull`,
///                 ``CIVector``, ``CIColor``, `CGImage`, `CGColorSpace`, or `MLModel`.
///    - error: Pointer to the `NSError` object into which processing errors will be written.
/// - Returns:
///     An autoreleased ``CIImage``
///
+ (nullable NSArray<CIImage*> *)applyWithExtents:(NSArray<CIVector*> *)extents
                                          inputs:(nullable NSArray<CIImage*> *)inputs
                                       arguments:(nullable NSDictionary<NSString*,id> *)arguments
                                           error:(NSError **)error NS_AVAILABLE(26_0, 26_0);

@end


@interface CIImageProcessorKernel (TiledOutputSupport)

/// Call this method on your Core Image Processor Kernel subclass to create a new image
/// based on an array of tile extents that together cover the output.
/// 
/// Each tile is a CGRect encoded as a CIVector using +[CIVector vectorWithCGRect:].
/// The overall output extent is computed as the union of all tile extents.
///
/// This method will return `nil` and an error if:
/// * calling ``outputFormat`` on your subclass returns an unsupported format.
/// * calling ``formatForInputAtIndex:`` on your subclass returns an unsupported format.
/// * your subclass does not implement ``processWithInputs:arguments:output:error:``
///
/// - Parameters:
///    - tileExtents: The array of bounding rectangles that the `CIImageProcessorKernel` can produce.
///                   Each rectangle in the array is an object created using ``/CIVector/vectorWithCGRect:`` 
///                   This method will return `CIImage.emptyImage` if the rectangles in the array 
///                   have gaps or overlaps.
///    - inputs: An array of ``CIImage`` objects to use as input.
///    - arguments: This dictionary contains any additional parameters that the processor needs to
///                 produce its output. The argument objects can be of any type but in order for 
///                 CoreImage to cache intermediates, they must be of the following immutable types:
///                 `NSArray`, `NSDictionary`, `NSNumber`, `NSValue`, `NSData`, `NSString`, `NSNull`,
///                 ``CIVector``, ``CIColor``, `CGImage`, `CGColorSpace`, or `MLModel`.
///    - error: Pointer to the `NSError` object into which processing errors will be written.
/// - Returns:
///     An autoreleased ``CIImage``
///
+ (nullable CIImage *) applyWithTiledExtent:(NSArray<CIVector*>*)tileExtents
                                     inputs:(nullable NSArray<CIImage*>*)inputs
                                  arguments:(nullable NSDictionary<NSString*,id>*)args
                                      error:(NSError **)error
    NS_SWIFT_NAME(apply(tiledExtent:inputs:arguments:))
API_AVAILABLE(macos(27.0), ios(27.0), tvos(27.0), visionos(27.0)) API_UNAVAILABLE(watchos);

@end


/// Your app does not define classes that adopt this protocol; Core Image provides an object of this type 
/// when rendering a custom image processor you create with a ``CIImageProcessorKernel`` subclass.
/// 
/// When a `CIImage` containing your `CIImageProcessorKernel` class is rendered, your
/// ``CIImageProcessorKernel/processWithInputs:arguments:output:error:`` class method will be called as 
/// needed for that render.  The method may be called more than once if Core Image needs to tile to
/// limit memory usage.
/// 
/// When your image processor class method is called, use the provided `CIImageProcessorInput` object to 
/// access the image data and supporting information to perform your custom image processing routine. 
/// For example, if you process the image using a Metal shader, use the `metalTexture` property to bind the
/// image as an input texture. Or, if you process the image using a CPU-based routine, use the `baseAddress` 
/// property to access pixel data in memory.
/// 
/// You should use the input's `region` property to determine which portion of the input image is available 
/// to be processed.
/// 
/// To finish setting up or performing your image processing routine, use the provided ``CIImageProcessorOutput``
/// object to return processed pixel data to Core Image.
///
NS_CLASS_AVAILABLE(10_12, 10_0)
@protocol CIImageProcessorInput

/// The rectangular region of the input image that your Core Image Processor Kernel can use to provide the output.
/// > Note: This will contain but may be larger than the rect returned by 'roiCallback'.
@property (nonatomic, readonly) CGRect region;


/// The bytes per row of the CPU memory that your Core Image Processor Kernel can read pixelsfrom.
@property (nonatomic, readonly) size_t bytesPerRow;

/// The pixel format of the CPU memory that your Core Image Processor Kernel can read pixels from.
@property (nonatomic, readonly) CIFormat format;

/// The base address of CPU memory that your Core Image Processor Kernel can read pixels from.
/// > Warning: This memory must not be modified by the ``CIImageProcessorKernel``.
@property (readonly, nonatomic) const void *baseAddress NS_RETURNS_INNER_POINTER;


#if COREIMAGE_SUPPORTS_IOSURFACE
/// An input surface object that your Core Image Processor Kernel can read from.
/// > Warning: This surface must not be modified by the ``CIImageProcessorKernel``.
@property (nonatomic, readonly) IOSurfaceRef surface;
#endif

/// An input pixel buffer object that your Core Image Processor Kernel can read from.
/// > Warning: This buffer must not be modified by the ``CIImageProcessorKernel``.
@property (nonatomic, readonly, nullable) CVPixelBufferRef pixelBuffer;


/// A MTLTexture object that can be bound for input using Metal.
/// > Warning: This texture must not be modified by the ``CIImageProcessorKernel``.
@property (nonatomic, readonly, nullable) id<MTLTexture> metalTexture;

/// A 64-bit digest that uniquely describes the contents of the input to a processor.
/// 
/// This digest will change if the graph of the input changes in any way.
@property (nonatomic, readonly) uint64_t digest NS_AVAILABLE(13_0, 16_0);

/// This property tells a tiled-input processor which input tile index is being processed.
/// 
/// This property is only relevant if your processor implements ``/CIImageProcessorKernel/roiTileArrayForInput:arguments:outputRect:``
/// 
/// This can be useful if the processor needs to clear the ``CIImageProcessorOutput`` before the first tile is processed.
@property (nonatomic, readonly) NSUInteger roiTileIndex NS_AVAILABLE(14_0, 17_0);

/// This property tells a tiled-input processor how many input tiles will be processed.
/// 
/// This property is only relevant if your processor implements ``/CIImageProcessorKernel/roiTileArrayForInput:arguments:outputRect:``
/// 
/// This can be useful if the processor needs to do work ``CIImageProcessorOutput`` after the last tile is processed.
@property (nonatomic, readonly) NSUInteger roiTileCount NS_AVAILABLE(14_0, 17_0);

@end


/// Your app does not define classes that adopt this protocol; Core Image provides an object of this type 
/// when rendering a custom image processor you create with a ``CIImageProcessorKernel`` subclass.
/// 
/// When a `CIImage` containing your `CIImageProcessorKernel` class is rendered, your
/// ``CIImageProcessorKernel/processWithInputs:arguments:output:error:`` class method will be called as 
/// needed for that render.  The method may be called more than once if Core Image needs to tile to
/// limit memory usage.
/// 
/// When your image processor class method is called, use the provided `CIImageProcessorOutput` object to return
/// processed pixel data to Core Image. For example, if you process the image using a Metal shader, bind the `metalTexture` 
/// property as an attachment in a render pass or as an output texture in a compute pass. Or, if you process the image
/// using a CPU-based routine, write processed pixel data to memory using the `baseAddress` pointer. 
/// 
/// You should use the output's `region` property to determine which portion of the output image needs to be processed. 
/// Your code should fill the entirety of the `region`. This includes setting to zero any pixels in the `region` that 
/// are outside the extent passed extent `applyWithExtent:inputs:arguments:error:`.
/// 
/// > Important: You must provide rendered output using only one of the following properties of the output: 
/// `baseAddress`, `surface`, `pixelBuffer`, `metalTexture`.
///
NS_CLASS_AVAILABLE(10_12, 10_0)
@protocol CIImageProcessorOutput

/// The rectangular region of the output image that your Core Image Processor Kernel must provide.
/// > Note: This may be different (larger or smaller) than the `extent` that was passed to 
/// ``/CIImageProcessorKernel/applyWithExtent:inputs:arguments:error:``.
@property (nonatomic, readonly) CGRect region;

/// The bytes per row of the CPU memory that your Core Image Processor Kernel can write pixels to.
@property (nonatomic, readonly) size_t bytesPerRow;

/// The pixel format of the CPU memory that your Core Image Processor Kernel can write pixels to.
@property (nonatomic, readonly) CIFormat format;

/// The base address of CPU memory that your Core Image Processor Kernel can write pixels to.
@property (readonly, nonatomic) void *baseAddress NS_RETURNS_INNER_POINTER;


#if COREIMAGE_SUPPORTS_IOSURFACE
/// An output surface object that your Core Image Processor Kernel can write to.
@property (nonatomic, readonly) IOSurfaceRef surface;
#endif

/// An output pixelBuffer object that your Core Image Processor Kernel can write to.
@property (nonatomic, readonly, nullable) CVPixelBufferRef pixelBuffer;


/// A Metal texture object that can be bound for output using Metal.
@property (nonatomic, readonly, nullable) id<MTLTexture> metalTexture;

/// Returns a Metal command buffer object that can be used for encoding commands.
@property (nonatomic, readonly, nullable) id<MTLCommandBuffer> metalCommandBuffer;

/// A 64-bit digest that uniquely describes the contents of the output of a processor.
/// 
/// This digest will change if the graph up to and including the output of the processor changes in any way.
@property (nonatomic, readonly) uint64_t digest NS_AVAILABLE(13_0, 16_0);

/// Returns a temporary IOSurface that your Core Image Processor Kernel can use as scratch storage during processing.
///
/// Use this method when your processor needs an intermediate `IOSurface` to stash data between stages of its work.
/// Core Image manages the lifetime of the returned surface and reuses the underlying allocation across multiple invocations
/// when possible. This is more efficient than allocating a fresh `IOSurface` on each invocation of your processor.
///
/// The returned surface is valid only for the duration of the
/// ``CIImageProcessorKernel/processWithInputs:arguments:output:error:`` call that requested it.
/// Don't retain it beyond the scope of that method or use it after the method returns.
///
/// Calling this method multiple times within the same processor invocation with the same `identifier`, `format`,
/// `width`, and `height` returns the same surface. Otherwise it returns a distinct surface.
/// This lets a processor request several independent surfaces by giving each one a unique name.
///
/// - Parameters:
///    - identifier: A name that uniquely identifies this scratch surface within the processor invocation.
///    - format: The pixel format for the surface. Must be a non-zero `OSType` pixel format constant.
///    - width: The width of the surface in pixels. Must be greater than zero.
///    - height: The height of the surface in pixels. Must be greater than zero.
/// - Returns:
///    An autoreleased `IOSurface` of the requested size and format, or `nil` if the surface could not be created.
///
- (nullable IOSurface *)temporarySurfaceWithIdentifier:(NSString *)identifier
                                                format:(OSType)format
                                                 width:(size_t)width
                                                height:(size_t)height
    NS_SWIFT_NAME(temporarySurface(identifier:format:width:height:))
API_AVAILABLE(macos(27.0), ios(27.0), tvos(27.0), visionos(27.0)) API_UNAVAILABLE(watchos);

/// Returns a temporary CVPixelBuffer that your Core Image Processor Kernel can use as scratch storage during processing.
///
/// Use this method when your processor needs an intermediate `CVPixelBuffer` to stash data between stages of its work.
/// Core Image manages the lifetime of the returned buffer and reuses the underlying allocation across multiple invocations
/// when possible. This is more efficient than allocating a fresh `CVPixelBuffer` on each invocation of your processor.
///
/// The returned pixel buffer is valid only for the duration of the
/// ``CIImageProcessorKernel/processWithInputs:arguments:output:error:`` call that requested it.
/// Don't retain it beyond the scope of that method or use it after the method returns.
///
/// Calling this method multiple times within the same processor invocation with the same `identifier`, `format`,
/// `width`, and `height` returns a pixel buffer backed by the same `IOSurface`. Otherwise it returns a distinct pixel buffer.
/// This lets a processor request several independent pixel buffers by giving each one a unique name.
///
/// - Parameters:
///    - identifier: A name that uniquely identifies this scratch buffer within the processor invocation.
///    - format: The pixel format for the buffer. Must be a non-zero `OSType` pixel format constant.
///    - width: The width of the buffer in pixels. Must be greater than zero.
///    - height: The height of the buffer in pixels. Must be greater than zero.
///    - attributes: An optional dictionary of `CVPixelBuffer` creation attributes.
/// - Returns:
///    A non-retained `CVPixelBuffer` of the requested size and format, or `nil` if the buffer could not be created.
///
- (nullable CVPixelBufferRef)temporaryPixelBufferWithIdentifier:(NSString *)identifier
                                                         format:(OSType)format
                                                          width:(size_t)width
                                                         height:(size_t)height
                                                     attributes:(nullable NSDictionary *)attributes
    NS_SWIFT_NAME(temporaryPixelBuffer(identifier:format:width:height:attributes:))
API_AVAILABLE(macos(27.0), ios(27.0), tvos(27.0), visionos(27.0)) API_UNAVAILABLE(watchos) CF_RETURNS_NOT_RETAINED;

@end


NS_ASSUME_NONNULL_END

#endif /* __OBJC__ */

#endif /* CIIMAGEPROCESSOR_H */
