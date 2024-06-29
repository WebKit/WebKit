// RUN: %metal-compile main

@group(0) @binding(1) var<storage, read> b : array<i32>;
@group(0) @binding(0) var<storage, read_write> a : i32;

@compute @workgroup_size(1)
fn main()
{
    _ = a;
    _ = arrayLength(&b);
}
