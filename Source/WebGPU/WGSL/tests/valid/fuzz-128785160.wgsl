// RUN: %metal-compile main

struct S {
  @size(1<<30) f0: u32,
}

@group(0) @binding(0)
var<storage, read_write> k: S;

@compute @workgroup_size(1)
fn main() {
  k.f0 = 0;
}
