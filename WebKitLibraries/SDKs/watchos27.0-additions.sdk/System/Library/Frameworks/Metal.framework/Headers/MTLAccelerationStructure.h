//
//  MTLAccelerationStructure.h
//  Metal
//
//  Copyright (c) 2020 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLArgument.h>
#import <Metal/MTLStageInputOutputDescriptor.h>
#import <Metal/MTLRenderCommandEncoder.h>
#import <Metal/MTLAccelerationStructureTypes.h>
#import <Metal/MTLResource.h>
#import <Metal/MTLStageInputOutputDescriptor.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 @enum MTLAccelerationStructureRefitOptions
 @abstract Controls the acceleration structure refit operation
 */
typedef NS_OPTIONS(NSUInteger, MTLAccelerationStructureRefitOptions) {
    /**
     * @brief Refitting shall result in updated vertex data from the provided geometry descriptor.
     * If not set, vertex buffers shall be ignored on the geometry descriptor and vertex data previously
     * encoded shall be copied.
     */
    MTLAccelerationStructureRefitOptionVertexData = (1 << 0),

    /**
     * @brief Refitting shall result in updated per primitive data from the provided geometry descriptor.
     * If not set, per primitive data buffers shall be ignored on the geometry descriptor and per primitive
     * data previously encoded shall be copied.
     */
    MTLAccelerationStructureRefitOptionPerPrimitiveData = (1 << 1),
} API_AVAILABLE(macos(13.0), ios(16.0));


@protocol MTLBuffer;
@protocol MTLAccelerationStructure;

typedef NS_OPTIONS(NSUInteger, MTLAccelerationStructureUsage) {
    /**
     * @brief Default usage
     */
    MTLAccelerationStructureUsageNone = 0,
    
    /**
     * @brief Enable refitting for this acceleration structure. Note that this may reduce
     * acceleration structure quality.
     */
    MTLAccelerationStructureUsageRefit = (1 << 0),
   
    /**
     * @brief Prefer building this acceleration structure quickly at the cost of reduced ray
     * tracing performance.
     */
    MTLAccelerationStructureUsagePreferFastBuild = (1 << 1),

    /**
     * @brief Enable extended limits for this acceleration structure, possibly at the cost of
     * reduced ray tracing performance.
     */
    MTLAccelerationStructureUsageExtendedLimits API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0)) = (1 << 2),

    /**
     * @brief Prioritize intersection performance over acceleration structure build time
     */
    MTLAccelerationStructureUsagePreferFastIntersection API_AVAILABLE(macos(26.0), ios(26.0)) = (1 << 4),

    /**
     * @brief Minimize the size of the acceleration structure in memory, potentially at
     * the cost of increased build time or reduced intersection performance.
     */
    MTLAccelerationStructureUsageMinimizeMemory API_AVAILABLE(macos(26.0), ios(26.0)) = (1 << 5),
} API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

typedef NS_OPTIONS(uint32_t, MTLAccelerationStructureInstanceOptions) {
    /**
     * @brief No options
     */
    MTLAccelerationStructureInstanceOptionNone = 0,

    /**
     * @brief Disable triangle back or front face culling
     */
    MTLAccelerationStructureInstanceOptionDisableTriangleCulling = (1 << 0),

    /**
     * @brief Override triangle front-facing winding. By default, the winding is
     * assumed to be clockwise unless overridden by the intersector object. This overrides
     * the intersector's winding order.
     */
    MTLAccelerationStructureInstanceOptionTriangleFrontFacingWindingCounterClockwise = (1 << 1),

    /**
     * @brief Geometry is opaque
     */
    MTLAccelerationStructureInstanceOptionOpaque = (1 << 2),

    /**
     * @brief Geometry is non-opaque
     */
    MTLAccelerationStructureInstanceOptionNonOpaque = (1 << 3),
} API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0));

typedef NS_ENUM(NSInteger, MTLMatrixLayout) {
    /**
     * @brief Column-major order
     */
    MTLMatrixLayoutColumnMajor = 0,
    
    /**
     * @brief Row-major order
     */
    MTLMatrixLayoutRowMajor = 1,
} API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.1), visionos(2.1));

/**
 * @brief Base class for acceleration structure descriptors. Do not use this class directly. Use
 * one of the derived classes instead.
 */
MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0))
@interface MTLAccelerationStructureDescriptor : NSObject <NSCopying>

@property (nonatomic) MTLAccelerationStructureUsage usage;

@end

/**
 * @brief Base class for all geometry descriptors. Do not use this class directly. Use one of the derived
 * classes instead.
 */
MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0))
@interface MTLAccelerationStructureGeometryDescriptor : NSObject <NSCopying>

@property (nonatomic) NSUInteger intersectionFunctionTableOffset;

/**
 * @brief Whether the geometry is opaque
 */
@property (nonatomic) BOOL opaque;

/**
 * @brief Whether intersection functions may be invoked more than once per ray/primitive
 * intersection. Defaults to YES.
 */
