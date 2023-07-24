// RUN: %metal-compile main
// RUN: %metal main 2>&1 | %check

struct T {
    v2f: vec2<f32>,
    v3f: vec3<f32>,
    v4f: vec4<f32>,
    v2u: vec2<u32>,
    v3u: vec3<u32>,
    v4u: vec4<u32>,
    f: f32,
    u: u32,
}

var<private> t: T;
var<private> m2: mat2x2<f32>;
var<private> m3: mat3x3<f32>;

@group(0) @binding(0) var<storage, read_write> t1: T;
@group(0) @binding(1) var<storage, read_write> t2: T;

@group(0) @binding(2) var<storage, read_write> v1: vec3<f32>;
@group(0) @binding(3) var<storage, read_write> v2: vec3<f32>;

@group(0) @binding(4) var<storage, read_write> at1: array<T, 1>;
@group(0) @binding(5) var<storage, read_write> at2: array<T, 1>;

fn testUnpacked() -> i32
{
    _ = t.v3f * m3;
    _ = m3 * t.v3f;

    _ = t1.v2f * m2;
    _ = m2 * t1.v2f;
    return 0;
}

fn testAssignment() -> i32
{
    // packed struct
    // CHECK: local\d+ = __unpack\(parameter\d+\);
    var t = t1;
    // CHECK-NEXT: parameter\d+ = parameter\d+;
    t1 = t2;
    // CHECK-NEXT: parameter\d+ = __pack\(local\d+\);
    t2 = t;

    // array of packed structs
    // CHECK-NEXT: local\d+ = __unpack_array\(parameter\d+\);
    var at = at1;
    // CHECK-NEXT: parameter\d+ = parameter\d+;
    at1 = at2;
    // CHECK-NEXT: parameter\d+ = __pack_array\(local\d+\);
    at2 = at;

    return 0;
}

