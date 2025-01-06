// RUN: %metal-compile main

@compute @workgroup_size(1)
fn main() {
  const_assert 1 < 2;
}
