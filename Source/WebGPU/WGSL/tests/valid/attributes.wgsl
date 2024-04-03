// RUN: %wgslc

struct S {
    @align(4) x: i32,
    @location(0) y: i32,
    @size(4) z: i32,
}

@group(0) @binding(0) var<storage> x : i32;

@group(0) @binding(0) var<storage> y : i32;

// FIXME: we don't yet parse @id
// @id(0) override z : i32;

@compute @workgroup_size(1)
fn f1() { }

@compute @workgroup_size(1, 1)
fn f2() { }

@compute @workgroup_size(1, 1, 1)
fn f3() { }

@compute @workgroup_size(1i, 1i, 1i)
fn f4() { }

@compute @workgroup_size(1, 1, 1i)
fn f5() { }

@compute @workgroup_size(1, 1, 1u)
fn f6() { }

@compute @workgroup_size(1, 1i, 1)
fn f7() { }

@compute @workgroup_size(1, 1u, 1)
fn f8() { }
