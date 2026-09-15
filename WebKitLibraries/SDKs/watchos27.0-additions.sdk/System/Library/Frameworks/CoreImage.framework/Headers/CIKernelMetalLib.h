//
//  Extensions to Metal Shading Language for Core Image Kernels
//
//  Copyright (c) 2017 Apple, Inc. All rights reserved.
//

#ifndef CIKERNELMETALLIB_H
#define CIKERNELMETALLIB_H

#if __METAL_VERSION__

#ifndef __CIKERNEL_METAL_VERSION__ // if not explicitly defined already
    #if !defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)  && \
        !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
        !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__)     && \
        !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__)  && \
        !defined(__ENVIRONMENT_OS_VERSION_MIN_REQUIRED__)
        #define __CIKERNEL_METAL_VERSION__ 200 // the includer of this didn't specify a MIN_REQUIRED compatibility
    #elif (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__  >= 160000 || \
           __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ >= 190000 || \
           __ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__     >= 190000 || \
           __ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__  >= 110000 || \
           (__is_target_os(visionos) && __ENVIRONMENT_OS_VERSION_MIN_REQUIRED__==3000))
        #define __CIKERNEL_METAL_VERSION__ 400 // compatible w/ macOS 16/iOS 19/tvOS 19 or later
    #elif (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__  >= 120000  || \
           __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ >= 150000  || \
           __ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__     >= 150000  || \
           __ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__  >=  80000  || \
           __is_target_os(visionos))
        #define __CIKERNEL_METAL_VERSION__ 300
    #elif (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__  >= 101400 || \
           __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__ >= 120000 || \
           __ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__     >= 120000 || \
           __ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__  >=  50000)
        #define __CIKERNEL_METAL_VERSION__ 200 // compatible w/ macOS 10.14/iOS 12.0/tvOS 12.0 or later
    #else
        #define __CIKERNEL_METAL_VERSION__ 100 // compatible w/ macOS 10.13/iOS 11.0/tvOS 11.0 or later
    #endif
#endif


#if __METAL_CIKERNEL__ // If Metal CIKernel is compiled with -fcikernel
#if  __CIKERNEL_METAL_VERSION__ >= 200
    #undef __CIKERNEL_METAL_VERSION__
    #define __CIKERNEL_METAL_VERSION__ 200
#endif
#endif

#if __CIKERNEL_METAL_VERSION__ >= 200
#define __CIKERNEL_METAL_SUPPORTS_HALF__ 1
#define __CIKERNEL_METAL_SUPPORTS_GATHER__ 1
#define __CIKERNEL_METAL_SUPPORTS_GROUP_DESTINATION__ 1
#endif

#include <metal_stdlib>

namespace coreimage
{
    using namespace metal;
    
    typedef float4 sample_t;
    
#ifdef __CIKERNEL_METAL_SUPPORTS_HALF__
    typedef half4 sample_h;
#endif
    
    //MARK: - Sampler
    
