// RUN: %metal-compile main
// RUN: %metal main 2>&1 | %check

struct T {
    x: vec3<f32>,
    y: i32,
}

var<private> t: T;
var<private> m: mat3x3<f32>;

@group(0) @binding(0) var<storage, read_write> t1: T;
@group(0) @binding(1) var<storage, read_write> t2: T;

fn testUnpacked() -> i32
{
    _ = t.x * m;
    _ = m * t.x;
    return 0;
}

fn testAssignment() -> i32 {
    // packed struct
    // CHECK: local\d+ = __unpack\(parameter\d+\);
    var t = t1;
    // CHECK-NEXT: parameter\d+ = parameter\d+;
    t1 = t2;
    // CHECK-NEXT: parameter\d+ = __pack\(local\d+\);
    t2 = t;

    return 0;
}

@compute @workgroup_size(1)
fn main()
{
    _ = testUnpacked();
    _ = testAssignment();
}
