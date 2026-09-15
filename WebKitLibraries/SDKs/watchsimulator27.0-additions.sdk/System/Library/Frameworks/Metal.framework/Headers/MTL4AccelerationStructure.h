//
//  MTL4AccelerationStructure.h
//  Metal
//
//  Copyright (c) 2024 Apple Inc. All rights reserved.
//

#import <Metal/MTLAccelerationStructure.h>


NS_ASSUME_NONNULL_BEGIN

/// Base class for Metal 4 acceleration structure descriptors.
///
/// Don't use this class directly. Use one of its subclasses instead.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4AccelerationStructureDescriptor : MTLAccelerationStructureDescriptor
@end

/// Base class for all Metal 4 acceleration structure geometry descriptors.
///
/// Don't use this class directly. Use one of the derived classes instead.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4AccelerationStructureGeometryDescriptor : NSObject <NSCopying>

/// Sets the offset that this geometry contributes to determining the intersection function to invoke when a ray intersects it.
///
/// When you perform a ray tracing operation in the Metal Shading Language, and provide the ray intersector object
/// with an instance of ``MTLIntersectionFunctionTable``, Metal adds this offset to the instance offset from structs such
/// as:
///
/// - ``MTLAccelerationStructureInstanceDescriptor``
/// - ``MTLAccelerationStructureUserIDInstanceDescriptor``
/// - ``MTLAccelerationStructureMotionInstanceDescriptor``
/// - ``MTLIndirectAccelerationStructureInstanceDescriptor``
/// - ``MTLIndirectAccelerationStructureMotionInstanceDescriptor``
///
/// The sum of these offsets provides an index into the intersection function table that the ray tracing system uses
/// to retrieve and invoke the function at this index, allowing you to customize the intersection evaluation process.
@property (nonatomic) NSUInteger intersectionFunctionTableOffset;

/// Provides a hint to Metal that this geometry is opaque, potentially accelerating the ray/primitive intersection process.
@property (nonatomic) BOOL opaque;

/// A boolean value that indicates whether the ray-tracing system in Metal allows the invocation of intersection functions
/// more than once per ray-primitive intersection.
///
/// The property's default value is <doc://com.apple.documentation/documentation/swift/true>.
@property (nonatomic) BOOL allowDuplicateIntersectionFunctionInvocation;

/// Assigns an optional label you can assign to this geometry for debugging purposes.
@property (nonatomic, copy, nullable) NSString* label;

/// Assigns optional buffer containing data to associate with each primitive in this geometry.
///
/// You can use zero as the buffer address in this buffer range.
@property (nonatomic) MTL4BufferRange primitiveDataBuffer;

/// Defines the stride, in bytes, between each primitive's data in the primitive data buffer ``primitiveDataBuffer`` references.
///
/// You are responsible for ensuring the stride is at least ``primitiveDataElementSize`` in size and a multiple of 4 bytes.
///
/// This property defaults to `0` bytes,  which indicates the stride is equal to ``primitiveDataElementSize``.
@property (nonatomic) NSUInteger primitiveDataStride;

/// Sets the size, in bytes, of the data for each primitive in the primitive data buffer ``primitiveDataBuffer`` references.
///
/// This size needs to be at most ``primitiveDataStride`` in size and a multiple of 4 bytes.
///
/// This property defaults to 0 bytes.
@property (nonatomic) NSUInteger primitiveDataElementSize;

@end

/// Descriptor for a primitive acceleration structure that directly references geometric shapes, such as triangles and
/// bounding boxes.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4PrimitiveAccelerationStructureDescriptor : MTL4AccelerationStructureDescriptor

/// Associates the array of geometry descriptors that comprise this primitive acceleration structure.
///
/// If you enable keyframe motion by setting property ``motionKeyframeCount`` to a value greater than `1`, then
/// all geometry descriptors this array references need to be motion geometry descriptors and have a number of
/// primitive buffers equals to ``motionKeyframeCount``.
///
/// Example of motion geometry descriptors include: ``MTL4AccelerationStructureMotionTriangleGeometryDescriptor``,
/// ``MTL4AccelerationStructureMotionBoundingBoxGeometryDescriptor``, ``MTL4AccelerationStructureMotionCurveGeometryDescriptor``.
@property (nonatomic, retain, nullable) NSArray <MTL4AccelerationStructureGeometryDescriptor *> *geometryDescriptors;