    typedef struct
    #if __CIKERNEL_METAL_VERSION__ >= 300
    Sampler
    #endif
    {
        // Returns the pixel value produced from sampler at the position p, where p is specified in sampler space.
        float4 sample(float2 p) const thread;
        
        // Returns the position in the coordinate space of the source that is associated with the position defined in working-space coordinates.
        float2 transform(float2 p) const thread;
        
        // Returns the position, in sampler space, of the sampler that is associated with the current output pixel (that is, after any transformation matrix
        // associated with sampler is applied). The sample space refers to the coordinate space of that you are texturing from. Note that if your source data is tiled,
        // the sample coordinate will have an offset (dx/dy). You can convert a destination location to the sampler location using the transform function.
        float2 coord() const thread;
        
        // Returns the extent of the sampler in world coordinates, as a four-element vector [x, y, width, height].
        
        
        float4 extent() const thread;
        inline float2 origin() const thread { return extent().xy; }
        inline float2 size() const thread { return extent().zw; }

#ifdef __CIKERNEL_METAL_SUPPORTS_GATHER__
        // Returns four samples (placed in CCW order starting with sample to the lower left) that would be used for bilinear interpolation when sampling at the position p,
        // where p is specified in sampler space.
        float4 gatherX(float2 p) const thread;
        float4 gatherY(float2 p) const thread;
        float4 gatherZ(float2 p) const thread;
        float4 gatherW(float2 p) const thread;

        // Returns four (unordered) samples that would be used for bilinear interpolation when sampling at the position p, where p is specified in sampler space.
        float4 gatherX_unordered(float2 p) const thread;
        float4 gatherY_unordered(float2 p) const thread;
        float4 gatherZ_unordered(float2 p) const thread;
        float4 gatherW_unordered(float2 p) const thread;
#endif
        
    private:
        #if __CIKERNEL_METAL_VERSION__ >= 300
        friend Sampler make_sampler(texture2d<float, access::sample> t, metal::sampler s, constant float4x4& m, float2 dc );
        Sampler(texture2d<float, access::sample> t_, metal::sampler s_, constant float4x4& m_, float2 dc_) thread
            : t(t_), s(s_),m(m_), dc(dc_) {}
        #endif
        
        texture2d<float, access::sample> t;
        metal::sampler s;
        constant float4x4& m;
        float2 dc;
        
    } sampler;
    
#ifdef __CIKERNEL_METAL_SUPPORTS_HALF__
    typedef struct
    #if __CIKERNEL_METAL_VERSION__ >= 300
    Sampler_h
    #endif
    {
        // Returns the pixel value produced from sampler at the position p, where p is specified in sampler space.
        
        half4 sample(float2 p) const thread;
        // Returns the position in the coordinate space of the source that is associated with the position defined in working-space coordinates.
        
        float2 transform(float2 p) const thread;
        // Returns the position, in sampler space, of the sampler that is associated with the current output pixel (that is, after any transformation matrix
        // associated with sampler is applied). The sample space refers to the coordinate space of that you are texturing from. Note that if your source data is tiled,
        // the sample coordinate will have an offset (dx/dy). You can convert a destination location to the sampler location using the transform function.
        
        float2 coord() const thread;
        // Returns the extent of the sampler in world coordinates, as a four-element vector [x, y, width, height].
        
        
        float4 extent() const thread;
        inline float2 origin() const thread { return extent().xy; }
        inline float2 size() const thread { return extent().zw; }

        // Returns four samples (placed in CCW order starting with sample to the lower left) that would be used for bilinear interpolation when sampling at the position p,
        // where p is specified in sampler space.
        
        half4 gatherX(float2 p) const thread;
        half4 gatherY(float2 p) const thread;
        half4 gatherZ(float2 p) const thread;
        half4 gatherW(float2 p) const thread;

        // Returns four (unordered) samples that would be used for bilinear interpolation when sampling at the position p, where p is specified in sampler space.
        half4 gatherX_unordered(float2 p) const thread;
        half4 gatherY_unordered(float2 p) const thread;
        half4 gatherZ_unordered(float2 p) const thread;
        half4 gatherW_unordered(float2 p) const thread;
        
    private:
        #if __CIKERNEL_METAL_VERSION__ >= 300
        friend Sampler_h make_sampler(texture2d<half, access::sample> t, metal::sampler s, constant float4x4& m, float2 dc );
        Sampler_h(texture2d<half, access::sample> t_, metal::sampler s_, constant float4x4& m_, float2 dc_) thread
            : t(t_), s(s_),m(m_), dc(dc_) {}
        #endif
        
        texture2d<half, access::sample> t;
        metal::sampler s;
        constant float4x4& m;
        float2 dc;
        
    } sampler_h;
#endif
    
    // Global equivalents of sampler struct member functions, to facilitate porting of non-Metal legacy CI kernels.
    
    // Equivalent to src.sample(p)
    float4 sample (sampler src, float2 p);
    
    // Equivalent to src.transform(p)
    float2 samplerTransform (sampler src, float2 p);
    
    // Equivalent to src.coord()
    float2 samplerCoord (sampler src);
    
    // Equivalent to src.extent()
    float4 samplerExtent (sampler src);
    
    // Equivalent to src.origin()
    float2 samplerOrigin (sampler src);
    
    // Equivalent to src.size()
    float2 samplerSize (sampler src);
    
