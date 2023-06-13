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
