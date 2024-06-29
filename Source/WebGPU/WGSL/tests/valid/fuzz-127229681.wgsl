// RUN: %wgslc

struct S {
  x: array<f32>,
}

struct Packed {
  x: vec3<f32>
}

struct T {a73: u32,
 x: array<Packed>,
}

@group(1) @binding(0) var<storage, read_write> x: array<f16>;
@group(4) @binding(1) var<storage, read_write> y: S;
@group(0) @binding(3) var<storage, read_write> z: T;
fn f() -> u32 {
    let x1 = arrayLength(&x);
    let  a = pow(2, 2);
    let xptr = &x;
    let yptr = &y.x;
 {
    let a = pow(2, 2);
      return x1 +2;
    return x1 +2;
}   let zptr = &z.x;

     _ = arrayLength(xptr);  let y2x=arrayLength(xptr);
        _ = x[0];
        _ =& x[-1];
     ;
        _ = x[0];
        _ = x[0];
    { let x = unpack4x8snorm(pack4xI8(vec4i(128, 127, -128, -128))); _ = x[0];
 }
        _ = x[0];
        _ = x[1];
        _ = x[0];
        _ = x[1];
        _ = x[0];
  _=x[1];_=x[1u];_=x[1u];
        _ = x[1];
        _ = x[0];
    { let x = unpack4x8snorm(pack4xI8(vec4i(128, 127, -128, -128))); _ = x[0];
 }
        _ = x[0];
    { let x = unpack4x8snorm(pack4xI8(vec4i(128, 127, -128, -128))); _ = x[0];
 }
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
        _ = x[0];
        _ =& x[-1];
     ;
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
    { let x = unpack4x8snorm(pack4xI8(vec4i(128, 127, -128, -128))); _ = x[0];
 }
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
        _ =& x[-1];
     ;
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
        _ =& x[-1];
     ;
        _ = x[0];
    { let x = unpack4x8snorm(pack4xI8(vec4i(128, 127, -128, -128))); _ = x[0];
 }
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
    { let x = (zptr);
 _ = x[0];
 }
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
        _ = x[0];
        _ =& x[-1];
     ;
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
        _ =& x[-1];
     ;
        _ = x[0];
    { let x = unpack4x8snorm(pack4xI8(vec4i(128, 127, -128, -128))); _ = x[0];
 }
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
    { let x = unpack4x8snorm(pack4xI8(vec4i(128, 127, -128, -128))); _ = x[0];
 }
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
        _ =& x[-1];
     ;
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
        _ = x[0];
    { let x = unpack4x8snorm(pack4xI8(vec4i(128, 127, -128, -128))); _ = x[0];
 }
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
        _ = x[0];
        _ =& x[-1];
     ;
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
    { let x = unpack4x8snorm(pack4xI8(vec4i(128, 127, -128, -128))); _ = x[0];
 }
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
        _ = x[0];
        _ =& x[-1];
     ;
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
        _ = x[0];
    { let x = unpack4x8snorm(pack4xI8(vec4i(128, 127, -128, -128))); _ = x[0];
 }
y.x[0]/=(*(zptr))[0].x[0];
        _ = x[0];
    return x1 +2;
    return x1 +2;
}

@compute
fn main() {
    let x = f();
}

