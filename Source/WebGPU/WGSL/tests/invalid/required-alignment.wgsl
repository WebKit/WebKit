// RUN: %not %wgslc | %check

struct S1 {
    // CHECK-L: arrays in the uniform address space must have a stride multiple of 16 bytes, but has a stride of 8 bytes
    x: array<vec2<i32>, 2>,

    x1: i32,
    // CHECK-L: offset of struct member S1::y1 must be a multiple of 16 bytes, but its offset is 20 bytes
    @align(4) y1: array<vec4<i32>, 2>,

    @align(16) x2: i32,
    // CHECK-L: offset of struct member S1::y2 must be a multiple of 32 bytes, but its offset is 80 bytes
    @align(16) y2: U,

    x3: i32,
    y3: T,
    // CHECK-L: uniform address space requires that the number of bytes between S1::y3 and S1::z3 must be at least 16 bytes, but it is 8 bytes
    z3: i32,
}

struct T {
    x: i32,
    y: i32,
};

struct U {
    x: i32,
    @align(32) y: i32,
};

@group(0) @binding(1) var<uniform> x1: S1;
