// RUN: %not %wgslc | %check

struct S {
    // CHECK-L: @align must be an i32 or u32 value
    @align(4.0) x: i32,

    // CHECK-L: @location must be an i32 or u32 value
    @location(0.0) y: i32,

    // CHECK-L: @size must be an i32 or u32 value
    @size(4.0) z: i32,
}

// CHECK-L: @group must be an i32 or u32 value
@group(0.0) @binding(0) var x : i32;

// CHECK-L: @binding must be an i32 or u32 value
@group(0) @binding(0.0) var y : i32;

// FIXME: Type checking is implemented, but we don't yet parse @id
// FIXME: CHECK-L: @binding must be an i32 or u32 value
// @id(0.0) override z : i32;

// CHECK-L: @workgroup_size x dimension must be an i32 or u32 value
@compute @workgroup_size(1.0)
fn f1() { }

// CHECK-L: @workgroup_size y dimension must be an i32 or u32 value
@compute @workgroup_size(1, 1.0)
fn f2() { }

// CHECK-L: @workgroup_size z dimension must be an i32 or u32 value
@compute @workgroup_size(1, 1, 1.0)
fn f3() { }

// CHECK-L: workgroup_size arguments must be of the same type, either i32 or u32
@compute @workgroup_size(1i, 1u)
fn f4() { }

// CHECK-L: workgroup_size arguments must be of the same type, either i32 or u32
@compute @workgroup_size(1, 1i, 1u)
fn f5() { }

// CHECK-L: workgroup_size arguments must be of the same type, either i32 or u32
@compute @workgroup_size(1, 1u, 1i)
fn f6() { }
