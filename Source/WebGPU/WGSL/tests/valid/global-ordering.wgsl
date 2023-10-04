// RUN: %metal-compile main

@group(0) @binding(1) var<storage> y: i32;
@group(0) @binding(0) var<storage> x: i32;

@compute @workgroup_size(1)
fn main()
{
    _ = y;
    _ = x;
}
