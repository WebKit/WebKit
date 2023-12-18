// RUN: %metal-compile main

@group(0) @binding(0) var<storage> a1: array<vec4f>;
@group(0) @binding(1) var te: texture_external;
@group(0) @binding(2) var<storage> a2: array<vec4f>;

@compute @workgroup_size(1)
fn main()
{
    _ = arrayLength(&a1);
    _ = te;
    _ = arrayLength(&a2);
}
