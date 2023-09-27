// RUN: %not %wgslc | %check

// CHECK-L: 'atomic' requires 1 template argument
@group(0) @binding(0) var<storage, read_write> x : atomic<i32, i32>;

// CHECK-L: atomic only supports i32 or u32 types
@group(0) @binding(1) var<storage, read_write> y : atomic<f32>;
