// RUN: %metal-compile main

struct S {
    x: array<vec3<f32>, 1>,
    y: vec3f,
}


@group(0) @binding(0) var<storage, read> s : S;

@compute @workgroup_size(1)
fn main()
{
  _ = s.x;
}