@property (nonatomic) BOOL allowDuplicateIntersectionFunctionInvocation;

/**
 * @brief Label
 */
@property (nonatomic, copy, nullable) NSString* label API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/**
 * @brief Data buffer containing per-primitive data. May be nil.
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> primitiveDataBuffer API_AVAILABLE(macos(13.0), ios(16.0));

/**
 * @brief Primitive data buffer offset in bytes. Must be aligned to the platform's buffer offset alignment. Defaults to 0 bytes.
 */
@property (nonatomic) NSUInteger primitiveDataBufferOffset API_AVAILABLE(macos(13.0), ios(16.0));

/**
 * @brief Stride, in bytes, between per-primitive data in the primitive data buffer. Must be at least primitiveDataElementSize and must be a
 * multiple of 4 bytes. Defaults to 0 bytes. Assumed to be equal to primitiveDataElementSize if zero.
 */
@property (nonatomic) NSUInteger primitiveDataStride API_AVAILABLE(macos(13.0), ios(16.0));

/**
 * @brief Size, in bytes, of the data for each primitive in the primitive data buffer. Must be at most primitiveDataStride and must be a
 * multiple of 4 bytes. Defaults to 0 bytes.
 */
@property (nonatomic) NSUInteger primitiveDataElementSize API_AVAILABLE(macos(13.0), ios(16.0));
@end

/**
 * @brief Describes what happens to the object before the first motion key and after the last
 * motion key.
 */
typedef NS_ENUM(uint32_t, MTLMotionBorderMode){

    /**
     * @brief Motion is stopped. (default)
     */
    MTLMotionBorderModeClamp = 0,

    /**
     * @brief Object disappears
     */
    MTLMotionBorderModeVanish = 1
} API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/**
 * @brief Descriptor for a primitive acceleration structure
 */
MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0))
@interface MTLPrimitiveAccelerationStructureDescriptor : MTLAccelerationStructureDescriptor

/**
 * @brief Array of geometry descriptors. If motionKeyframeCount is greater than one all geometryDescriptors
 * must be motion versions and have motionKeyframeCount of primitive buffers.
 */
@property (nonatomic, retain, nullable) NSArray <MTLAccelerationStructureGeometryDescriptor *> * geometryDescriptors;

/**
 * @brief Motion border mode describing what happens if acceleration structure is sampled before
 * motionStartTime. If not set defaults to MTLMotionBorderModeClamp.
 */
@property (nonatomic) MTLMotionBorderMode motionStartBorderMode API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/**
 * @brief Motion border mode describing what happens if acceleration structure is sampled after
 * motionEndTime. If not set defaults to MTLMotionBorderModeClamp.
 */
@property (nonatomic) MTLMotionBorderMode motionEndBorderMode API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/**
 * @brief Motion start time of this geometry. If not set defaults to 0.0f.
 */
@property (nonatomic) float motionStartTime API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/**
 * @brief Motion end time of this geometry. If not set defaults to 1.0f.
 */
@property (nonatomic) float motionEndTime API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/**
 * @brief Motion keyframe count. Is 1 by default which means no motion.
 */
@property (nonatomic) NSUInteger motionKeyframeCount API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

+ (instancetype)descriptor;

@end

/**
 * @brief Descriptor for triangle geometry
 */
MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0))
@interface MTLAccelerationStructureTriangleGeometryDescriptor : MTLAccelerationStructureGeometryDescriptor

/**
 * @brief Vertex buffer containing triangle vertices. Each vertex position must be formatted
 * according to the vertex format. Must not be nil.
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> vertexBuffer;

/**
 * @brief Vertex buffer offset. Must be a multiple of the vertex stride and must be aligned to the
 * platform's buffer offset alignment.
 */
@property (nonatomic) NSUInteger vertexBufferOffset;

/**
 * @brief Format type of the vertex buffer.
 * Defaults to MTLAttributeFormatFloat3 (packed).
 */
@property (nonatomic) MTLAttributeFormat vertexFormat API_AVAILABLE(macos(13.0), ios(16.0));

/**
 * @brief Stride, in bytes, between vertices in the vertex buffer. Must be a multiple of the vertex format data type size and must be aligned to
 * the vertex format data type's alignment. Defaults to 0, which will result in a stride of the vertex format data size.
 */
@property (nonatomic) NSUInteger vertexStride;

/**
 * Optional index buffer containing references to vertices in the vertex buffer. May be nil.
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> indexBuffer;

/**
 * @brief Index buffer offset. Must be a multiple of the index data type size and must be aligned to both
 * the index data type's alignment and the platform's buffer offset alignment.
 */
@property (nonatomic) NSUInteger indexBufferOffset;

/**
 * @brief Index type
 */
@property (nonatomic) MTLIndexType indexType;

/**
 * @brief Number of triangles
 */
@property (nonatomic) NSUInteger triangleCount;