fn testFieldAccess() -> i32
{
    // CHECK: parameter\d+\.v2f\.x = parameter\d+\.v2f\.x;
    // CHECK-NEXT: parameter\d+\.v3f\.x = parameter\d+\.v3f\.x;
    // CHECK-NEXT: parameter\d+\.v4f\.x = parameter\d+\.v4f\.x;
    // CHECK-NEXT: parameter\d+\.v2u\.x = parameter\d+\.v2u\.x;
    // CHECK-NEXT: parameter\d+\.v3u\.x = parameter\d+\.v3u\.x;
    // CHECK-NEXT: parameter\d+\.v4u\.x = parameter\d+\.v4u\.x;
    // CHECK-NEXT: parameter\d+\.v2f = parameter\d+\.v2f;
    // CHECK-NEXT: parameter\d+\.v3f = float3\(parameter\d+\.v3f\);
    // CHECK-NEXT: parameter\d+\.v4f = parameter\d+\.v4f;
    // CHECK-NEXT: parameter\d+\.v2u = parameter\d+\.v2u;
    // CHECK-NEXT: parameter\d+\.v3u = uint3\(parameter\d+\.v3u\);
    // CHECK-NEXT: parameter\d+\.v4u = parameter\d+\.v4u;
    // CHECK-NEXT: parameter\d+\.f = parameter\d+\.f;
    // CHECK-NEXT: parameter\d+\.u = parameter\d+\.u;
    t.v2f.x = t1.v2f.x;
    t.v3f.x = t1.v3f.x;
    t.v4f.x = t1.v4f.x;
    t.v2u.x = t1.v2u.x;
    t.v3u.x = t1.v3u.x;
    t.v4u.x = t1.v4u.x;
    t.v2f   = t1.v2f;
    t.v3f   = t1.v3f;
    t.v4f   = t1.v4f;
    t.v2u   = t1.v2u;
    t.v3u   = t1.v3u;
    t.v4u   = t1.v4u;
    t.f     = t1.f;
    t.u     = t1.u;

    // CHECK-NEXT: parameter\d+\.v2f\.x = parameter\d+\.v2f\.x;
    // CHECK-NEXT: parameter\d+\.v3f\.x = parameter\d+\.v3f\.x;
    // CHECK-NEXT: parameter\d+\.v4f\.x = parameter\d+\.v4f\.x;
    // CHECK-NEXT: parameter\d+\.v2u\.x = parameter\d+\.v2u\.x;
    // CHECK-NEXT: parameter\d+\.v3u\.x = parameter\d+\.v3u\.x;
    // CHECK-NEXT: parameter\d+\.v4u\.x = parameter\d+\.v4u\.x;
    // CHECK-NEXT: parameter\d+\.v2f = parameter\d+\.v2f;
    // CHECK-NEXT: parameter\d+\.v3f = parameter\d+\.v3f;
    // CHECK-NEXT: parameter\d+\.v4f = parameter\d+\.v4f;
    // CHECK-NEXT: parameter\d+\.v2u = parameter\d+\.v2u;
    // CHECK-NEXT: parameter\d+\.v3u = parameter\d+\.v3u;
    // CHECK-NEXT: parameter\d+\.v4u = parameter\d+\.v4u;
    // CHECK-NEXT: parameter\d+\.f = parameter\d+\.f;
    // CHECK-NEXT: parameter\d+\.u = parameter\d+\.u;
    t1.v2f.x = t2.v2f.x;
    t1.v3f.x = t2.v3f.x;
    t1.v4f.x = t2.v4f.x;
    t1.v2u.x = t2.v2u.x;
    t1.v3u.x = t2.v3u.x;
    t1.v4u.x = t2.v4u.x;
    t1.v2f   = t2.v2f;
    t1.v3f   = t2.v3f;
    t1.v4f   = t2.v4f;
    t1.v2u   = t2.v2u;
    t1.v3u   = t2.v3u;
    t1.v4u   = t2.v4u;
    t1.f     = t2.f;
    t1.u     = t2.u;

    // CHECK-NEXT: parameter\d+\.v2f\.x = parameter\d+\.v2f\.x;
    // CHECK-NEXT: parameter\d+\.v3f\.x = parameter\d+\.v3f\.x;
    // CHECK-NEXT: parameter\d+\.v4f\.x = parameter\d+\.v4f\.x;
    // CHECK-NEXT: parameter\d+\.v2u\.x = parameter\d+\.v2u\.x;
    // CHECK-NEXT: parameter\d+\.v3u\.x = parameter\d+\.v3u\.x;
    // CHECK-NEXT: parameter\d+\.v4u\.x = parameter\d+\.v4u\.x;
    // CHECK-NEXT: parameter\d+\.v2f = parameter\d+\.v2f;
    // CHECK-NEXT: parameter\d+\.v3f = packed_float3\(parameter\d+\.v3f\);
    // CHECK-NEXT: parameter\d+\.v4f = parameter\d+\.v4f;
    // CHECK-NEXT: parameter\d+\.v2u = parameter\d+\.v2u;
    // CHECK-NEXT: parameter\d+\.v3u = packed_uint3\(parameter\d+\.v3u\);
    // CHECK-NEXT: parameter\d+\.v4u = parameter\d+\.v4u;
    // CHECK-NEXT: parameter\d+\.f = parameter\d+\.f;
    // CHECK-NEXT: parameter\d+\.u = parameter\d+\.u;
    t2.v2f.x = t.v2f.x;
    t2.v3f.x = t.v3f.x;
    t2.v4f.x = t.v4f.x;
    t2.v2u.x = t.v2u.x;
    t2.v3u.x = t.v3u.x;
    t2.v4u.x = t.v4u.x;
    t2.v2f   = t.v2f;
    t2.v3f   = t.v3f;
    t2.v4f   = t.v4f;
    t2.v2u   = t.v2u;
    t2.v3u   = t.v3u;
    t2.v4u   = t.v4u;
    t2.f     = t.f;
    t2.u     = t.u;

    return 0;
}

fn testIndexAccess() -> i32
{
    // CHECK: local\d+ = __unpack_array\(parameter\d+\);
    // CHECK-NEXT: local\d+\[0\] = __unpack\(parameter\d+\[0\]\);
    // CHECK-NEXT: parameter\d+\[0\] = parameter\d+\[0\];
    // CHECK-NEXT: parameter\d+\[0\] = __pack\(local\d+\[0\]\);
    var at = at1;
    at[0] = at1[0];
    at1[0] = at2[0];
    at2[0] = at[0];
    return 0;
}


