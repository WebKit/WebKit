// RUN: %metal main 2>&1 | %check
const a = vec2u(0);
const b = vec2f(a);
@compute @workgroup_size(1)
fn main() {
  // CHECK: vec.* local\d+ = float2
  // CHECK: \(void\)\(local\d+\[0\]\)
  let x = vec4f(b, 0, 0);
  _ = x[0];
}
