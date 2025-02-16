// RUN: %metal-compile main

@group(0) @binding(4294967294u) var<storage, read_write> s: i32;

@compute @workgroup_size(2)
fn main()
{
    _ = s;
}