fn testBinaryOperations() -> i32
{
    // CHECK: parameter\d+\.v2f\.x = \(2 \* parameter\d+\.v2f\.x\);
    // CHECK-NEXT: parameter\d+\.v3f\.x = \(2 \* parameter\d+\.v3f\.x\);
    // CHECK-NEXT: parameter\d+\.v4f\.x = \(2 \* parameter\d+\.v4f\.x\);
    // CHECK-NEXT: parameter\d+\.v2u\.x = \(2u \* parameter\d+\.v2u\.x\);
    // CHECK-NEXT: parameter\d+\.v3u\.x = \(2u \* parameter\d+\.v3u\.x\);
    // CHECK-NEXT: parameter\d+\.v4u\.x = \(2u \* parameter\d+\.v4u\.x\);
    // CHECK-NEXT: parameter\d+\.v2f = \(2 \* parameter\d+\.v2f\);
    // CHECK-NEXT: parameter\d+\.v3f = \(2 \* float3\(parameter\d+\.v3f\)\);
    // CHECK-NEXT: parameter\d+\.v4f = \(2 \* parameter\d+\.v4f\);
    // CHECK-NEXT: parameter\d+\.v2u = \(2u \* parameter\d+\.v2u\);
    // CHECK-NEXT: parameter\d+\.v3u = \(2u \* uint3\(parameter\d+\.v3u\)\);
    // CHECK-NEXT: parameter\d+\.v4u = \(2u \* parameter\d+\.v4u\);
    // CHECK-NEXT: parameter\d+\.f = \(2 \* parameter\d+\.f\);
    // CHECK-NEXT: parameter\d+\.u = \(2u \* parameter\d+\.u\);
    t.v2f.x = 2 * t1.v2f.x;
    t.v3f.x = 2 * t1.v3f.x;
    t.v4f.x = 2 * t1.v4f.x;
    t.v2u.x = 2 * t1.v2u.x;
    t.v3u.x = 2 * t1.v3u.x;
    t.v4u.x = 2 * t1.v4u.x;
    t.v2f   = 2 * t1.v2f;
    t.v3f   = 2 * t1.v3f;
    t.v4f   = 2 * t1.v4f;
    t.v2u   = 2 * t1.v2u;
    t.v3u   = 2 * t1.v3u;
    t.v4u   = 2 * t1.v4u;
    t.f     = 2 * t1.f;
    t.u     = 2 * t1.u;

    // CHECK-NEXT: parameter\d+\.v2f\.x = \(2 \* parameter\d+\.v2f\.x\);
    // CHECK-NEXT: parameter\d+\.v3f\.x = \(2 \* parameter\d+\.v3f\.x\);
    // CHECK-NEXT: parameter\d+\.v4f\.x = \(2 \* parameter\d+\.v4f\.x\);
    // CHECK-NEXT: parameter\d+\.v2u\.x = \(2u \* parameter\d+\.v2u\.x\);
    // CHECK-NEXT: parameter\d+\.v3u\.x = \(2u \* parameter\d+\.v3u\.x\);
    // CHECK-NEXT: parameter\d+\.v4u\.x = \(2u \* parameter\d+\.v4u\.x\);
    // CHECK-NEXT: parameter\d+\.v2f = \(2 \* parameter\d+\.v2f\);
    // CHECK-NEXT: parameter\d+\.v3f = packed_float3\(\(2 \* float3\(parameter\d+\.v3f\)\)\);
    // CHECK-NEXT: parameter\d+\.v4f = \(2 \* parameter\d+\.v4f\);
    // CHECK-NEXT: parameter\d+\.v2u = \(2u \* parameter\d+\.v2u\);
    // CHECK-NEXT: parameter\d+\.v3u = packed_uint3\(\(2u \* uint3\(parameter\d+\.v3u\)\)\);
    // CHECK-NEXT: parameter\d+\.v4u = \(2u \* parameter\d+\.v4u\);
    // CHECK-NEXT: parameter\d+\.f = \(2 \* parameter\d+\.f\);
    // CHECK-NEXT: parameter\d+\.u = \(2u \* parameter\d+\.u\);
    t1.v2f.x = 2 * t2.v2f.x;
    t1.v3f.x = 2 * t2.v3f.x;
    t1.v4f.x = 2 * t2.v4f.x;
    t1.v2u.x = 2 * t2.v2u.x;
    t1.v3u.x = 2 * t2.v3u.x;
    t1.v4u.x = 2 * t2.v4u.x;
    t1.v2f   = 2 * t2.v2f;
    t1.v3f   = 2 * t2.v3f;
    t1.v4f   = 2 * t2.v4f;
    t1.v2u   = 2 * t2.v2u;
    t1.v3u   = 2 * t2.v3u;
    t1.v4u   = 2 * t2.v4u;
    t1.f     = 2 * t2.f;
    t1.u     = 2 * t2.u;

    // CHECK-NEXT: parameter\d+\.v2f\.x = \(2 \* parameter\d+\.v2f\.x\);
    // CHECK-NEXT: parameter\d+\.v3f\.x = \(2 \* parameter\d+\.v3f\.x\);
    // CHECK-NEXT: parameter\d+\.v4f\.x = \(2 \* parameter\d+\.v4f\.x\);
    // CHECK-NEXT: parameter\d+\.v2u\.x = \(2u \* parameter\d+\.v2u\.x\);
    // CHECK-NEXT: parameter\d+\.v3u\.x = \(2u \* parameter\d+\.v3u\.x\);
    // CHECK-NEXT: parameter\d+\.v4u\.x = \(2u \* parameter\d+\.v4u\.x\);
    // CHECK-NEXT: parameter\d+\.v2f = \(2 \* parameter\d+\.v2f\);
    // CHECK-NEXT: parameter\d+\.v3f = packed_float3\(\(2 \* parameter\d+\.v3f\)\);
    // CHECK-NEXT: parameter\d+\.v4f = \(2 \* parameter\d+\.v4f\);
    // CHECK-NEXT: parameter\d+\.v2u = \(2u \* parameter\d+\.v2u\);
    // CHECK-NEXT: parameter\d+\.v3u = packed_uint3\(\(2u \* parameter\d+\.v3u\)\);
    // CHECK-NEXT: parameter\d+\.v4u = \(2u \* parameter\d+\.v4u\);
    // CHECK-NEXT: parameter\d+\.f = \(2 \* parameter\d+\.f\);
    // CHECK-NEXT: parameter\d+\.u = \(2u \* parameter\d+\.u\);
    t2.v2f.x = 2 * t.v2f.x;
    t2.v3f.x = 2 * t.v3f.x;
    t2.v4f.x = 2 * t.v4f.x;
    t2.v2u.x = 2 * t.v2u.x;
    t2.v3u.x = 2 * t.v3u.x;
    t2.v4u.x = 2 * t.v4u.x;
    t2.v2f   = 2 * t.v2f;
    t2.v3f   = 2 * t.v3f;
    t2.v4f   = 2 * t.v4f;
    t2.v2u   = 2 * t.v2u;
    t2.v3u   = 2 * t.v3u;
    t2.v4u   = 2 * t.v4u;
    t2.f     = 2 * t.f;
    t2.u     = 2 * t.u;

    return 0;
}

