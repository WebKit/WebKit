// RUN: %not %wgslc | %check

struct S {
    x: f32,
    x: T,
    // CHECK-L: duplicate member 'x' in struct 'S'
    y: i32,
}