/// Configures the behavior when the ray-tracing system samples the acceleration structure before the motion start time.
///
/// Use this property to control the behavior when the ray-tracing system samples the acceleration structure
/// at a time prior to the one you set for ``motionStartTime``.
///
/// The default value of this property is `MTLMotionBorderModeClamp`.
@property (nonatomic) MTLMotionBorderMode motionStartBorderMode;

/// Configures the motion border mode.
///
/// This property controls what happens if Metal samples the acceleration structure after ``motionEndTime``.
///
/// Its default value is `MTLMotionBorderModeClamp`.
@property (nonatomic) MTLMotionBorderMode motionEndBorderMode;

/// Configures the motion start time for this geometry.
///
/// The default value of this property is `0.0f`.
@property (nonatomic) float motionStartTime;

/// Configures the motion end time for this geometry.
///
/// The default value of this property is `1.0f`.
@property (nonatomic) float motionEndTime;

/// Sets the motion keyframe count.
///
/// This property's default is `1`, indicating no motion.
@property (nonatomic) NSUInteger motionKeyframeCount;

@end

/// Describes triangle geometry suitable for ray tracing.
///
/// Use a ``MTLResidencySet`` to mark residency of all buffers this descriptor references when you build this
/// acceleration structure.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4AccelerationStructureTriangleGeometryDescriptor : MTL4AccelerationStructureGeometryDescriptor

/// Associates a vertex buffer containing triangle vertices.
///
/// You are responsible for ensuring that the format of all vertex positions match the ``vertexFormat`` property, and
/// that the buffer address for the buffer range is not zero.
@property (nonatomic) MTL4BufferRange vertexBuffer;

/// Describes the format of the vertices in the vertex buffer.
///
/// This property controls the format of the position attribute of the vertices the ``vertexBuffer`` references.
///
/// The format defaults to `MTLAttributeFormatFloat3`, corresponding to three packed floating point numbers.
@property (nonatomic) MTLAttributeFormat vertexFormat;

/// Sets the stride, in bytes, between vertices in the vertex buffer.
///
/// The stride you specify needs to be a multiple of the size of the vertex format you provide in the ``vertexFormat``
/// property. Similarly, you are responsible for ensuring this stride matches the vertex format data type's alignment.
///
/// Defaults to `0`, which signals the stride matches the size of the ``vertexFormat`` data.
@property (nonatomic) NSUInteger vertexStride;

/// Sets an optional index buffer containing references to vertices in the `vertexBuffer`.
///
/// You can set this property to `0`, the default, to avoid specifying an index buffer.
@property (nonatomic) MTL4BufferRange indexBuffer;

/// Configures the size of the indices the `indexBuffer` contains, which is typically either 16 or 32-bits for each index.
@property (nonatomic) MTLIndexType indexType;

/// Declares the number of triangles in this geometry descriptor.
@property (nonatomic) NSUInteger triangleCount;

/// Assigns an optional reference to a buffer containing a `float4x3` transformation matrix.
///
/// When the buffer address is non-zero, Metal applies this transform to the vertex data positions when building
/// the acceleration structure.
///
/// Building an acceleration structure with a descriptor that specifies this property doesn't modify the contents of
/// the input `vertexBuffer`.
@property (nonatomic) MTL4BufferRange transformationMatrixBuffer;

/// Configures the layout for the transformation matrix in the transformation matrix buffer.
///
/// You can provide matrices in column-major or row-major form, and this property allows you to control
/// how Metal interprets them.
///
/// Defaults to `MTLMatrixLayoutColumnMajor`.
@property (nonatomic) MTLMatrixLayout transformationMatrixLayout;

@end

/// Describes bounding-box geometry suitable for ray tracing.
///
/// You use bounding boxes to implement procedural geometry for ray tracing, such as spheres or any other shape
/// you define by using intersection functions.
///
/// Use a ``MTLResidencySet`` to mark residency of all buffers this descriptor references when you build this
/// acceleration structure.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4AccelerationStructureBoundingBoxGeometryDescriptor : MTL4AccelerationStructureGeometryDescriptor

/// References a buffer containing bounding box data in `MTLAxisAlignedBoundingBoxes` format.
///
/// You are responsible for ensuring the buffer address of the range is not zero.
@property (nonatomic) MTL4BufferRange boundingBoxBuffer;