    //MARK: - Destination
    
    typedef struct Destination
    {
        // Returns the position, in working space coordinates, of the pixel currently being computed.
        // The destination space refers to the coordinate space of the image you are rendering.
        inline float2 coord() const thread { return c; }
        
    private:
        float2 c;
        
    } destination;
    
#ifdef __CIKERNEL_METAL_SUPPORTS_GROUP_DESTINATION__
    namespace group
    {
        typedef struct
        #if __CIKERNEL_METAL_VERSION__ >= 300
        Destination
        #endif
        {
            // Returns the position, in working space coordinates, of the pixel currently being computed.
            // The destination space refers to the coordinate space of the image you are rendering.
            
            inline float2 coord() const thread { return c; }
            // Writes 4 color values to the destination texture for the current 2x2 group of pixels.
            void write(float4 v0, float4 v1, float4 v2, float4 v3) thread;
            
        private:
            #if __CIKERNEL_METAL_VERSION__ >= 300
            friend group::Destination make_destination (float2 c, uint2 gid, float4 r, float4x4 m, metal::texture2d<float, access::write> t );
            Destination(float2 c_, uint2 gid_, float4 r_, float4x4 m_, texture2d<float, access::write> t_) thread : c(c_), gid(gid_), r(r_), m(m_), t(t_) {}
            #endif
            
            float2 c;
            uint2 gid;
            float4 r;
            float4x4 m;
            texture2d<float, access::write> t;
            
        } __attribute__((packed)) destination;
        
        typedef struct
        #if __CIKERNEL_METAL_VERSION__ >= 300
        Destination_h
        #endif
        {
            // Returns the position, in working space coordinates, of the pixel currently being computed.
            // The destination space refers to the coordinate space of the image you are rendering.
            
            inline float2 coord() const thread { return c; }
            // Writes 4 color values to the destination texture for the current 2x2 group of pixels.
            void write(half4 v0, half4 v1, half4 v2, half4 v3) thread;
            
        private:
            #if __CIKERNEL_METAL_VERSION__ >= 300
            friend group::Destination_h make_destination (float2 c, uint2 gid, float4 r, float4x4 m, metal::texture2d<half, access::write> t );
            Destination_h(float2 c_, uint2 gid_, float4 r_, float4x4 m_, texture2d<half, access::write> t_) thread : c(c_), gid(gid_), r(r_), m(m_), t(t_) {}
            #endif
            
            float2 c;
            uint2 gid;
            float4 r;
            float4x4 m;
            texture2d<half, access::write> t;
            
        } __attribute__((packed)) destination_h;
    }
#endif
    
    //MARK: - Common Functions
    
    float4 premultiply(float4 s);
    float4 unpremultiply(float4 s);
    
    float3 srgb_to_linear(float3 s);
    float4 srgb_to_linear(float4 s);
        
    float3 linear_to_srgb(float3 s);
    float4 linear_to_srgb(float4 s);
    
#ifdef __CIKERNEL_METAL_SUPPORTS_HALF__
    half4 premultiply(half4 s);
    half4 unpremultiply(half4 s);

    half3 srgb_to_linear(half3 s);
    half4 srgb_to_linear(half4 s);
        
    half3 linear_to_srgb(half3 s);
    half4 linear_to_srgb(half4 s);
#endif
    
    //MARK: - Relational Functions
    
    // for each component, returns x < 0 ? y : z
    float  compare(float  x, float  y, float  z);
    float2 compare(float2 x, float2 y, float2 z);
    float3 compare(float3 x, float3 y, float3 z);
    float4 compare(float4 x, float4 y, float4 z);
    
#ifdef __CIKERNEL_METAL_SUPPORTS_HALF__
    half   compare(half   x, half   y, half   z);
    half2  compare(half2  x, half2  y, half2  z);
    half3  compare(half3  x, half3  y, half3  z);
    half4  compare(half4  x, half4  y, half4  z);
#endif
    
    //MARK: - Math Functions
    
    float2 cossin(float x);
    float2 sincos(float x);
}

namespace ci = coreimage;

#endif /* __METAL_VERSION__ */

#endif /* CIKERNELMETALLIB_H */
