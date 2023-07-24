// RUN: %metal main 2>&1 | %check

fn f(x: i32) { }
@compute @workgroup_size(1)
fn main() {
  // CHECK: vec.* local\d+ = vec.*
  const a = vec2(0);

  // CHECK: \(void\)\(local\d+\)
  _ = a;
}