/// Assigns the stride, in bytes, between bounding boxes in the bounding box buffer `boundingBoxBuffer` references.
///
/// You are responsible for ensuring this stride is at least 24 bytes and a multiple of 4 bytes.
///
/// This property defaults to `24` bytes.
@property (nonatomic) NSUInteger boundingBoxStride;

/// Describes the number of bounding boxes the `boundingBoxBuffer` contains.
@property (nonatomic) NSUInteger boundingBoxCount;

@end

/// Describes motion triangle geometry, suitable for motion ray tracing.
///
/// Use a ``MTLResidencySet`` to mark residency of all buffers this descriptor references when you build this
/// acceleration structure.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4AccelerationStructureMotionTriangleGeometryDescriptor : MTL4AccelerationStructureGeometryDescriptor

/// Assigns a buffer where each entry contains a reference to a vertex buffer.
///
/// This property references a buffer that conceptually represents an array with one entry for each keyframe in the
/// motion animation. Each one of these entries consists of a ``MTL4BufferRange`` that, in turn, references a
/// vertex buffer containing the vertex data for the keyframe.
///
/// You are responsible for ensuring the buffer address is not zero for the top-level buffer, as well as for all
/// the vertex buffers it references.
@property (nonatomic) MTL4BufferRange vertexBuffers;

/// Defines the format of the vertices in the vertex buffers.
///
/// All keyframes share the same vertex format. Defaults to `MTLAttributeFormatFloat3`, corresponding to three packed
/// floating point numbers.
@property (nonatomic) MTLAttributeFormat vertexFormat;

/// Sets the stride, in bytes, between vertices in all the vertex buffer.
///
/// All keyframes share the same vertex stride. This stride needs to be a multiple of the size of the vertex format you
/// provide in the ``vertexFormat`` property.
///
/// Similarly, you are responsible for ensuring this stride matches the vertex format data type's alignment.
///
/// Defaults to `0`, which signals the stride matches the size of the ``vertexFormat`` data.
@property (nonatomic) NSUInteger vertexStride;

/// Assigns an optional index buffer containing references to vertices in the vertex buffers you reference through the
/// vertex buffers property.
///
/// You can set this property to `0`, the default, to avoid specifying an index buffer. All keyframes share the same
/// index buffer.
@property (nonatomic) MTL4BufferRange indexBuffer;

/// Specifies the size of the indices the `indexBuffer` contains, which is typically either 16 or 32-bits for each index.
@property (nonatomic) MTLIndexType indexType;

/// Declares the number of triangles in the vertex buffers that the buffer in the vertex buffers property references.
///
/// All keyframes share the same triangle count.
@property (nonatomic) NSUInteger triangleCount;

/// Assings an optional reference to a buffer containing a `float4x3` transformation matrix.
///
/// When the buffer address is non-zero, Metal applies this transform to the vertex data positions when building
/// the acceleration structure. All keyframes share the same transformation matrix.
///
/// Building an acceleration structure with a descriptor that specifies this property doesn't modify the contents of
/// the input `vertexBuffer`.
@property (nonatomic) MTL4BufferRange transformationMatrixBuffer;

/// Configures the layout for the transformation matrix in the transformation matrix buffer.
///
/// You can provide matrices in column-major or row-major form, and this property allows you to control
/// how Metal interprets them.
///
/// Defaults to `MTLMatrixLayoutColumnMajor`.
@property (nonatomic) MTLMatrixLayout transformationMatrixLayout;

@end

/// Describes motion bounding box geometry, suitable for motion ray tracing.
///
/// You use bounding boxes to implement procedural geometry for ray tracing, such as spheres or any other shape
/// you define by using intersection functions.
///
/// Use a ``MTLResidencySet`` to mark residency of all buffers this descriptor references when you build this
/// acceleration structure.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4AccelerationStructureMotionBoundingBoxGeometryDescriptor : MTL4AccelerationStructureGeometryDescriptor


/// Configures a reference to a buffer where each entry contains a reference to a buffer of bounding boxes.
///
/// This property references a buffer that conceptually represents an array with one entry for each keyframe in the
/// motion animation. Each one of these entries consists of a ``MTL4BufferRange`` that, in turn, references a
/// vertex buffer containing the bounding box data for the keyframe.
///
/// You are responsible for ensuring the buffer address is not zero for the top-level buffer, as well as for all
/// the vertex buffers it references.
///
@property (nonatomic) MTL4BufferRange boundingBoxBuffers;

