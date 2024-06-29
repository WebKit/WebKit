// RUN: %not %wgslc | %check

@compute @workgroup_size(1)
fn main() {
    switch 1 {
    default: {
      // CHECK-L: continue statement must be in a loop
      continue;
    }
    }
}