/**
 * @brief Buffer containing packed float4x3 transformation matrix. Transform is applied to the vertex data when building the acceleration structure. Input vertex buffers are not modified.
 * When set to nil, transformation matrix is not applied to vertex data.
 */
@property (nonatomic, retain, nullable) id<MTLBuffer> transformationMatrixBuffer API_AVAILABLE(macos(13.0), ios(16.0));

/**
 * @brief Transformation matrix buffer offset. Must be a multiple of 4 bytes. Defaults to 0.
 */
@property (nonatomic) NSUInteger transformationMatrixBufferOffset API_AVAILABLE(macos(13.0), ios(16.0));

/**
 * @brief Matrix layout for the transformation matrix in the transformation
 * matrix buffer. Defaults to MTLMatrixLayoutColumnMajor.
 */
@property (nonatomic) MTLMatrixLayout transformationMatrixLayout API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.1), visionos(2.1));
+ (instancetype)descriptor;

@end

/**
 * @brief Descriptor for bounding box geometry
 */
MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0))
@interface MTLAccelerationStructureBoundingBoxGeometryDescriptor : MTLAccelerationStructureGeometryDescriptor

/**
 * @brief Bounding box buffer containing MTLAxisAlignedBoundingBoxes. Must not be nil.
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> boundingBoxBuffer;

/**
 * @brief Bounding box buffer offset. Must be a multiple of the bounding box stride and must be
 * aligned to the platform's buffer offset alignment.
 */
@property (nonatomic) NSUInteger boundingBoxBufferOffset;

/**
 * @brief Stride, in bytes, between bounding boxes in the bounding box buffer. Must be at least 24
 * bytes and must be a multiple of 4 bytes. Defaults to 24 bytes.
 */
@property (nonatomic) NSUInteger boundingBoxStride;

/**
 * @brief Number of bounding boxes
 */
@property (nonatomic) NSUInteger boundingBoxCount;

+ (instancetype)descriptor;

@end

/**
 * @brief MTLbuffer and description how the data is stored in it.
 */
MTL_EXPORT API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0))
@interface MTLMotionKeyframeData : NSObject

/**
 * @brief Buffer containing the data of a single keyframe. Multiple keyframes can be interleaved in one MTLBuffer.
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> buffer;

/**
 * @brief Buffer offset. Must be a multiple of 4 bytes.
 */
@property (nonatomic) NSUInteger offset;


+ (instancetype)data;

@end

/**
 * @brief Descriptor for motion triangle geometry
 */
MTL_EXPORT API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0))
@interface MTLAccelerationStructureMotionTriangleGeometryDescriptor : MTLAccelerationStructureGeometryDescriptor

/**
 * @brief Vertex buffer containing triangle vertices similar to what MTLAccelerationStructureTriangleGeometryDescriptor has but array of the values.
 */
@property (nonatomic, copy) NSArray <MTLMotionKeyframeData *> * vertexBuffers;

/**
 * @brief Format type of the vertex buffers across all keyframes.
 * Defaults to MTLAttributeFormatFloat3 (packed).
 */
@property (nonatomic) MTLAttributeFormat vertexFormat API_AVAILABLE(macos(13.0), ios(16.0));

/**
 * @brief Stride, in bytes, between vertices in each keyframe's vertex buffer. Must be a multiple of the vertex format data type size and must be aligned to
 * the vertex format data type's alignment. Defaults to 0, which will result in a stride of the vertex format data size.
 */
@property (nonatomic) NSUInteger vertexStride;

/**
 * Optional index buffer containing references to vertices in the vertex buffer. May be nil.
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> indexBuffer;

/**
 * @brief Index buffer offset. Must be a multiple of the index data type size and must be aligned to both
 * the index data type's alignment and the platform's buffer offset alignment.
 */
@property (nonatomic) NSUInteger indexBufferOffset;

/**
 * @brief Index type
 */
@property (nonatomic) MTLIndexType indexType;

/**
 * @brief Number of triangles
 */
@property (nonatomic) NSUInteger triangleCount;

/**
 * @brief Buffer containing packed float4x3 transformation matrix. Transform is applied to the vertex data when building the acceleration structure. Input vertex buffers are not modified.
 * The transformation matrix is applied to all keyframes' vertex data.
 * When set to nil, transformation matrix is not applied to vertex data.
 */
@property (nonatomic, retain, nullable) id<MTLBuffer> transformationMatrixBuffer API_AVAILABLE(macos(13.0), ios(16.0));

/**
 * @brief Transformation matrix buffer offset. Must be a multiple of 4 bytes. Defaults to 0.
 */
@property (nonatomic) NSUInteger transformationMatrixBufferOffset API_AVAILABLE(macos(13.0), ios(16.0));

/**
 * @brief Matrix layout for the transformation matrix in the transformation
 * matrix buffer. Defaults to MTLMatrixLayoutColumnMajor.
 */
@property (nonatomic) MTLMatrixLayout transformationMatrixLayout API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.1), visionos(2.1));
+ (instancetype)descriptor;

@end

/**
 * @brief Descriptor for motion bounding box geometry
 */
