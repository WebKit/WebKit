// RUN: %metal-compile main

// 8.5. Composite Value Decomposition Expressions (https://www.w3.org/TR/WGSL/#composite-value-decomposition-expr)
@compute @workgroup_size(1, 1, 1)
fn main() {
    testVectorAccessExpression();
    testMatrixAccessExpression();
    testArrayAccessExpression();
    testStructAccessExpression();
}

// 8.5.1. Vector Access Expression (https://www.w3.org/TR/WGSL/#vector-access-expr)
fn testVectorAccessExpression()
{
    testVectorSingleComponentSelection();
    testVectorMultipleComponentSelection();
    testComponentReferenceFromVectorReference();
}

// 8.5.1.1. Vector Single Component Selection (https://www.w3.org/TR/WGSL/#vector-single-component)
fn testVectorSingleComponentSelection()
{
    let v2 = vec2<f32>(0);
    let v3 = vec3<i32>(0);
    let v4 = vec4<u32>(0);

    _ = v2.x;
    _ = v3.x;
    _ = v4.x;
    _ = v2.r;
    _ = v3.r;
    _ = v4.r;

    _ = v2.y;
    _ = v3.y;
    _ = v4.y;
    _ = v2.g;
    _ = v3.g;
    _ = v4.g;

    _ = v3.z;
    _ = v4.z;
    _ = v3.b;
    _ = v4.b;

    _ = v4.w;
    _ = v4.a;

    _ = v2[0];
    _ = v3[0];
    _ = v4[0];

    _ = v2[1];
    _ = v3[1];
    _ = v4[1];

    _ = v3[2];
    _ = v4[2];

    _ = v4[3];
}

// 8.5.1.2. Vector Multiple Component Selection (https://www.w3.org/TR/WGSL/#vector-multi-component)
fn testVectorMultipleComponentSelection()
{
    let v2 = vec2<f32>(0);
    let v3 = vec3<i32>(0);
    let v4 = vec4<u32>(0);

    _ = v2.xy;
    _ = v3.yz;
    _ = v4.zw;
    _ = v2.rg;
    _ = v3.gb;
    _ = v4.ba;

    _ = v2.xxy;
    _ = v3.xyz;
    _ = v4.yzw;
    _ = v2.rrg;
    _ = v3.rgb;
    _ = v4.gba;

    _ = v2.xxyy;
    _ = v3.xxyz;
    _ = v4.xyzw;
    _ = v2.rrgg;
    _ = v3.rrgb;
    _ = v4.rgba;
}

// 8.5.1.3. Component Reference from Vector Reference (https://www.w3.org/TR/WGSL/#vector-multi-component)
fn testComponentReferenceFromVectorReference()
{
    var v2 = vec2<f32>(0);
    var v3 = vec3<i32>(0);
    var v4 = vec4<u32>(0);

    v2.x = 0;
    v3.x = 0;
    v4.x = 0;
    v2.r = 0;
    v3.r = 0;
    v4.r = 0;

    v2.y = 0;
    v3.y = 0;
    v4.y = 0;
    v2.g = 0;
    v3.g = 0;
    v4.g = 0;

    v3.z = 0;
    v4.z = 0;
    v3.b = 0;
    v4.b = 0;

    v4.w = 0;
    v4.a = 0;

    v2[0] = 0;
    v3[0] = 0;
    v4[0] = 0;

    v2[1] = 0;
    v3[1] = 0;
    v4[1] = 0;

    v3[2] = 0;
    v4[2] = 0;

    v4[3] = 0;
}

