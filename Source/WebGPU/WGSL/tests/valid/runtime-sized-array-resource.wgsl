// RUN: %metal-compile main

@group(0) @binding(0) var<storage, read_write> x: array<i32>;

@compute @workgroup_size(1)
fn main()
{
    x[0] = x[42];

    // pointer access
    _ = (&x)[0];
}