MTL_EXPORT API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0))
@interface MTLAccelerationStructureMotionBoundingBoxGeometryDescriptor : MTLAccelerationStructureGeometryDescriptor

/**
 * @brief Bounding box buffer containing MTLAxisAlignedBoundingBoxes similar to what MTLAccelerationStructureBoundingBoxGeometryDescriptor has but array of the values.
 */
@property (nonatomic, copy) NSArray <MTLMotionKeyframeData *> * boundingBoxBuffers;

/**
 * @brief Stride, in bytes, between bounding boxes in the bounding box buffer. Must be at least 24
 * bytes and must be a multiple of 4 bytes. Defaults to 24 bytes.
 */
@property (nonatomic) NSUInteger boundingBoxStride;

/**
 * @brief Number of bounding boxes
 */
@property (nonatomic) NSUInteger boundingBoxCount;

+ (instancetype)descriptor;

@end


/**
 * @brief Curve types
 */
typedef NS_ENUM(NSInteger, MTLCurveType) {
    /**
     * @brief Curve with a circular cross-section. These curves have the
     * advantage of having a real 3D shape consistent across different ray
     * directions, well-defined surface normals, etc. However, they may be
     * slower to intersect. These curves are ideal for viewing close-up.
     */
    MTLCurveTypeRound = 0,
    
    /**
     * @brief Curve with a flat cross-section aligned with the ray direction.
     * These curves may be faster to intersect but do not have a consistent
     * 3D structure across different rays. These curves are ideal for viewing
     * at a distance or curves with a small radius such as hair and fur.
     */
    MTLCurveTypeFlat = 1,
} API_AVAILABLE(macos(14.0), ios(17.0));

/**
 * @brief Basis function to use to interpolate curve control points
 */
typedef NS_ENUM(NSInteger, MTLCurveBasis) {
    /**
     * @brief B-Spline basis. Each curve segment must have 3 or 4 control
     * points. Curve segments join with C^(N - 2) continuity, where N is
     * the number of control points. The curve does not necessarily pass
     * through the control points without additional control points at the
     * beginning and end of the curve. Each curve segment can overlap
     * N-1 control points.
     */
    MTLCurveBasisBSpline = 0,
    
    /**
     * @brief Catmull-Rom basis. Curves represented in this basis can also be
     * easily converted to and from the Bézier basis. Each curve segment must
     * have 4 control points. Each index in the control point index buffer
     * points to the first of 4 consecutive control points in the control point
     * buffer.
    * 
     * The tangent at each control point is given by
     * (P_(i+1) - P_(i-1)) / 2. Therefore, the curve does not pass through the
     * first and last control point of each connected sequence of curve
     * segments. Instead, the first and last control point are used to control
     * the tangent vector at the beginning and end of the curve.
     * 
     * Curve segments join with C^1 continuity and the
     * curve passes through the control points. Each curve segment can overlap
     * 3 control points.
     */
    MTLCurveBasisCatmullRom = 1,
    
    /**
     * @brief Linear basis. The curve is made of a sequence of connected line
     * segments each with 2 control points.
     */
    MTLCurveBasisLinear = 2,

    /**
     * @brief Bezier basis
     */
    MTLCurveBasisBezier = 3,
} API_AVAILABLE(macos(14.0), ios(17.0));

/**
 * @brief Type of end cap to insert at the beginning and end of each connected
 * sequence of curve segments.
 */
typedef NS_ENUM(NSInteger, MTLCurveEndCaps) {
    /**
    * @brief No end caps
    */
    MTLCurveEndCapsNone = 0,
    
    /**
     * @brief Disk end caps
     */
    MTLCurveEndCapsDisk = 1,
    
    /**
     * @brief Spherical end caps
     */
    MTLCurveEndCapsSphere = 2,
} API_AVAILABLE(macos(14.0), ios(17.0));

/**
 * @brief Acceleration structure geometry descriptor describing geometry
 * made of curve primitives
 */
MTL_EXPORT API_AVAILABLE(macos(14.0), ios(17.0))
@interface MTLAccelerationStructureCurveGeometryDescriptor : MTLAccelerationStructureGeometryDescriptor

/**
 * @brief Buffer containing curve control points. Each control point must
 * be of the format specified by the control point format. Must not be
 * nil when the acceleration structure is built.
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> controlPointBuffer;

/**
 * @brief Control point buffer offset. Must be a multiple of the control
 * point format's element size and must be aligned to the platform's
 * buffer offset alignment.
 */
@property (nonatomic) NSUInteger controlPointBufferOffset;

/**
 * @brief Number of control points in the control point buffer
 */
@property (nonatomic) NSUInteger controlPointCount;

/**
 * @brief Stride, in bytes, between control points in the control point
 * buffer. Must be a multiple of the control point format's element size
 * and must be at least the control point format's size. Defaults to 0
 * bytes, indicating that the control points are tightly packed.
 */
@property (nonatomic) NSUInteger controlPointStride;