// 8.5.2. Matrix Access Expression (https://www.w3.org/TR/WGSL/#matrix-access-expr)
fn testMatrixAccessExpression()
{
    var m2x2 = mat2x2(0, 0, 0, 0);
    var m2x3 = mat2x3(0, 0, 0, 0, 0, 0);
    var m2x4 = mat2x4(0, 0, 0, 0, 0, 0, 0, 0);
    var m3x2 = mat3x2(0, 0, 0, 0, 0, 0);
    var m3x3 = mat3x3(0, 0, 0, 0, 0, 0, 0, 0, 0);
    var m3x4 = mat3x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    var m4x2 = mat4x2(0, 0, 0, 0, 0, 0, 0, 0);
    var m4x3 = mat4x3(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    var m4x4 = mat4x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    { let x: vec2<f32> = m2x2[0]; }
    { let x: vec2<f32> = m3x2[0]; }
    { let x: vec2<f32> = m4x2[0]; }
    { let x: vec2<f32> = m2x2[1]; }
    { let x: vec2<f32> = m3x2[1]; }
    { let x: vec2<f32> = m4x2[1]; }
    { let x: vec2<f32> = m3x2[2]; }
    { let x: vec2<f32> = m4x2[2]; }
    { let x: vec2<f32> = m4x2[3]; }

    { let x: vec3<f32> = m2x3[0]; }
    { let x: vec3<f32> = m3x3[0]; }
    { let x: vec3<f32> = m4x3[0]; }
    { let x: vec3<f32> = m2x3[1]; }
    { let x: vec3<f32> = m3x3[1]; }
    { let x: vec3<f32> = m4x3[1]; }
    { let x: vec3<f32> = m3x3[2]; }
    { let x: vec3<f32> = m4x3[2]; }
    { let x: vec3<f32> = m4x3[3]; }

    { let x: vec4<f32> = m2x4[0]; }
    { let x: vec4<f32> = m3x4[0]; }
    { let x: vec4<f32> = m4x4[0]; }
    { let x: vec4<f32> = m2x4[1]; }
    { let x: vec4<f32> = m3x4[1]; }
    { let x: vec4<f32> = m4x4[1]; }
    { let x: vec4<f32> = m3x4[2]; }
    { let x: vec4<f32> = m4x4[2]; }
    { let x: vec4<f32> = m4x4[3]; }

    // Reference test
    m2x2[0] = vec2<f32>(0);
    m3x2[0] = vec2<f32>(0);
    m4x2[0] = vec2<f32>(0);
    m2x2[1] = vec2<f32>(0);
    m3x2[1] = vec2<f32>(0);
    m4x2[1] = vec2<f32>(0);
    m3x2[2] = vec2<f32>(0);
    m4x2[2] = vec2<f32>(0);
    m4x2[3] = vec2<f32>(0);

    m2x3[0] = vec3<f32>(0);
    m3x3[0] = vec3<f32>(0);
    m4x3[0] = vec3<f32>(0);
    m2x3[1] = vec3<f32>(0);
    m3x3[1] = vec3<f32>(0);
    m4x3[1] = vec3<f32>(0);
    m3x3[2] = vec3<f32>(0);
    m4x3[2] = vec3<f32>(0);
    m4x3[3] = vec3<f32>(0);

    m2x4[0] = vec4<f32>(0);
    m3x4[0] = vec4<f32>(0);
    m4x4[0] = vec4<f32>(0);
    m2x4[1] = vec4<f32>(0);
    m3x4[1] = vec4<f32>(0);
    m4x4[1] = vec4<f32>(0);
    m3x4[2] = vec4<f32>(0);
    m4x4[2] = vec4<f32>(0);
    m4x4[3] = vec4<f32>(0);
}

// 8.5.3. Array Access Expression (https://www.w3.org/TR/WGSL/#array-access-expr)
@group(0) @binding(0) var<storage, read_write> ga: array<i32>;
fn testArrayAccessExpression()
{
    var a: array<i32, 1> = array(0);

    { let x: i32 = a[0]; };
    { let x: i32 = ga[0]; };

    a[0] = 0;
    ga[0] = 0;
}

// 8.5.4. Structure Access Expression (https://www.w3.org/TR/WGSL/#struct-access-expr)
struct S { x: i32 };
fn testStructAccessExpression()
{
    var s: S;
    let x: i32 = s.x;
    s.x = 0;
}
