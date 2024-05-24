// RUN: %metal-compile main

struct S {
    @size(16) x: f32,
    @align(16) y: i32,
}

fn testStructConstructor() -> i32
{
    _ = S(0, 0);
    _ = S(0.0, 0);
    _ = S(0f, 0i);
    return 0;
}

@compute @workgroup_size(1)
fn main()
{
    _ = testStructConstructor();
}

struct S1 {
@location(0) x: i32,
}

struct S2 {
@location(1) x: i32,
}

@fragment
fn fragment_main(s1: S1, s2: S2) {
}
