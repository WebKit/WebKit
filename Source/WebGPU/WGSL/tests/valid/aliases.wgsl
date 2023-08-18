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

@compute @workgroup_size(1)
fn main() {
    _ = f1(vec2f(vec2(0u)));
    _ = f2(vec2i(vec2(0f)));
    _ = f3(vec2u(vec2(0f)));
    _ = f4(vec3f(vec3(0u)));
    _ = f5(vec3i(vec3(0f)));
    _ = f6(vec3u(vec3(0f)));
    _ = f7(vec4f(vec4(0u)));
    _ = f8(vec4i(vec4(0f)));
    _ = f9(vec4u(vec4(0f)));
}
