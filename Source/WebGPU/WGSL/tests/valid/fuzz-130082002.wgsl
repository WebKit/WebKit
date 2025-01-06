// RUN: %metal-compile main

@group(0) @binding(0) var<storage, read_write> b: vec3f;

@compute @workgroup_size(1)
fn main() {
  b += vec3();
}
