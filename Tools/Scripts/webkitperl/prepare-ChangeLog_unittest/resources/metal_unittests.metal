/*
 * Copyright (C) 2026 Apple Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <metal_stdlib>

using namespace metal;

constant bool has_color [[ function_constant (0) ]];
constant float weights[] = { 0.25, 0.5, 0.25 };

struct VertexIn {
    float2 position [[ attribute (0) ]];
    float2 texcoord [[ attribute (1) ]];
};

struct VertexOut {
    float4 position [[ position ]];
    float2 texcoord;
    half4 color [[ function_constant (has_color) ]];
};

static inline float square(float x)
{
    return x * x;
}

static always_inline float2 rotate(float2 v, float angle)
{
    return float2(v.x * cos(angle) - v.y * sin(angle), v.x * sin(angle) + v.y * cos(angle));
}

template<typename T>
T clampToUnit(T value)
{
    return clamp(value, T(0), T(1));
}

template <int taps>
static always_inline float sumWeights(const constant float* w)
{
    float sum = 0;
    for (int i = 0; i < taps; ++i)
        sum += w[i];
    return sum;
}

struct Transform {
    float2x2 matrix;
    float2 offset;

    float2 apply(float2 p) const
    {
        return matrix * p + offset;
    }

    void translate(float2 delta)
    {
        offset += delta;
    }
};

template <typename lowp>
struct Shader {
    template <int count>
    struct Kernel {
        float weight[count];

        float tap(int i) const
        {
            return weight[i];
        }
    };

    static always_inline lowp blend(lowp a, lowp b, float t)
    {
        return mix(a, b, lowp(t));
    }
};

vertex VertexOut basic_vertex(VertexIn in [[ stage_in ]],
                              constant Transform& transform [[ buffer (1) ]],
                              uint vid [[ vertex_id ]])
{
    VertexOut out;
    out.position = float4(transform.apply(in.position), 0, 1);
    out.texcoord = in.texcoord;
    return out;
}

fragment half4 basic_fragment(
    VertexOut in [[ stage_in ]],
    texture2d<half> tex [[ texture (0) ]],
    sampler samp [[ sampler (0) ]])
{
    return tex.sample(samp, in.texcoord);
}

kernel void clear_buffer(device float* output [[ buffer (0) ]],
                         uint gid [[ thread_position_in_grid ]])
{
    output[gid] = 0;
}

[[ max_total_threads_per_threadgroup (64) ]] kernel void
blur_kernel(texture2d<float, access::read> input [[ texture (0) ]],
            texture2d<float, access::write> output [[ texture (1) ]],
            threadgroup float* scratch [[ threadgroup (0) ]],
            uint2 gid [[ thread_position_in_grid ]])
{
    thread float4& value = *(thread float4*)scratch;
    value = input.read(gid);
    output.write(value, gid);
}

#define DEFINE_FILL_FRAGMENT(NAME, COLOR)      \
fragment half4 fill_##NAME##_fragment()        \
{                                              \
    return COLOR;                              \
}

DEFINE_FILL_FRAGMENT(red, half4(1, 0, 0, 1))
DEFINE_FILL_FRAGMENT(green, half4(0, 1, 0, 1))

namespace Effects {

float vignette(float2 uv)
{
    return 1 - square(length(uv - 0.5));
}

} // namespace Effects
