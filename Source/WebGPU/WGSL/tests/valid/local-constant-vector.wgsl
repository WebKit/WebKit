// RUN: %metal main 2>&1 | %check

fn f(x: i32) { }
@compute @workgroup_size(1)
fn main() {
  const a = vec2(0);

  // CHECK: \(void\)\(vec<int, 2>\(0, 0\)\)
  _ = a;
}
