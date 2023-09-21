// RUN: %metal main | %check

@compute @workgroup_size(1)
fn main() {
  // CHECK-NOT: pow\(.*\)
  // CHECK-NOT: float .* = \d
  const a = pow(2, 2);
  // CHECK-L: (void)(4.)
  _ = a;
}