fn testUnaryOperations() -> i32
{
    // CHECK: parameter\d+\.v2f\.x = -parameter\d+\.v2f\.x;
    // CHECK-NEXT: parameter\d+\.v3f\.x = -parameter\d+\.v3f\.x;
    // CHECK-NEXT: parameter\d+\.v4f\.x = -parameter\d+\.v4f\.x;
    // CHECK-NEXT: parameter\d+\.v2f = -parameter\d+\.v2f;
    // CHECK-NEXT: parameter\d+\.v3f = -float3\(parameter\d+\.v3f\);
    // CHECK-NEXT: parameter\d+\.v4f = -parameter\d+\.v4f;
    // CHECK-NEXT: parameter\d+\.f = -parameter\d+\.f;
    t.v2f.x = -t1.v2f.x;
    t.v3f.x = -t1.v3f.x;
    t.v4f.x = -t1.v4f.x;
    t.v2f   = -t1.v2f;
    t.v3f   = -t1.v3f;
    t.v4f   = -t1.v4f;
    t.f     = -t1.f;

    // CHECK-NEXT: parameter\d+\.v2f\.x = -parameter\d+\.v2f\.x;
    // CHECK-NEXT: parameter\d+\.v3f\.x = -parameter\d+\.v3f\.x;
    // CHECK-NEXT: parameter\d+\.v4f\.x = -parameter\d+\.v4f\.x;
    // CHECK-NEXT: parameter\d+\.v2f = -parameter\d+\.v2f;
    // CHECK-NEXT: parameter\d+\.v3f = packed_float3\(-float3\(parameter\d+\.v3f\)\);
    // CHECK-NEXT: parameter\d+\.v4f = -parameter\d+\.v4f;
    // CHECK-NEXT: parameter\d+\.f = -parameter\d+\.f;
    t1.v2f.x = -t2.v2f.x;
    t1.v3f.x = -t2.v3f.x;
    t1.v4f.x = -t2.v4f.x;
    t1.v2f   = -t2.v2f;
    t1.v3f   = -t2.v3f;
    t1.v4f   = -t2.v4f;
    t1.f     = -t2.f;

    // CHECK-NEXT: parameter\d+\.v2f\.x = -parameter\d+\.v2f\.x;
    // CHECK-NEXT: parameter\d+\.v3f\.x = -parameter\d+\.v3f\.x;
    // CHECK-NEXT: parameter\d+\.v4f\.x = -parameter\d+\.v4f\.x;
    // CHECK-NEXT: parameter\d+\.v2f = -parameter\d+\.v2f;
    // CHECK-NEXT: parameter\d+\.v3f = packed_float3\(-parameter\d+\.v3f\);
    // CHECK-NEXT: parameter\d+\.v4f = -parameter\d+\.v4f;
    // CHECK-NEXT: parameter\d+\.f = -parameter\d+\.f;
    t2.v2f.x = -t.v2f.x;
    t2.v3f.x = -t.v3f.x;
    t2.v4f.x = -t.v4f.x;
    t2.v2f   = -t.v2f;
    t2.v3f   = -t.v3f;
    t2.v4f   = -t.v4f;
    t2.f     = -t.f;

    return 0;
}

