// RUN: %metal-compile main

@binding(0) @group(0) var<uniform> x : vec4<f32>;
@binding(0) @group(0) var<uniform> y : vec4<f32>;

@compute @workgroup_size(1)
fn main() {
    _ = x;
}
