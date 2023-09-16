// RUN: %not %wgslc | %check

@compute @workgroup_size(1)
fn main() {
  _ = f();
}

// CHECK-L: encountered a dependency cycle: f -> f
fn f() -> i32 {
  _ = f();
  return g();
}