/**
 * @brief Format of the control points in the control point buffer.
 * Defaults to MTLAttributeFormatFloat3 (packed).
 */
@property (nonatomic) MTLAttributeFormat controlPointFormat;

/**
 * @brief Buffer containing the curve radius for each control point. Each
 * radius must be of the type specified by the radius format. Each radius
 * must be at least zero. Must not be nil when the acceleration structure
 * is built.
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> radiusBuffer;

/**
 * @brief Radius buffer offset. Must be a multiple of the radius format
 * size and must be aligned to the platform's buffer offset alignment.
 */
@property (nonatomic) NSUInteger radiusBufferOffset;

/**
 * @brief Format of the radii in the radius buffer. Defaults to 
 * MTLAttributeFormatFloat.
 */
@property (nonatomic) MTLAttributeFormat radiusFormat;

/**
 * @brief Stride, in bytes, between radii in the radius buffer. Must be
 * a multiple of the radius format size. Defaults to 0 bytes, indicating
 * that the radii are tightly packed.
 */
@property (nonatomic) NSUInteger radiusStride;

/**
 * Index buffer containing references to control points in the control
 * point buffer. Must not be nil when the acceleration structure is built.
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> indexBuffer;

/**
 * @brief Index buffer offset. Must be a multiple of the index data type
 * size and must be aligned to both the index data type's alignment and
 * the platform's buffer offset alignment.
 */
@property (nonatomic) NSUInteger indexBufferOffset;

/**
 * @brief Index type
 */
@property (nonatomic) MTLIndexType indexType;

/**
 * @brief Number of curve segments
 */
@property (nonatomic) NSUInteger segmentCount;

/**
 * @brief Number of control points per curve segment. Must be 2, 3, or 4.
 */
@property (nonatomic) NSUInteger segmentControlPointCount;

/**
 * @brief Curve type. Defaults to MTLCurveTypeRound.
 */
@property (nonatomic) MTLCurveType curveType;

/**
 * @brief Curve basis. Defaults to MTLCurveBasisBSpline.
 */
@property (nonatomic) MTLCurveBasis curveBasis;

/**
 * @brief Type of curve end caps. Defaults to MTLCurveEndCapsNone.
 */
@property (nonatomic) MTLCurveEndCaps curveEndCaps;

+ (instancetype)descriptor;

@end

/**
 * @brief Acceleration structure motion geometry descriptor describing
 * geometry made of curve primitives
 */
MTL_EXPORT API_AVAILABLE(macos(14.0), ios(17.0))
@interface MTLAccelerationStructureMotionCurveGeometryDescriptor : MTLAccelerationStructureGeometryDescriptor

/**
 * @brief Buffers containing curve control points for each keyframe.
 * Each control point must be of the format specified by the control
 * point format. Buffer offsets musts be multiples of the control
 * point format's element size and must be aligned to the platform's
 * buffer offset alignment. Must not be nil when the acceleration
 * structure is built.
 */
@property (nonatomic, copy) NSArray <MTLMotionKeyframeData *> *controlPointBuffers;

/**
 * @brief Number of control points in the control point buffers
 */
@property (nonatomic) NSUInteger controlPointCount;

/**
 * @brief Stride, in bytes, between control points in the control point
 * buffer. Must be a multiple of the control point format's element size
 * and must be at least the control point format's size. Defaults to 0
 * bytes, indicating that the control points are tightly packed.
 */
@property (nonatomic) NSUInteger controlPointStride;

/**
 * @brief Format of the control points in the control point buffer.
 * Defaults to MTLAttributeFormatFloat3 (packed).
 */
@property (nonatomic) MTLAttributeFormat controlPointFormat;

/**
 * @brief Buffers containing the curve radius for each control point for
 * each keyframe. Each radius must be of the type specified by the radius
 * format. Buffer offsets must be multiples of the radius format size
 * and must be aligned to the platform's buffer offset alignment. Each radius
 * must be at least zero. Must not be nil when the acceleration structure
 * is built.
 */
@property (nonatomic, copy) NSArray <MTLMotionKeyframeData *> *radiusBuffers;

/**
 * @brief Format of the radii in the radius buffer. Defaults to 
 * MTLAttributeFormatFloat.
 */
@property (nonatomic) MTLAttributeFormat radiusFormat;

/**
 * @brief Stride, in bytes, between radii in the radius buffer. Must be
 * a multiple of 4 bytes. Defaults to 4 bytes.
 */
@property (nonatomic) NSUInteger radiusStride;

/**
 * Index buffer containing references to control points in the control
 * point buffer. Must not be nil.
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> indexBuffer;

/**
 * @brief Index buffer offset. Must be a multiple of the index data type
 * size and must be aligned to both the index data type's alignment and
 * the platform's buffer offset alignment.
 */
@property (nonatomic) NSUInteger indexBufferOffset;

/**
 * @brief Index type
 */
@property (nonatomic) MTLIndexType indexType;

/**
 * @brief Number of curve segments
 */
