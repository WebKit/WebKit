// RUN: %wgslc

struct S {
    x: f32,
    y: i32,
}

fn testStructConstructor()
{
    _ = S(0, 0);
    _ = S(0.0, 0);
    _ = S(0f, 0i);
}
