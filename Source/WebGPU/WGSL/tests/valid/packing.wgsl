// RUN: %metal-compile main

struct T {
    x: vec3<f32>,
    y: i32,
}

var<private> t: T;
var<private> m: mat3x3<f32>;

@compute @workgroup_size(1)
fn main()
{
    _ = testUnpacked();
}

fn testUnpacked() -> i32
{
    _ = t.x * m;
    _ = m * t.x;
    return 0;
}
