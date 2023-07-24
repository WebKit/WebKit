// RUN: %metal main 2>&1 | %check

fn f(x: f32) { }

@compute @workgroup_size(1)
fn main() {
  // CHECK: float local\d+ = 1
  var a = 1.0;
  // CHECK: float local\d+ = local\d
  var b = a;
  // CHECK: local\d+ = 0
  b = 0.0;
  // CHECK: function\d\(local\d+\)
  _ = f(b);
}
