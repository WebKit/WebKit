#ifdef __METAL_VERSION__
#import <metal_stdlib>

typedef metal::packed_float3 MTLPackedFloat3;
typedef metal::packed_float4 MTLPackedFloatQuaternion;
#else
#include <math.h>
#include <stdint.h>

#ifndef _WIN32
#import <Metal/MTLDefines.h>
#else
#include <Metal/MTLDefines.h>
#endif // _WIN32

#include <Metal/MTL4BufferRange.h>

typedef struct _MTLPackedFloat3 {
    union {
        struct {
            float x;
            float y;
            float z;
        };
        float elements[3];
    };

#ifdef __cplusplus
    _MTLPackedFloat3()
        : x(0.0f), y(0.0f), z(0.0f)
    {
    }

    _MTLPackedFloat3(float x, float y, float z)
        : x(x), y(y), z(z)
    {
    }

    float & operator[](int idx) {
        return elements[idx];
    }

    const float & operator[](int idx) const {
        return elements[idx];
    }
#endif
} MTLPackedFloat3;

MTL_INLINE MTLPackedFloat3 MTLPackedFloat3Make(float x, float y, float z)
{
    MTLPackedFloat3 packedFloat3;
    packedFloat3.x = x;
    packedFloat3.y = y;
    packedFloat3.z = z;
    return packedFloat3;
}

typedef struct MTLPackedFloatQuaternion {
    float x;
    float y;
    float z;
    float w;

#ifdef __cplusplus
    MTLPackedFloatQuaternion()
        : x(0.0f), y(0.0f), z(0.0f), w(1.0f)
    {
    }

    MTLPackedFloatQuaternion(float x, float y, float z, float w)
        : x(x), y(y), z(z), w(w)
    {
    }

    float & operator[](int idx) {
        float *elements = &x;

        return elements[idx];
    }

    const float & operator[](int idx) const {
        const float *elements = &x;

        return elements[idx];
    }
#endif
} MTLPackedFloatQuaternion;

MTL_INLINE MTLPackedFloatQuaternion MTLPackedFloatQuaternionMake(float x, float y, float z, float w)
{
    MTLPackedFloatQuaternion packedQuaternion;
    packedQuaternion.x = x;
    packedQuaternion.y = y;
    packedQuaternion.z = z;
    packedQuaternion.w = w;
    return packedQuaternion;
}

#endif

typedef struct _MTLPackedFloat4x3 {
    MTLPackedFloat3 columns[4];

#ifdef __cplusplus
    _MTLPackedFloat4x3() {
        columns[0] = MTLPackedFloat3(0.0f, 0.0f, 0.0f);
        columns[1] = MTLPackedFloat3(0.0f, 0.0f, 0.0f);
        columns[2] = MTLPackedFloat3(0.0f, 0.0f, 0.0f);
        columns[3] = MTLPackedFloat3(0.0f, 0.0f, 0.0f);
    }
    
    _MTLPackedFloat4x3(MTLPackedFloat3 column0, MTLPackedFloat3 column1, MTLPackedFloat3 column2, MTLPackedFloat3 column3) {
        columns[0] = column0;
        columns[1] = column1;
        columns[2] = column2;
        columns[3] = column3;
    }
    
#ifndef __METAL_VERSION__
    MTLPackedFloat3 & operator[](int idx) {
        return columns[idx];
    }

    const MTLPackedFloat3 & operator[](int idx) const {
        return columns[idx];
    }
#else
    thread MTLPackedFloat3 & operator[](int idx) thread {
        return columns[idx];
    }

    const thread MTLPackedFloat3 & operator[](int idx) const thread {
        return columns[idx];
    }

    device MTLPackedFloat3 & operator[](int idx) device {
        return columns[idx];
    }

    const device MTLPackedFloat3 & operator[](int idx) const device {
        return columns[idx];
    }

    const constant MTLPackedFloat3 & operator[](int idx) const constant {
        return columns[idx];
    }
#endif
#endif
} MTLPackedFloat4x3;

/**
 * @brief An axis aligned bounding box with a min and max point
 */
typedef struct _MTLAxisAlignedBoundingBox {
    /**
     * @brief Minimum point
     */
    MTLPackedFloat3 min;

    /**
     * @brief Maximum point
     */
    MTLPackedFloat3 max;

#ifdef __cplusplus
    _MTLAxisAlignedBoundingBox()
        : min(INFINITY, INFINITY, INFINITY),
          max(-INFINITY, -INFINITY, -INFINITY)
    {
    }
    
#ifdef __METAL_VERSION__
    _MTLAxisAlignedBoundingBox() threadgroup = default;
#endif
    

    _MTLAxisAlignedBoundingBox(MTLPackedFloat3 p)
        : min(p),
          max(p)
    {
    }

    _MTLAxisAlignedBoundingBox(MTLPackedFloat3 min, MTLPackedFloat3 max)
        : min(min),
          max(max)
    {
    }
#endif
} MTLAxisAlignedBoundingBox;

/**
 * @brief A transformation represented by individual components such as translation and
 * rotation. The rotation is represented by a quaternion, allowing for correct motion
 * interpolation.
 */
typedef struct {
    /**
     * @brief The scale of the instance applied before rotation alongside shear and pivot
     */
    MTLPackedFloat3 scale;
    
    /**
     * @brief The shear of the instance applied before rotation alongside scale and pivot
     */
    MTLPackedFloat3 shear;
    
    /**
     * @brief Translation applied before rotation alongside scale and shear. Allows
     * rotation to pivot around a point.
     */
    MTLPackedFloat3 pivot;
    
    /**
     * @brief The rotation of the instance as a normalized quaternion. Applied after scale,
     * shear, and pivot and before translation
     */
    MTLPackedFloatQuaternion rotation;

    /**
     * @brief The translation of the instance. Applied after rotation. Typically contains
     * the composition of object translation and the inverse of the pivot translation.
     */
    MTLPackedFloat3 translation;
} MTLComponentTransform;