/// Declares the stride, in bytes, between bounding boxes in the bounding box buffers each entry in `boundingBoxBuffer`
/// references.
///
/// All keyframes share the same bounding box stride. You are responsible for ensuring this stride is at least 24 bytes
/// and a multiple of 4 bytes.
///
/// This property defaults to `24` bytes.
@property (nonatomic) NSUInteger boundingBoxStride;

/// Declares the number of bounding boxes in each buffer that `boundingBoxBuffer` references.
///
/// All keyframes share the same bounding box count.
@property (nonatomic) NSUInteger boundingBoxCount;

@end

/// Describes curve geometry suitable for ray tracing.
///
/// Use a ``MTLResidencySet`` to mark residency of all buffers this descriptor references when you build this
/// acceleration structure.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4AccelerationStructureCurveGeometryDescriptor : MTL4AccelerationStructureGeometryDescriptor

/// References a buffer containing curve control points.
///
/// Control points are interpolated according to the basis function you specify in ``curveBasis``.
///
/// You are responsible for ensuring each control is in a format matching the control point format ``controlPointFormat``
/// specifies, as well as ensuring that the buffer address of the range is not zero.
@property (nonatomic) MTL4BufferRange controlPointBuffer;

/// Declares the number of control points in the control point buffer.
@property (nonatomic) NSUInteger controlPointCount;

/// Sets the stride, in bytes, between control points in the control point buffer the control point buffer references.
///
/// You are responsible for ensuring this stride is a multiple of the control point format's element size, and
/// at a minimum exactly the control point format's size.
///
/// This property defaults to `0`, indicating that the control points are tightly-packed.
@property (nonatomic) NSUInteger controlPointStride;

/// Declares the format of the control points the control point buffer references.
///
/// Defaults to `MTLAttributeFormatFloat3`, representing 3 floating point values tightly packed.
@property (nonatomic) MTLAttributeFormat controlPointFormat;

/// Assigns a reference to a buffer containing the curve radius for each control point.
///
/// Metal interpolates curve radii according to the basis function you specify via ``curveBasis``.
///
/// You are responsible for ensuring the type of each radius matches the type property ``radiusFormat`` specifies,
/// that each radius is at least zero, and that the buffer address of the range is not zero.
@property (nonatomic) MTL4BufferRange radiusBuffer;

/// Declares the format of the radii in the radius buffer.
///
/// Defaults to  `MTLAttributeFormatFloat`.
@property (nonatomic) MTLAttributeFormat radiusFormat;

/// Configures the stride, in bytes, between radii in the radius buffer.
///
/// You are responsible for ensuring this property is set to a multiple of the size corresponding to the ``radiusFormat``.
///
/// This property defaults to `0` bytes, indicating that the radii are tightly packed.
@property (nonatomic) NSUInteger radiusStride;

/// Assigns an optional index buffer containing references to control points in the control point buffer.
///
/// Each index represents the first control point of a curve segment. You are responsible for ensuring the buffer
/// address of the range is not zero.
@property (nonatomic) MTL4BufferRange indexBuffer;

/// Specifies the size of the indices the `indexBuffer` contains, which is typically either 16 or 32-bits for each index.
@property (nonatomic) MTLIndexType indexType;

/// Declares the number of curve segments.
@property (nonatomic) NSUInteger segmentCount;

/// Declares the number of control points per curve segment.
///
/// Valid values for this property are `2`, `3`, or `4`.
@property (nonatomic) NSUInteger segmentControlPointCount;

/// Controls the curve type.
///
/// Defaults to `MTLCurveTypeRound`.
@property (nonatomic) MTLCurveType curveType;

/// Controls the curve basis function, determining how Metal interpolates the control points.
///
/// Defaults to `MTLCurveBasisBSpline`.
@property (nonatomic) MTLCurveBasis curveBasis;

/// Sets the type of curve end caps.
///
/// Defaults to `MTLCurveEndCapsNone`.
@property (nonatomic) MTLCurveEndCaps curveEndCaps;

@end

/// Describes motion curve geometry, suitable for motion ray tracing.
///
/// Use a ``MTLResidencySet`` to mark residency of all buffers this descriptor references when you build this
/// acceleration structure.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4AccelerationStructureMotionCurveGeometryDescriptor : MTL4AccelerationStructureGeometryDescriptor

