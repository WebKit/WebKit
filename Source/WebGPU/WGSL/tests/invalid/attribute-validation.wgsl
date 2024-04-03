// RUN: %not %wgslc | %check

struct S {
    // CHECK-L: @size value must be at least the byte-size of the type of the member
    @size(2) x: i32,

    // CHECK-L: @size value must be non-negative
    @size(-1) y: i32,

    // CHECK-L: @align value must be non-negative
    @align(-1) z: i32,

    // CHECK-L: @align value must be a power of two
    @align(3) w: i32,
}

// CHECK-L: @group attribute must only be applied to resource variables
// CHECK-L: @group value must be non-negative
// CHECK-L: @binding attribute must only be applied to resource variables
// CHECK-L: @binding value must be non-negative
@group(-1) @binding(-1) var<private> x: i32;

// CHECK-L: @id attribute must only be applied to override variables of scalar type
// CHECK-L: @id value must be non-negative
@id(-1) var<private> y: i32;

// CHECK-L: @id attribute must only be applied to override variables of scalar type
@id(0) override z: array<i32, 1>;

// CHECK-L: @must_use can only be applied to functions that return a value
@must_use
fn mustUseWithoutReturnType() { }

// CHECK-L: @builtin is not valid for non-entry point function types
fn f1() -> @builtin(position) i32 { return 0; }

// CHECK-L: @location is not valid for non-entry point function types
fn f2() -> @location(0) i32 { return 0; }

// CHECK-L: @location value must be non-negative
@fragment
fn f3() -> @location(-1) i32 { return 0; }

// CHECK-L: @location may not be used in the compute shader stage
@compute
fn f4() -> @location(0) i32 { return 0; }

// CHECK-L: @location must only be applied to declarations of numeric scalar or numeric vector type
@fragment
fn f5() -> @location(0) bool { return false; }

// CHECK-L: @interpolate is only allowed on declarations that have a @location attribute
fn f6() -> @interpolate(flat) i32 { return 0; }

// CHECK-L: @invariant is only allowed on declarations that have a @builtin(position) attribute
fn f7() -> @invariant i32 { return 0; }

// CHECK-L: @workgroup_size must only be applied to compute shader entry point function
@workgroup_size(1)
fn f8() { }

// CHECK-L: @workgroup_size argument must be at least 1
@workgroup_size(-1) @compute
fn f9() { }

struct S1 {
  @size(16) x : f32
};

struct S2 {
  // check that we don't crash by trying to read the size of S2, which won't have been computed
  @size(32) x: array<S1, 2>,
};
