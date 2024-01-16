// RUN: %metal-compile main
// RUN: %metal main 2>&1 | %check

struct Unpacked {
  x: i32,
}

struct T {
    v2f: vec2<f32>,
    v3f: vec3<f32>,
    v4f: vec4<f32>,
    v2u: vec2<u32>,
    v3u: vec3<u32>,
    v4u: vec4<u32>,
    f: f32,
    u: u32,
    unpacked: Unpacked,
}

struct U {
    // CHECK: array<type\d::PackedType, 1> field0
    ts: array<T>,
}

var<private> t: T;
var<private> m2: mat2x2<f32>;
var<private> m3: mat3x3<f32>;

@group(0) @binding(0) var<storage, read_write> t1: T;
@group(0) @binding(1) var<storage, read_write> t2: T;

@group(0) @binding(2) var<storage, read_write> v1: vec3<f32>;
@group(0) @binding(3) var<storage, read_write> v2: vec3<f32>;

@group(0) @binding(4) var<storage, read_write> at1: array<T, 2>;
@group(0) @binding(5) var<storage, read_write> at2: array<T, 2>;

@group(0) @binding(6) var<storage, read_write> av1: array<vec3f, 2>;
@group(0) @binding(7) var<storage, read_write> av2: array<vec3f, 2>;

@group(0) @binding(8) var<storage, read_write> u1: U;

fn testUnpacked() -> i32
{
    { let x = t.v3f * m3; }
    { let x = m3 * t.v3f; }

    { let x = t1.v2f * m2; }
    { let x = m2 * t1.v2f; }

    { let x = u1.ts[0].v3f * m3; }
    { let x = m3 * u1.ts[0].v3f; }

    { let x = av1[0] * m3; }
    { let x = m3 * av1[0]; }

    return 0;
}

fn testAssignment() -> i32
{
    // packed struct
    // CHECK: local\d+ = __unpack\(global\d+\);
    var t = t1;
    // CHECK: local\d+ = __unpack\(global\d+\);
    t = t1;
    // CHECK-NEXT: global\d+ = global\d+;
    t1 = t2;
    // CHECK-NEXT: global\d+ = __pack\(local\d+\);
    t2 = t;

    // array of packed structs
    // CHECK-NEXT: local\d+ = __unpack\(global\d+\);
    var at = at1;
    // CHECK-NEXT: local\d+ = __unpack\(global\d+\);
    at = at1;
    // CHECK-NEXT: global\d+ = global\d+;
    at1 = at2;
    // CHECK-NEXT: global\d+ = __pack\(local\d+\);
    at2 = at;

    // array of vec3
    // CHECK-NEXT: local\d+ = global\d+;
    var av = av1;
    // CHECK-NEXT: local\d+ = global\d+;
    av = av1;
    // CHECK-NEXT: global\d+ = global\d+;
    av1 = av2;
    // CHECK-NEXT: global\d+ = local\d+;
    av2 = av;

    return 0;
}

fn testFieldAccess() -> i32
{
    // CHECK: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
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

    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
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

    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d\.x = global\d+\.field\d\.x;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d;
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d
    // CHECK-NEXT: global\d+\.field\d = global\d+\.field\d
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

@group(0) @binding(9) var<storage, read_write> index: u32;
fn testIndexAccess() -> i32
{
    // CHECK: local\d+ = __unpack\(global\d+\);
    // CHECK-NEXT: local\d+\[0\] = __unpack\(global\d+\[0\]\);
    // CHECK-NEXT: global\d+\[0\] = global\d+\[0\];
    // CHECK-NEXT: global\d+\[0\] = __pack\(local\d+\[0\]\);
    // CHECK-NEXT: global\d+\[global\d+\] = __pack\(local\d+\[global\d+\]\);
    var at = at1;
    at[0] = at1[0];
    at1[0] = at2[0];
    at2[0] = at[0];
    at2[index] = at[index];
    return 0;
}


fn testBinaryOperations() -> i32
{
    // CHECK: global\d+\.field\d\.x = \(2. \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2. \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2. \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2u \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2u \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2u \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d = \(2. \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2. \* float3\(global\d+\.field\d\)\);
    // CHECK-NEXT: global\d+\.field\d = \(2. \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2u \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2u \* uint3\(global\d+\.field\d\)\);
    // CHECK-NEXT: global\d+\.field\d = \(2u \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2. \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2u \* global\d+\.field\d\);
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

    // CHECK-NEXT: global\d+\.field\d\.x = \(2. \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2. \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2. \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2u \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2u \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2u \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d = \(2. \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2. \* float3\(global\d+\.field\d\)\);
    // CHECK-NEXT: global\d+\.field\d = \(2. \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2u \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2u \* uint3\(global\d+\.field\d\)\);
    // CHECK-NEXT: global\d+\.field\d = \(2u \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2. \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2u \* global\d+\.field\d\);
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

    // CHECK-NEXT: global\d+\.field\d\.x = \(2. \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2. \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2. \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2u \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2u \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(2u \* global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d = \(2. \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2. \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2. \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2u \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2u \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2u \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2. \* global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(2u \* global\d+\.field\d\);
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
    // CHECK: global\d+\.field\d\.x = \(-global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(-global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(-global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d = \(-global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(-float3\(global\d+\.field\d\)\);
    // CHECK-NEXT: global\d+\.field\d = \(-global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(-global\d+\.field\d\);
    t.v2f.x = -t1.v2f.x;
    t.v3f.x = -t1.v3f.x;
    t.v4f.x = -t1.v4f.x;
    t.v2f   = -t1.v2f;
    t.v3f   = -t1.v3f;
    t.v4f   = -t1.v4f;
    t.f     = -t1.f;

    // CHECK-NEXT: global\d+\.field\d\.x = \(-global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(-global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(-global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d = \(-global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(-float3\(global\d+\.field\d\)\);
    // CHECK-NEXT: global\d+\.field\d = \(-global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(-global\d+\.field\d\);
    t1.v2f.x = -t2.v2f.x;
    t1.v3f.x = -t2.v3f.x;
    t1.v4f.x = -t2.v4f.x;
    t1.v2f   = -t2.v2f;
    t1.v3f   = -t2.v3f;
    t1.v4f   = -t2.v4f;
    t1.f     = -t2.f;

    // CHECK-NEXT: global\d+\.field\d\.x = \(-global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(-global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = \(-global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d = \(-global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(-global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(-global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = \(-global\d+\.field\d\);
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
    // CHECK: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(float3\(global\d+\.field\d\)\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(uint3\(global\d+\.field\d\)\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
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

    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(float3\(global\d+\.field\d\)\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(uint3\(global\d+\.field\d\)\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
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

    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d\.x = abs\(global\d+\.field\d\.x\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
    // CHECK-NEXT: global\d+\.field\d = abs\(global\d+\.field\d\);
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

struct S {
    @size(16) x: i32,
    @align(32) y: array<i32>,
}
@group(0) @binding(10) var<storage, read_write> s: S;

fn testRuntimeArray()
{
    s.x = 0;
    s.y[0] = 0;
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
    testRuntimeArray();
}