/// Assigns a reference to a buffer where each entry contains a reference to a buffer of control points.
///
/// This property references a buffer that conceptually represents an array with one entry for each keyframe in the
/// motion animation. Each one of these entries consists of a ``MTL4BufferRange`` that, in turn, references a
/// buffer containing the control points corresponding to the keyframe.
///
/// You are responsible for ensuring the buffer address is not zero for the top-level buffer, as well as for all
/// the vertex buffers it references.
///
@property (nonatomic) MTL4BufferRange controlPointBuffers;

/// Specifies the number of control points in the buffers the control point buffers reference.
///
/// All keyframes have the same number of control points.
@property (nonatomic) NSUInteger controlPointCount;

/// Sets the stride, in bytes, between control points in the control point buffer.
///
/// All keyframes share the same control point stride.
///
/// You are responsible for ensuring this stride is a multiple of the control point format's element size, and
/// at a minimum exactly the control point format's size.
///
/// This property defaults to `0`, indicating that the control points are tightly-packed.
@property (nonatomic) NSUInteger controlPointStride;

/// Declares the format of the control points in the buffers that the control point buffers reference.
///
/// All keyframes share the same control point format. Defaults to `MTLAttributeFormatFloat3`, representing 3 floating
/// point values tightly packed.
@property (nonatomic) MTLAttributeFormat controlPointFormat;

/// Assigns a reference to a buffer containing, in turn, references to curve radii buffers.
///
/// This property references a buffer that conceptually represents an array with one entry for each keyframe in the
/// motion animation. Each one of these entries consists of a ``MTL4BufferRange`` that, in turn, references a
/// buffer containing the radii corresponding to the keyframe.
///
/// Metal interpolates curve radii according to the basis function you specify via ``curveBasis``.
///
/// You are responsible for ensuring the type of each radius matches the type property ``radiusFormat`` specifies,
/// that each radius is at least zero, and that the buffer address of the top-level buffer, as well as of buffer
/// it references, is not zero.
@property (nonatomic) MTL4BufferRange radiusBuffers;

/// Sets the format of the radii in the radius buffer.
///
/// Defaults to  `MTLAttributeFormatFloat`. All keyframes share the same radius format.
@property (nonatomic) MTLAttributeFormat radiusFormat;

/// Sets the stride, in bytes, between radii in the radius buffer.
///
/// You are responsible for ensuring this property is set to a multiple of the size corresponding to the ``radiusFormat``.
/// All keyframes share the same radius stride.
///
/// This property defaults to `0` bytes, indicating that the radii are tightly packed.
@property (nonatomic) NSUInteger radiusStride;

/// Assigns an optional index buffer containing references to control points in the control point buffers.
///
/// All keyframes share the same index buffer, with each index representing the first control point of a curve segment.
///
/// You are responsible for ensuring the buffer address of the range is not zero.
@property (nonatomic) MTL4BufferRange indexBuffer;

/// Configures the size of the indices the `indexBuffer` contains, which is typically either 16 or 32-bits for each index.
@property (nonatomic) MTLIndexType indexType;

/// Declares the number of curve segments.
///
/// All keyframes have the same number of curve segments.
@property (nonatomic) NSUInteger segmentCount;

/// Controls the number of control points per curve segment.
///
/// Valid values for this property are `2`, `3`, or `4`. All keyframes have the same number of control points per curve segment.
@property (nonatomic) NSUInteger segmentControlPointCount;

/// Controls the curve type.
///
/// Defaults to `MTLCurveTypeRound`. All keyframes share the same curve type.
@property (nonatomic) MTLCurveType curveType;

/// Sets the curve basis function, determining how Metal interpolates the control points.
///
/// Defaults to `MTLCurveBasisBSpline`. All keyframes share the same curve basis function.
@property (nonatomic) MTLCurveBasis curveBasis;

/// Configures the type of curve end caps.
///
/// Defaults to `MTLCurveEndCapsNone`. All keyframes share the same end cap type.
@property (nonatomic) MTLCurveEndCaps curveEndCaps;

@end