@property (nonatomic) NSUInteger segmentCount;

/**
 * @brief Number of control points per curve segment. Must be 2, 3, or 4.
 */
@property (nonatomic) NSUInteger segmentControlPointCount;

/**
 * @brief Curve type. Defaults to MTLCurveTypeRound.
 */
@property (nonatomic) MTLCurveType curveType;

/**
 * @brief Curve basis. Defaults to MTLCurveBasisBSpline.
 */
@property (nonatomic) MTLCurveBasis curveBasis;

/**
 * @brief Type of curve end caps. Defaults to MTLCurveEndCapsNone.
 */
@property (nonatomic) MTLCurveEndCaps curveEndCaps;

+ (instancetype)descriptor;

@end

typedef struct {
    /**
     * @brief Transformation matrix describing how to transform the bottom-level acceleration structure.
     */
    MTLPackedFloat4x3 transformationMatrix;

    /**
     * @brief Instance options
     */
    MTLAccelerationStructureInstanceOptions options;
    
    /**
     * @brief Instance mask used to ignore geometry during ray tracing
     */
    uint32_t mask;

    /**
     * @brief Used to index into intersection function tables
     */
    uint32_t intersectionFunctionTableOffset;
    
    /**
     * @brief Acceleration structure index to use for this instance
     */
    uint32_t accelerationStructureIndex;
} MTLAccelerationStructureInstanceDescriptor API_AVAILABLE(macos(11.0), ios(14.0));

typedef struct {
    /**
     * @brief Transformation matrix describing how to transform the bottom-level acceleration structure.
     */
    MTLPackedFloat4x3 transformationMatrix;

    /**
     * @brief Instance options
     */
    MTLAccelerationStructureInstanceOptions options;

    /**
     * @brief Instance mask used to ignore geometry during ray tracing
     */
    uint32_t mask;

    /**
     * @brief Used to index into intersection function tables
     */
    uint32_t intersectionFunctionTableOffset;

    /**
     * @brief Acceleration structure index to use for this instance
     */
    uint32_t accelerationStructureIndex;

    /**
     * @brief User-assigned instance ID to help identify this instance in an
     * application-defined way
     */
    uint32_t userID;
} MTLAccelerationStructureUserIDInstanceDescriptor API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

typedef NS_ENUM(NSUInteger, MTLAccelerationStructureInstanceDescriptorType) {
    /**
     * @brief Default instance descriptor: MTLAccelerationStructureInstanceDescriptor
     */
    MTLAccelerationStructureInstanceDescriptorTypeDefault = 0,

    /**
     * @brief Instance descriptor with an added user-ID
     */
    MTLAccelerationStructureInstanceDescriptorTypeUserID = 1,

    /**
     * @brief Instance descriptor with support for motion
     */
    MTLAccelerationStructureInstanceDescriptorTypeMotion = 2,
    
    /**
     * @brief Instance descriptor with a resource handle for the instanced acceleration structure
     */
    MTLAccelerationStructureInstanceDescriptorTypeIndirect API_AVAILABLE(macos(14.0), ios(17.0)) = 3,
    
    /**
     * @brief Motion instance descriptor with a resource handle for the instanced acceleration structure.
     */
    MTLAccelerationStructureInstanceDescriptorTypeIndirectMotion API_AVAILABLE(macos(14.0), ios(17.0)) = 4,
} API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

typedef struct {
    /**
     * @brief Instance options
     */
    MTLAccelerationStructureInstanceOptions options;

    /**
     * @brief Instance mask used to ignore geometry during ray tracing
     */
    uint32_t mask;

    /**
     * @brief Used to index into intersection function tables
     */
    uint32_t intersectionFunctionTableOffset;

    /**
     * @brief Acceleration structure index to use for this instance
     */
    uint32_t accelerationStructureIndex;

    /**
     * @brief User-assigned instance ID to help identify this instance in an
     * application-defined way
     */
    uint32_t userID;

    /**
     * @brief The index of the first set of transforms describing one keyframe of the animation.
     * These transforms are stored in a separate buffer and they are uniformly distributed over
     * time time span of the motion.
     */
    uint32_t motionTransformsStartIndex;

    /**
     * @brief The count of motion transforms belonging to this motion which are stored in consecutive
     * memory addresses at the separate motionTransforms buffer.
     */
    uint32_t motionTransformsCount;
    /**
     * @brief Motion border mode describing what happens if acceleration structure is sampled
     * before motionStartTime
     */
    MTLMotionBorderMode motionStartBorderMode;

    /**
     * @brief Motion border mode describing what happens if acceleration structure is sampled
     * after motionEndTime
     */
    MTLMotionBorderMode motionEndBorderMode;

    /**
     * @brief Motion start time of this instance
     */
    float motionStartTime;

    /**
     * @brief Motion end time of this instance
     */
    float motionEndTime;
} MTLAccelerationStructureMotionInstanceDescriptor API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));


