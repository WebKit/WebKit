// RUN: %metal-compile main

fn f1(x: vec2f) {}
fn f2(x: vec2i) {}
fn f3(x: vec2u) {}
fn f4(x: vec3f) {}
fn f5(x: vec3i) {}
fn f6(x: vec3u) {}
fn f7(x: vec4f) {}
fn f8(x: vec4i) {}
fn f9(x: vec4u) {}

fn m1(x: mat2x2f) {}
fn m2(x: mat2x3f) {}
fn m3(x: mat2x4f) {}
fn m4(x: mat3x2f) {}
fn m5(x: mat3x3f) {}
fn m6(x: mat3x4f) {}
fn m7(x: mat4x2f) {}
fn m8(x: mat4x3f) {}
fn m9(x: mat4x4f) {}

struct S {
  x: i32,
}
alias v = vec2<i32>;
alias s = S;
alias f = f32;
alias i = i32;
alias u = u32;
alias b = bool;

@compute @workgroup_size(1)
fn main() {

    let z = 1;
    _ = f(0);
    _ = i(0);
    _ = u(0);
    _ = b(0);
    _ = f(z);
    _ = i(z);
    _ = u(z);
    _ = b(z);

    let x = v(0);
    let y = s(0);

    _ = f1(vec2f(vec2(0u)));
    _ = f2(vec2i(vec2(0f)));
    _ = f3(vec2u(vec2(0f)));
    _ = f4(vec3f(vec3(0u)));
    _ = f5(vec3i(vec3(0f)));
    _ = f6(vec3u(vec3(0f)));
    _ = f7(vec4f(vec4(0u)));
    _ = f8(vec4i(vec4(0f)));
    _ = f9(vec4u(vec4(0f)));

    _ = m1(mat2x2f(mat2x2(0, 0, 0, 0)));
    _ = m2(mat2x3f(mat2x3(0, 0, 0, 0, 0, 0)));
    _ = m3(mat2x4f(mat2x4(0, 0, 0, 0, 0, 0, 0, 0)));
    _ = m4(mat3x2f(mat3x2(0, 0, 0, 0, 0, 0)));
    _ = m5(mat3x3f(mat3x3(0, 0, 0, 0, 0, 0, 0, 0, 0)));
    _ = m6(mat3x4f(mat3x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)));
    _ = m7(mat4x2f(mat4x2(0, 0, 0, 0, 0, 0, 0, 0)));
    _ = m8(mat4x3f(mat4x3(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)));
    _ = m9(mat4x4f(mat4x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)));
}
