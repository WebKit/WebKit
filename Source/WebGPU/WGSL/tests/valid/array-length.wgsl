// RUN: %metal-compile main

struct S {
  x: array<f32>,
}

struct Packed {
  x: vec3<f32>
}


struct T {
  x: array<Packed>,
}

@group(0) @binding(0) var<storage, read_write> x: array<f32>;
@group(0) @binding(1) var<storage, read_write> y: S;
@group(0) @binding(2) var<storage, read_write> z: T;

fn f() -> u32 {
    let x1 = arrayLength(&x);
    let y1 = arrayLength(&y.x);
    let z1 = arrayLength(&z.x);

    let xptr = &x;
    let yptr = &y.x;
    let zptr = &z.x;

    let x2 = arrayLength(xptr);
    let y2 = arrayLength(yptr);
    let z2 = arrayLength(zptr);

    return x1 + y1 + z1 + x2 + y2 + z2;
}

@compute @workgroup_size(1, 1, 1)
fn main() {
    let x = f();
}