typedef struct {
    /**
     * @brief Transformation matrix describing how to transform the bottom-level acceleration structure.
     */
    MTLPackedFloat4x3 transformationMatrix;

    /**
     * @brief Instance options
     */
    MTLAccelerationStructureInstanceOptions options;

    /**
     * @brief Instance mask used to ignore geometry during ray tracing
     */
    uint32_t mask;

    /**
     * @brief Used to index into intersection function tables
     */
    uint32_t intersectionFunctionTableOffset;

    /**
     * @brief User-assigned instance ID to help identify this instance in an
     * application-defined way
     */
    uint32_t userID;

    /**
     * @brief Acceleration structure resource handle to use for this instance
     */
    MTLResourceID accelerationStructureID;
} MTLIndirectAccelerationStructureInstanceDescriptor API_AVAILABLE(macos(14.0), ios(17.0));

typedef struct {
    /**
     * @brief Instance options
     */
    MTLAccelerationStructureInstanceOptions options;

    /**
     * @brief Instance mask used to ignore geometry during ray tracing
     */
    uint32_t mask;

    /**
     * @brief Used to index into intersection function tables
     */
    uint32_t intersectionFunctionTableOffset;

    /**
     * @brief User-assigned instance ID to help identify this instance in an
     * application-defined way
     */
    uint32_t userID;
    
    /**
     * @brief Acceleration structure resource handle to use for this instance
     */
    MTLResourceID accelerationStructureID;

    /**
     * @brief The index of the first set of transforms describing one keyframe of the animation.
     * These transforms are stored in a separate buffer and they are uniformly distributed over
     * time time span of the motion.
     */
    uint32_t motionTransformsStartIndex;

    /**
     * @brief The count of motion transforms belonging to this motion which are stored in consecutive
     * memory addresses at the separate motionTransforms buffer.
     */
    uint32_t motionTransformsCount;
    /**
     * @brief Motion border mode describing what happens if acceleration structure is sampled
     * before motionStartTime
     */
    MTLMotionBorderMode motionStartBorderMode;

    /**
     * @brief Motion border mode describing what happens if acceleration structure is sampled
     * after motionEndTime
     */
    MTLMotionBorderMode motionEndBorderMode;

    /**
     * @brief Motion start time of this instance
     */
    float motionStartTime;

    /**
     * @brief Motion end time of this instance
     */
    float motionEndTime;
} MTLIndirectAccelerationStructureMotionInstanceDescriptor API_AVAILABLE(macos(14.0), ios(17.0));

typedef NS_ENUM(NSInteger, MTLTransformType) {
    /**
     * @brief A tightly packed matrix with 4 columns and 3 rows. The full transform is assumed
     * to be a 4x4 matrix with the last row being (0, 0, 0, 1).
     */
    MTLTransformTypePackedFloat4x3 = 0,
    
    /**
     * @brief A transformation represented by individual components such as translation and
     * rotation. The rotation is represented by a quaternion, allowing for correct motion
     * interpolation.
     */
    MTLTransformTypeComponent = 1,
} API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.1), visionos(2.1));

/**
 * @brief Descriptor for an instance acceleration structure
 */
MTL_EXPORT API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0))
@interface MTLInstanceAccelerationStructureDescriptor : MTLAccelerationStructureDescriptor

/**
 * @brief Buffer containing instance descriptors of the type specified by the instanceDescriptorType property
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> instanceDescriptorBuffer;

/**
 * @brief Offset into the instance descriptor buffer. Must be a multiple of 64 bytes and must be
 * aligned to the platform's buffer offset alignment.
 */
@property (nonatomic) NSUInteger instanceDescriptorBufferOffset;

/**
 * @brief Stride, in bytes, between instance descriptors in the instance descriptor buffer. Must
 * be at least the size of the instance descriptor type and must be a multiple of 4 bytes.
 * Defaults to the size of the instance descriptor type.
 */
@property (nonatomic) NSUInteger instanceDescriptorStride;

/**
 * @brief Number of instance descriptors
 */
@property (nonatomic) NSUInteger instanceCount;

/**
 * @brief Acceleration structures to be instanced
 */
@property (nonatomic, retain, nullable) NSArray <id <MTLAccelerationStructure>> * instancedAccelerationStructures;

/**
 * @brief Type of instance descriptor in the instance descriptor buffer. Defaults to
 * MTLAccelerationStructureInstanceDescriptorTypeDefault.
 */
@property (nonatomic) MTLAccelerationStructureInstanceDescriptorType instanceDescriptorType API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/**
 * @brief Buffer containing transformation information for motion
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> motionTransformBuffer API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/**
 * @brief Offset into the instance motion descriptor buffer. Must be a multiple of 64 bytes and
 * must be aligned to the platform's buffer offset alignment.
 */
@property (nonatomic) NSUInteger motionTransformBufferOffset API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/**
 * @brief Number of motion transforms
 */