/// Descriptor for an instance acceleration structure.
///
/// An instance acceleration structure references other acceleration structures, and provides the ability to
/// "instantiate" them multiple times, each one with potentially a different transformation matrix.
///
/// You specify the properties of the instances in the acceleration structure this descriptor builds by providing a
/// buffer of `structs` via its ``instanceDescriptorBuffer`` property.
///
/// Use a ``MTLResidencySet`` to mark residency of all buffers and acceleration structures this descriptor references
/// when you build this acceleration structure.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4InstanceAccelerationStructureDescriptor : MTL4AccelerationStructureDescriptor

/// Assigns a reference to a buffer containing instance descriptors for acceleration structures to reference.
///
/// This buffer conceptually represents an array of instance data. The specific format for the structs that comprise
/// each entry depends on the value of the  ``instanceDescriptorType`` property.
///
/// You are responsible for ensuring the buffer address the range contains is not zero.
@property (nonatomic) MTL4BufferRange instanceDescriptorBuffer;

/// Sets the stride, in bytes, between instance descriptors the instance descriptor buffer references.
///
/// You are responsible for ensuring this stride is at least the size of the structure type corresponding to the instance
/// descriptor type and a multiple of 4 bytes.
///
/// Defaults to `0`, indicating the instance descriptors are tightly packed.
@property (nonatomic) NSUInteger instanceDescriptorStride;

/// Controls the number of instance descriptors in the instance descriptor buffer references.
@property (nonatomic) NSUInteger instanceCount;

/// Sets the type of instance descriptor that the instance descriptor buffer references.
///
/// This value determines the layout Metal expects for the structs the instance descriptor buffer contains.
///
/// Defaults to `MTLAccelerationStructureInstanceDescriptorTypeIndirect`. Valid values for this property are
/// `MTLAccelerationStructureInstanceDescriptorTypeIndirect` or `MTLAccelerationStructureInstanceDescriptorTypeIndirectMotion`.
@property (nonatomic) MTLAccelerationStructureInstanceDescriptorType instanceDescriptorType;

/// A buffer containing transformation information for instance motion keyframes, formatted according
/// to the motion transform type.
///
/// Each instance can have a different number of keyframes that you configure via individual instance
/// descriptors.
///
/// You are responsible for ensuring the buffer address the range references is not zero when using motion instance descriptors.
@property (nonatomic) MTL4BufferRange motionTransformBuffer;

/// Controls the total number of motion transforms in the motion transform buffer.
@property (nonatomic) NSUInteger motionTransformCount;

/// Specifies the layout for the transformation matrices in the instance descriptor buffer and the motion transformation matrix buffer.
///
/// Metal interprets the value of this property as the layout for the buffers that both ``instanceDescriptorBuffer`` and
/// ``motionTransformBuffer`` reference.
///
/// Defaults to `MTLMatrixLayoutColumnMajor`.
@property (nonatomic) MTLMatrixLayout instanceTransformationMatrixLayout;

/// Controls the type of motion transforms, either as a matrix or individual components.
///
/// Defaults to `MTLTransformTypePackedFloat4x3`. Using a `MTLTransformTypeComponent` allows you to represent the
/// rotation by a quaternion (instead as of part of the matrix), allowing for correct motion interpolation.
@property (nonatomic) MTLTransformType motionTransformType;

/// Specify the stride for motion transform.
///
/// Defaults to `0`, indicating that transforms are tightly packed according to the motion transform type.
@property (nonatomic) NSUInteger motionTransformStride;

@end

/// Descriptor for an "indirect" instance acceleration structure that allows providing the instance count and
/// motion transform count indirectly, through buffer references.
///
/// An instance acceleration structure references other acceleration structures, and provides the ability to
/// "instantiate" them multiple times, each one with potentially a different transformation matrix.
///
/// You specify the properties of the instances in the acceleration structure this descriptor builds by providing a
/// buffer of `structs` via its ``instanceDescriptorBuffer`` property.
///
/// Compared to ``MTL4InstanceAccelerationStructureDescriptor``, this descriptor allows you to provide the number
/// of instances it references indirectly through a buffer reference, as well as the number of motion transforms.
///
/// This enables you to determine these counts indirectly in the GPU timeline via a compute pipeline.
/// Metal needs only to know the maximum possible number of instances and motion transforms to support,
/// which you specify via the ``maxInstanceCount`` and ``maxMotionTransformCount`` properties.
///
/// Use a ``MTLResidencySet`` to mark residency of all buffers and acceleration structures this descriptor references
/// when you build this acceleration structure.
MTL_EXPORT API_AVAILABLE(macos(26.0), ios(26.0))
@interface MTL4IndirectInstanceAccelerationStructureDescriptor : MTL4AccelerationStructureDescriptor

