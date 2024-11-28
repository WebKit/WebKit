// RUN: %metal-compile main
// RUN: %metal main | %check

@fragment
fn main() {
  var h = vec2f();
  // CHECK-L: length(local0)
  _ = length(*&h);
}