@property (nonatomic) NSUInteger motionTransformCount API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/**
 * Matrix layout of the transformation matrices in the instance descriptors
 * in the instance descriptor buffer and the transformation matrices in the
 * transformation matrix buffer. Defaults to MTLMatrixLayoutColumnMajor.
 */
@property (nonatomic) MTLMatrixLayout instanceTransformationMatrixLayout API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.1), visionos(2.1));

/**
 * @brief Type of motion transforms. Defaults to MTLTransformTypePackedFloat4x3.
 */
@property (nonatomic) MTLTransformType motionTransformType API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.1), visionos(2.1));
/**
 * @brief Motion transform stride. Defaults to 0, indicating that transforms are tightly packed according to the
 * motion transform type.
 */
@property (nonatomic) NSUInteger motionTransformStride API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.1), visionos(2.1));

+ (instancetype)descriptor;

@end

/**
 * @brief Descriptor for an instance acceleration structure built with an indirected buffer of instances.
 */
MTL_EXPORT API_AVAILABLE(macos(14.0), ios(17.0))
@interface MTLIndirectInstanceAccelerationStructureDescriptor : MTLAccelerationStructureDescriptor

/**
 * @brief Buffer containing instance descriptors of the type specified by the instanceDescriptorType property
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> instanceDescriptorBuffer;

/**
 * @brief Offset into the instance descriptor buffer. Must be a multiple of 64 bytes and must be
 * aligned to the platform's buffer offset alignment.
 */
@property (nonatomic) NSUInteger instanceDescriptorBufferOffset;

/**
 * @brief Stride, in bytes, between instance descriptors in the instance descriptor buffer. Must
 * be at least the size of the instance descriptor type and must be a multiple of 4 bytes.
 * Defaults to the size of the instance descriptor type.
 */
@property (nonatomic) NSUInteger instanceDescriptorStride;

/**
 * @brief Maximum number of instance descriptors
 */
@property (nonatomic) NSUInteger maxInstanceCount;

/**
 * @brief Buffer containing the instance count as a uint32_t value. Value at build time
 * must be less than or equal to maxInstanceCount.
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> instanceCountBuffer;

/**
 * @brief Offset into the instance count buffer. Must be a multiple of 4 bytes and must be
 * aligned to the platform's buffer offset alignment.
 */
@property (nonatomic) NSUInteger instanceCountBufferOffset;

/**
 * @brief Type of instance descriptor in the instance descriptor buffer. Defaults to
 * MTLAccelerationStructureInstanceDescriptorTypeIndirect. Must be
 * MTLAccelerationStructureInstanceDescriptorTypeIndirect or
 * MTLAccelerationStructureInstanceDescriptorTypeIndirectMotion.
 */
@property (nonatomic) MTLAccelerationStructureInstanceDescriptorType instanceDescriptorType;

/**
 * @brief Buffer containing transformation information for motion
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> motionTransformBuffer;

/**
 * @brief Offset into the instance motion descriptor buffer. Must be a multiple of 64 bytes and
 * must be aligned to the platform's buffer offset alignment.
 */
@property (nonatomic) NSUInteger motionTransformBufferOffset;

/**
 * @brief Maximum number of motion transforms
 */
@property (nonatomic) NSUInteger maxMotionTransformCount;

/**
 * @brief Buffer containing the motion transform count as a uint32_t value. Value at build time
 * must be less than or equal to maxMotionTransformCount.
 */
@property (nonatomic, retain, nullable) id <MTLBuffer> motionTransformCountBuffer;

/**
 * @brief Offset into the motion transform count buffer. Must be a multiple of 4 bytes and must be
 * aligned to the platform's buffer offset alignment.
 */
@property (nonatomic) NSUInteger motionTransformCountBufferOffset;

/**
 * Matrix layout of the transformation matrices in the instance descriptors
 * in the instance descriptor buffer and the transformation matrices in the
 * transformation matrix buffer. Defaults to MTLMatrixLayoutColumnMajor.
 */
@property (nonatomic) MTLMatrixLayout instanceTransformationMatrixLayout API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.1), visionos(2.1));

/**
 * @brief Type of motion transforms. Defaults to MTLTransformTypePackedFloat4x3.
 */
@property (nonatomic) MTLTransformType motionTransformType API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.1), visionos(2.1));

/**
 * @brief Motion transform stride. Defaults to 0, indicating that transforms are tightly packed according to the
 * motion transform type.
 */
@property (nonatomic) NSUInteger motionTransformStride API_AVAILABLE(macos(15.0), ios(18.0), tvos(18.1), visionos(2.1));

+ (instancetype)descriptor;

@end

API_AVAILABLE(macos(11.0), ios(14.0), tvos(16.0))
@protocol MTLAccelerationStructure <MTLResource>

@property (nonatomic, readonly) NSUInteger size;

/*!
 @property gpuResourceID
 @abstract Handle of the GPU resource suitable for storing in an Argument Buffer
 */
@property (readonly) MTLResourceID gpuResourceID API_AVAILABLE(macos(13.0), ios(16.0));

@end


NS_ASSUME_NONNULL_END