/// Assigns a reference to a buffer containing instance descriptors for acceleration structures to reference.
///
/// This buffer conceptually represents an array of instance data. The specific format for the structs that comprise
/// each entry depends on the value of the  ``instanceDescriptorType`` property.
///
/// You are responsible for ensuring the buffer address the range contains is not zero.
@property (nonatomic) MTL4BufferRange instanceDescriptorBuffer;

/// Sets the stride, in bytes, between instance descriptors in the instance descriptor buffer.
///
/// You are responsible for ensuring this stride is at least the size of the structure type corresponding to the instance
/// descriptor type and a multiple of 4 bytes.
///
/// Defaults to `0`, indicating the instance descriptors are tightly packed.
@property (nonatomic) NSUInteger instanceDescriptorStride;

/// Controls the maximum number of instance descriptors the instance descriptor buffer can reference.
///
/// You are responsible for ensuring that the final number of instances at build time, which you provide indirectly
/// via a buffer reference in ``instanceCountBuffer``, is less than or equal to this number.
@property (nonatomic) NSUInteger maxInstanceCount;

/// Provides a reference to a buffer containing the number of instances in the instance descriptor buffer, formatted as a
/// 32-bit unsigned integer.
///
/// You are responsible for ensuring that the final number of instances at build time, which you provide indirectly
/// via this buffer reference , is less than or equal to the value of property ``maxInstanceCount``.
@property (nonatomic) MTL4BufferRange instanceCountBuffer;

/// Controls the type of instance descriptor that the instance descriptor buffer references.
///
/// This value determines the layout Metal expects for the structs the instance descriptor buffer contains.
///
/// Defaults to `MTLAccelerationStructureInstanceDescriptorTypeIndirect`. Valid values for this property are
/// `MTLAccelerationStructureInstanceDescriptorTypeIndirect` or `MTLAccelerationStructureInstanceDescriptorTypeIndirectMotion`.
@property (nonatomic) MTLAccelerationStructureInstanceDescriptorType instanceDescriptorType;

/// A buffer containing transformation information for instance motion keyframes, formatted according
/// to the motion transform type.
///
/// Each instance can have a different number of keyframes that you configure via individual instance
/// descriptors.
///
/// You are responsible for ensuring the buffer address the range references is not zero when using motion instance descriptors.
@property (nonatomic) MTL4BufferRange motionTransformBuffer;

/// Controls the maximum number of motion transforms in the motion transform buffer.
///
/// You are responsible for ensuring that final number of motion transforms at build time that the buffer
/// ``motionTransformCountBuffer`` references is less than or equal to this number.
@property (nonatomic) NSUInteger maxMotionTransformCount;

/// Associates a buffer reference containing the number of motion transforms in the motion transform buffer, formatted as a
/// 32-bit unsigned integer.
///
/// You are responsible for ensuring that the final number of motion transforms at build time in the buffer this property
/// references is less than or equal to the value of property ``maxMotionTransformCount``.
@property (nonatomic) MTL4BufferRange motionTransformCountBuffer;

/// Specifies the layout for the transformation matrices in the instance descriptor buffer and the motion transformation matrix buffer.
///
/// Metal interprets the value of this property as the layout for the buffers that both ``instanceDescriptorBuffer`` and
/// ``motionTransformBuffer`` reference.
///
/// Defaults to `MTLMatrixLayoutColumnMajor`.
@property (nonatomic) MTLMatrixLayout instanceTransformationMatrixLayout;

/// Sets the type of motion transforms, either as a matrix or individual components.
///
/// Defaults to `MTLTransformTypePackedFloat4x3`. Using a `MTLTransformTypeComponent` allows you to represent the
/// rotation by a quaternion (instead as of part of the matrix), allowing for correct motion interpolation.
@property (nonatomic) MTLTransformType motionTransformType;

/// Sets the stride for motion transform.
///
/// Defaults to `0`, indicating that transforms are tightly packed according to the motion transform type.
@property (nonatomic) NSUInteger motionTransformStride;

@end

NS_ASSUME_NONNULL_END

