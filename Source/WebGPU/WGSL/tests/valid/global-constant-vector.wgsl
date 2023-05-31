// RUN: %metal main 2>&1 | %check
const a = vec2(0);
@compute @workgroup_size(1)
fn main() {
  // CHECK: vec.* local\d+ = vec.*
  // CHECK: \(void\)\(local\d+\)
  _ = a;
}
