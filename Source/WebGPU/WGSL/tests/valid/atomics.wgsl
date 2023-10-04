// RUN: %metal-compile main

@group(1) @binding(0) var<storage, read_write> x : atomic<i32>;

@compute @workgroup_size(1)
fn main() {
  let y : ptr<storage, atomic<i32>, read_write> = &x;
}