fn testCall() -> i32
{
    // CHECK: parameter\d+\.v2f\.x = abs\(parameter\d+\.v2f\.x\);
    // CHECK-NEXT: parameter\d+\.v3f\.x = abs\(parameter\d+\.v3f\.x\);
    // CHECK-NEXT: parameter\d+\.v4f\.x = abs\(parameter\d+\.v4f\.x\);
    // CHECK-NEXT: parameter\d+\.v2u\.x = abs\(parameter\d+\.v2u\.x\);
    // CHECK-NEXT: parameter\d+\.v3u\.x = abs\(parameter\d+\.v3u\.x\);
    // CHECK-NEXT: parameter\d+\.v4u\.x = abs\(parameter\d+\.v4u\.x\);
    // CHECK-NEXT: parameter\d+\.v2f = abs\(parameter\d+\.v2f\);
    // CHECK-NEXT: parameter\d+\.v3f = abs\(float3\(parameter\d+\.v3f\)\);
    // CHECK-NEXT: parameter\d+\.v4f = abs\(parameter\d+\.v4f\);
    // CHECK-NEXT: parameter\d+\.v2u = abs\(parameter\d+\.v2u\);
    // CHECK-NEXT: parameter\d+\.v3u = abs\(uint3\(parameter\d+\.v3u\)\);
    // CHECK-NEXT: parameter\d+\.v4u = abs\(parameter\d+\.v4u\);
    // CHECK-NEXT: parameter\d+\.f = abs\(parameter\d+\.f\);
    // CHECK-NEXT: parameter\d+\.u = abs\(parameter\d+\.u\);
    t.v2f.x = abs(t1.v2f.x);
    t.v3f.x = abs(t1.v3f.x);
    t.v4f.x = abs(t1.v4f.x);
    t.v2u.x = abs(t1.v2u.x);
    t.v3u.x = abs(t1.v3u.x);
    t.v4u.x = abs(t1.v4u.x);
    t.v2f   = abs(t1.v2f);
    t.v3f   = abs(t1.v3f);
    t.v4f   = abs(t1.v4f);
    t.v2u   = abs(t1.v2u);
    t.v3u   = abs(t1.v3u);
    t.v4u   = abs(t1.v4u);
    t.f     = abs(t1.f);
    t.u     = abs(t1.u);

    // CHECK-NEXT: parameter\d+\.v2f\.x = abs\(parameter\d+\.v2f\.x\);
    // CHECK-NEXT: parameter\d+\.v3f\.x = abs\(parameter\d+\.v3f\.x\);
    // CHECK-NEXT: parameter\d+\.v4f\.x = abs\(parameter\d+\.v4f\.x\);
    // CHECK-NEXT: parameter\d+\.v2u\.x = abs\(parameter\d+\.v2u\.x\);
    // CHECK-NEXT: parameter\d+\.v3u\.x = abs\(parameter\d+\.v3u\.x\);
    // CHECK-NEXT: parameter\d+\.v4u\.x = abs\(parameter\d+\.v4u\.x\);
    // CHECK-NEXT: parameter\d+\.v2f = abs\(parameter\d+\.v2f\);
    // CHECK-NEXT: parameter\d+\.v3f = packed_float3\(abs\(float3\(parameter\d+\.v3f\)\)\);
    // CHECK-NEXT: parameter\d+\.v4f = abs\(parameter\d+\.v4f\);
    // CHECK-NEXT: parameter\d+\.v2u = abs\(parameter\d+\.v2u\);
    // CHECK-NEXT: parameter\d+\.v3u = packed_uint3\(abs\(uint3\(parameter\d+\.v3u\)\)\);
    // CHECK-NEXT: parameter\d+\.v4u = abs\(parameter\d+\.v4u\);
    // CHECK-NEXT: parameter\d+\.f = abs\(parameter\d+\.f\);
    // CHECK-NEXT: parameter\d+\.u = abs\(parameter\d+\.u\);
    t1.v2f.x = abs(t2.v2f.x);
    t1.v3f.x = abs(t2.v3f.x);
    t1.v4f.x = abs(t2.v4f.x);
    t1.v2u.x = abs(t2.v2u.x);
    t1.v3u.x = abs(t2.v3u.x);
    t1.v4u.x = abs(t2.v4u.x);
    t1.v2f   = abs(t2.v2f);
    t1.v3f   = abs(t2.v3f);
    t1.v4f   = abs(t2.v4f);
    t1.v2u   = abs(t2.v2u);
    t1.v3u   = abs(t2.v3u);
    t1.v4u   = abs(t2.v4u);
    t1.f     = abs(t2.f);
    t1.u     = abs(t2.u);

    // CHECK-NEXT: parameter\d+\.v2f\.x = abs\(parameter\d+\.v2f\.x\);
    // CHECK-NEXT: parameter\d+\.v3f\.x = abs\(parameter\d+\.v3f\.x\);
    // CHECK-NEXT: parameter\d+\.v4f\.x = abs\(parameter\d+\.v4f\.x\);
    // CHECK-NEXT: parameter\d+\.v2u\.x = abs\(parameter\d+\.v2u\.x\);
    // CHECK-NEXT: parameter\d+\.v3u\.x = abs\(parameter\d+\.v3u\.x\);
    // CHECK-NEXT: parameter\d+\.v4u\.x = abs\(parameter\d+\.v4u\.x\);
    // CHECK-NEXT: parameter\d+\.v2f = abs\(parameter\d+\.v2f\);
    // CHECK-NEXT: parameter\d+\.v3f = packed_float3\(abs\(parameter\d+\.v3f\)\);
    // CHECK-NEXT: parameter\d+\.v4f = abs\(parameter\d+\.v4f\);
    // CHECK-NEXT: parameter\d+\.v2u = abs\(parameter\d+\.v2u\);
    // CHECK-NEXT: parameter\d+\.v3u = packed_uint3\(abs\(parameter\d+\.v3u\)\);
    // CHECK-NEXT: parameter\d+\.v4u = abs\(parameter\d+\.v4u\);
    // CHECK-NEXT: parameter\d+\.f = abs\(parameter\d+\.f\);
    // CHECK-NEXT: parameter\d+\.u = abs\(parameter\d+\.u\);
    t2.v2f.x = abs(t.v2f.x);
    t2.v3f.x = abs(t.v3f.x);
    t2.v4f.x = abs(t.v4f.x);
    t2.v2u.x = abs(t.v2u.x);
    t2.v3u.x = abs(t.v3u.x);
    t2.v4u.x = abs(t.v4u.x);
    t2.v2f   = abs(t.v2f);
    t2.v3f   = abs(t.v3f);
    t2.v4f   = abs(t.v4f);
    t2.v2u   = abs(t.v2u);
    t2.v3u   = abs(t.v3u);
    t2.v4u   = abs(t.v4u);
    t2.f     = abs(t.f);
    t2.u     = abs(t.u);

    return 0;
}

@compute @workgroup_size(1)
fn main()
{
    _ = testUnpacked();
    _ = testAssignment();
    _ = testFieldAccess();
    _ = testIndexAccess();
    _ = testBinaryOperations();
    _ = testUnaryOperations();
    _ = testCall();
}
