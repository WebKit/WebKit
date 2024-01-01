// RUN: %metal-compile main 2>&1| %check

@compute @workgroup_size(1)
fn main()
{
    // CHECK: 1., 0.50\d*, -0.49\d*, -1.
    { let x = unpack4x8snorm(pack4x8snorm(vec4f(1.0, 0.5, -0.5, -1))); }

    // CHECK: 1., 0.50\d*, 0.25\d*, 0.
    { let x = unpack4x8unorm(pack4x8unorm(vec4f(1.0, 0.5, 0.25, 0))); }

    // CHECK: -128, 127, -128, 127
    { let x = unpack4xI8(pack4xI8(vec4i(128, 127, -128, -129))); }

    // CHECK: 0u, 255u, 128u, 64u
    { let x = unpack4xU8(pack4xU8(vec4u(256, 255, 128, 64))); }

    // CHECK: 127, 127, -128, -128
    { let x = unpack4xI8(pack4xI8Clamp(vec4i(128, 127, -128, -129))); }

    // CHECK: 255u, 255u, 128u, 64u
    { let x = unpack4xU8(pack4xU8Clamp(vec4u(256, 255, 128, 64))); }

    // CHECK: 0.50\d*, -0.49\d*
    { let x = unpack2x16snorm(pack2x16snorm(vec2f(0.5, -0.5))); }

    // CHECK: 0.50\d*, 0.
    { let x = unpack2x16unorm(pack2x16unorm(vec2f(0.5, -0.5))); }

    // CHECK: 0.5, -0.5
    { let x = unpack2x16float(pack2x16float(vec2f(0.5, -0.5))); }
}
